#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <opencv2/opencv.hpp>

#include "people_counter.h"
#include "json.h"
#include "testing_harness.h"

#define RESOLUTION (width * height)

using namespace cv;

/*---------------------------------------global variables--------------------------------------*/

/* the width of the image and the height of the image, width * height = resolution */
static int width = 8;
static int height = 8;
/* the lower limit of the temperature mapping and the upper limit */
static int llimit = 20;
static int hlimit = 40;

/* frame rate */
static int frame_rate = 10;

/* the step of fowarding the image pointer */
static int step = 0;

/* pointer to the frame array */
static double **frames_ptr;

/* path to the configuration file */
static const char *config_path = "harness_config.json";

/* image pointer which indicates which image should be processed next */
static unsigned long img_ptr = 0;

/* how many frames are there in this video */
static unsigned long frame_count = 0;

/* background image that will be passed into pipeline */
static uint8_t *background;

/* configuration of the pipeline */
ip_config config = {
	.kernel_1 = 5,
	.threshold = 32,
	.max_area = 18
};

/* number of rectangles detected in the frame */
uint8_t rec_num = 0;

/* rectangles find by the pipeline */
rec hrects[RECTS_MAX_SIZE] = {{0, 0, 0, 0}};

/* Intermedia result 1: image after thresholding */
uint8_t *th_frame;
/*---------------------------------------------------------------------------------------------*/

/*--------------------------------------------function prototypes------------------------------*/

static void parse_json(const char *, void (*parse_value)(json_value *, int));
static void parse_frame(json_value *, int);
static uint8_t grey_map(int, int, double);
static void show_image(uint8_t *, const char *, void (*process_mat)(Mat *));
static void get_background(uint8_t *, unsigned long);
static void read_config(json_value *, int);
static void frame_convert(uint8_t *);
static void draw_rect(Mat *);
static void create_trackbar(const char*, void*);
void rec_areaCallback(int, void*);
void kernel_1Callback(int, void*);
void threshold_Callback(int, void*);
void sensitivity_Callback(int, void*);
void get_LoG_kernel(double, int, int8_t**);
/*---------------------------------------------------------------------------------------------*/

/* hash function that will map string to an int */
constexpr unsigned int str2int(const char *str, int h = 0)
{
	return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
}

void get_LoG_kernel(double sigma, int ksize, int8_t** result)
{
        if (!(ksize % 2)) {
                fprintf(stderr, "Invalid kernel size %d\n, should be an odd number", ksize);
                exit(1);
        }   
        double kernel[ksize][ksize];
        int mean = ksize / 2;
        for (int i = 0; i < ksize; ++i) {
                for (int j = 0; j < ksize; ++j) {
                        double temp  = ( -0.5 * (pow((i - mean)/sigma, 2.0) + pow((j - mean)/sigma, 2.0)));
                        kernel[i][j] = (1 + temp) * exp(temp) / (-M_PI * pow(sigma, 4.0)); 
			/*double temp = ((i - mean) * (i - mean) + (j - mean) * (j - mean)) / (sigma * sigma);
			kernel[i][j] = (temp - 2) * exp(-0.5 * temp);*/
                }   
        }   


        for (int i = 0; i < ksize; ++i) {
                for (int j = 0; j < ksize; ++j) {
                        result[i][j] = (int8_t)(kernel[i][j]* 500);
			// printf("%d " , result[i][j]);
                }   
		// printf("\n");
        }   

}

int main(int argc, char *argv[])
{
	parse_json(config_path, read_config);

	uint8_t *cur_frame = (uint8_t *)malloc(RESOLUTION * sizeof(uint8_t));
	uint8_t *buf_frame = (uint8_t *)malloc(RESOLUTION * sizeof(uint8_t));
	background = (uint8_t *)malloc(RESOLUTION * sizeof(uint8_t));
	ip_mat mat_background = {.data = background};

	/* Check whether the amount of arguments is correct */
	if (argc != 2)
	{
		fprintf(stderr, "Invalid argument(s) please only enter the name of the json file\n");
		exit(1);
	}

	char *file_name = argv[1];

	parse_json(file_name, parse_frame);
	printf("Total frame:  %ld, Pipeline frame rate: %d, video frame rate: %d\n", frame_count, FRAME_RATE, frame_rate);

	/* check if the frame rate is valid, to be valid the frame_rate has to be smaller or equal to FRAME_RATE and to be a power of 2 as well */
	if (FRAME_RATE > frame_rate || (frame_rate % 2))
	{
		fprintf(stderr, "Invalid frame rate setting[%d] in harness_config.txt\n", frame_rate);
		exit(1);
	}
	else
	{
		/* step = the frame rate of the video divided by the frame rate that the pipeline works with */
		step = frame_rate / FRAME_RATE;
	}

	/* Intermedia result 1: image after thresholding */
	th_frame = (uint8_t *)malloc(RESOLUTION * sizeof(uint8_t));
	const char *thermal_window = "Thermal image";
	const char *threshold_window = "Thresholding_image";

	namedWindow(thermal_window, WINDOW_NORMAL);
	namedWindow(threshold_window, WINDOW_NORMAL);
	uint8_t black_img[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT] = {0};
	moveWindow(thermal_window, 0, 0);
	moveWindow(threshold_window, 320, 0);
	show_image(black_img, thermal_window, NULL);
	show_image(black_img, threshold_window, NULL);
	resizeWindow(thermal_window, 320, 240);
	resizeWindow(threshold_window, 320, 240);
	create_trackbar(thermal_window, NULL);

	int8_t **log_kernel;
	log_kernel = (int8_t**)malloc(LOG_KSIZE * sizeof(int8_t*));
	for (int i = 0; i < LOG_KSIZE; ++i) {
		log_kernel[i] = (int8_t *)malloc(LOG_KSIZE * sizeof(int8_t));
	}
	// printf("sigma is %f\n", LOG_SIGMA);
	get_LoG_kernel(LOG_SIGMA, LOG_KSIZE, log_kernel);

	int8_t room_count = 0;
	const object *objects = getObjectsAddress();
    
	while (img_ptr < frame_count) {
		/* first convert the raw thermal data into processable and displayable format, namely frame and Mat */
		frame_convert(cur_frame);
		memcpy(buf_frame, cur_frame, RESOLUTION * sizeof(uint8_t));
		/* construct the proper struct required by the pipeline */
		ip_mat mat = {.data = cur_frame};
		/* get the background TODO: should be done by the pipeline */
		get_background(cur_frame, img_ptr);

		ip_result result = IpProcess((void *)&mat, (void *)&mat_background, (void *)log_kernel);

		printf("Object list\n");
		if(result.objects_length > 0)
		{
		printf(" %-4s| %-10s| %-18s\n", "ID", "Position", "Disappeared count");
		}
		for (uint8_t i = 0; i < result.objects_length; ++i)
		{
			printf(" %-4i| (%2i, %2i)  | %-18i\n", objects[i].id,
				objects[i].centroid.x, objects[i].centroid.y, objects[i].disappeared_frames_count);
		}
		
		room_count += result.up - result.down;
		printf("Frame %lu: ", img_ptr);
		if(result.up)
		{
			printf("\033[1;32m");
		}
		printf("%i ", result.up);
		printf("up\033[0m, ");
		if(result.down)
		{
			printf("\033[1;31m");
		}
		printf("%i ", result.down);
		printf("down.\033[0m There are ");
		if(result.up-result.down != 0)
		{
			printf("\033[1;32m");
		}
		printf("%i ", room_count);
		printf("\033[0mpeople in the room.\n\n");
		
		/* the show_image should be called after the IpProcess to correctly display the rectangles 
		 * found by pipeline */
		show_image(buf_frame, thermal_window, draw_rect);
		show_image(th_frame, threshold_window, NULL);
			
		//TODO print info about the returned ip_result
		// if (status == IP_EMPTY) {
		// 	printf("\033[1;31m");	
		// 	printf("Frame[%ld] is empty", img_ptr);
		// }
		// else if (status == IP_STILL)
		// {
		// 	printf("\033[1;33m");
		// 	printf("Frame[%ld] is still", img_ptr);
		// }
		// else
		// {
		// 	printf("\033[1;32m");
		// 	printf("Frame[%ld], Dir: %s, Count: %d", img_ptr, (count.direc == DIRECTION_UP) ? "UP" : "DOWN", count.num);
		// }
		// printf(", [%d] rects detected\n", rec_num);
		// printf("\033[0m");
		
		/* wait for keyboard input to continue to next frame */
		waitKey(0);
		img_ptr += step;
	}
	free(cur_frame);
	free(buf_frame);
	free(background);
	free(th_frame);

	for (unsigned long i = 0; i < frame_count; i++)
	{
		free(frames_ptr[i]);
	}
	free(frames_ptr);
    
	for (int i = 0; i < LOG_KSIZE; i++) {
		free(log_kernel[i]);
	}
	free(log_kernel);

}

/**
 * @brief Create trackbars on the window
 * @param window on which window the call backs are going to be created
 * @param data user data with which global variable could be avoid.
 */
static void create_trackbar(const char *window, void* data) {
	/* create slide bar to adjust the rectangles' max area */
	const char* rec_max = "Rec max area";
	int irec_max = config.max_area;
	createTrackbar(rec_max, window, (int *)&irec_max, 255, rec_areaCallback, data);
	/* slide bar to adjust the kernel 1 */
	const char *kernel_1 = "kernel_1";
	int ikernel_1 = config.kernel_1;
	createTrackbar(kernel_1, window, &ikernel_1, 24, kernel_1Callback, data);
	/* slide bar to adjust the threshold*/
	const char *threshold = "threshold";
	int ithreshold= config.threshold;
	createTrackbar(threshold, window, &ithreshold, 20000, threshold_Callback, data);
	/* slide bar to adjust the sensitivity*/
	const char *sensitivity = "sensitivity";
	int isensitivity= config.sensitivity;
	createTrackbar(sensitivity, window, &isensitivity, 20, sensitivity_Callback, data);
}

void rec_areaCallback(int value, void* data) {
	config.max_area = (uint8_t)value;
}

void kernel_1Callback(int value, void* data) {
	config.kernel_1 = (uint8_t)value;
}

void threshold_Callback(int value, void* data) {
	config.threshold = (int16_t)value;
}


void sensitivity_Callback(int value, void *data) {
	config.sensitivity = (uint8_t)value;
}

/**
 * @brief Parse the json file into a two-demansinal array consits of temperatures retrived
 * 	  by the sensor.
 * @param file_name path to the json file
 * @param parse_value a function pointer that points to the function which parses the json_value
 */
static void parse_json(const char *file_name, void (*parse_value)(json_value *, int))
{
	FILE *json_file;
	int file_size;
	struct stat filestatus;
	char *json;
	json_char *json_chars;
	json_value *value;

	/* Check whether the file exists as well as retrive the size information */
	if (stat(file_name, &filestatus))
	{
		fprintf(stderr, "File %s not found\n", file_name);
		exit(1);
	}

	file_size = filestatus.st_size;
	json = (char *)malloc(file_size);
	if (json == NULL)
	{
		fprintf(stderr, "Memory error: unable to allocate %d bytes:\n", file_size);
		exit(1);
	}

	json_file = fopen(file_name, "rt");

	if (json_file == NULL)
	{
		fprintf(stderr, "Unable to open %s\n", file_name);
		fclose(json_file);
		free(json);
		exit(1);
	}

	if (!fread(json, file_size, 1, json_file))
	{
		fprintf(stderr, "Unable to read content of %s\n", file_name);
		fclose(json_file);
		free(json);
		exit(1);
	}

	fclose(json_file);
	json_chars = (json_char *)json;
	value = json_parse(json_chars, file_size);

	if (value == NULL)
	{
		fprintf(stderr, "Unable to parse data\n");
		free(json);
		exit(1);
	}

	parse_value(value, 0);
}

/**
 * @brief Parse the json value into the two demansional array.
 * @param value the vaule parsed out from the json string
 */
static void parse_frame(json_value *value, int depth)
{
	if (value->type != json_object && value->u.object.length)
	{
		fprintf(stderr, "Json file has a wrong format\n");
		exit(1);
	}

	json_value *frames;
	for (unsigned int i = 0; i < value->u.object.length; i++)
	{
		if (!strcmp(value->u.object.values[i].name, "frames"))
			frames = value->u.object.values[i].value;
	}
	if (frames->type != json_array)
	{
		fprintf(stderr, "Json file has a wrong format\n");
		exit(1);
	}

	frame_count = frames->u.array.length;
	frames_ptr = (double **)malloc(frame_count * sizeof(double *));

	for (unsigned long i = 0; i < frame_count; i++)
	{

		double *frame_ptr = (double *)malloc(RESOLUTION * sizeof(double));
		frames_ptr[i] = frame_ptr;

		json_value *frame = frames->u.array.values[i];
		if (frame->type != json_array)
		{
			fprintf(stderr, "Invalid frame\n");
			exit(1);
		}
		for (unsigned int j = 0; j < frame->u.array.length; j++)
		{
			json_value *value_dbl = frame->u.array.values[j];
			if (value_dbl->type != json_double)
			{
				fprintf(stderr, "Invalid data format\n");
				exit(1);
			}
			frame_ptr[j] = value_dbl->u.dbl;
		}
	}
}

/**
 *  @brief map the temperature value to grey scale value from 0 - 255 
 *  @param low lower limit of the temperature range that mapped from 
 *  @param higher limit of the temperature range that mapped from
 *  @temp temperature read from the sensor
 *  */
static uint8_t grey_map(int low, int high, double temp)
{
	temp = (temp - low) * 255 / (high - low);
	temp = (temp < 0) ? 0 : temp;
	temp = (temp > 255) ? 255 : temp;
	return (uint8_t)temp;
}

/**
 * @brief this function also convert the raw temperature values within the fame to 8bit greyscale values that are processable
 * 	  by the pipeline. This function should be called befor the IpProcess.
 * @param frame frame retrieved from the sensor
 */
static void frame_convert(uint8_t *frame)
{
	for (int i = 0; i < RESOLUTION; i++)
	{
		frame[i] = grey_map(llimit, hlimit, frames_ptr[img_ptr][i]);
	}
}

/**
 * @brief covert the data format of the frame into uint8, display the image in a named window
 * 	  and save it into cur_frame which will be passed into pipeline later.
 * @param process_mat a function pointer that will execute additional functionalities to the image
 */
static void show_image(uint8_t *frame, const char *window, void (*process_mat)(Mat *))
{
	Mat image = Mat(width, height, CV_8UC1, frame);

	if (process_mat != NULL)
		process_mat(&image);

	imshow(window, image);

	/* waitKey(0); */
}

/**
 * @brief draw rectangles detected by the pipeline on the image
 * @param image a pointer points to the image to be drawn on
 */
static void draw_rect(Mat *image)
{
	/* draw rectangles found by the pipeline */
	for (int i = 0; i < rec_num; i++) {
		rec temp = hrects[i];
		if (temp.rid == REC_IGNORE) continue;
		Point pt_min;
		pt_min.x = temp.min_x;
		pt_min.y = temp.min_y;
		Point pt_max;
		pt_max.x = temp.max_x; 
		pt_max.y = temp.max_y; 
		rectangle(*image, pt_min, pt_max, Scalar(255));
		// printf("rid: %d, area: %d\n", temp.rid, temp.area);
	}
}

/**
 * @brief Wrapper function, get the background of the pipeline, the way that the background
 * 	  is retrived can be replaced by overwritten this function 
 * @param frame background frame
 */
static void get_background(uint8_t *frame, unsigned long frame_count)
{
	/* take the first frame as the background frame */
	if (frame_count == 0)
		memcpy(background, frame, RESOLUTION * sizeof(uint8_t));
}

/**
 * @brief Read the configuration from the config file and load the parameters
 * @param depth a counter that counts how deep the recursion goes
 */
static void read_config(json_value *value, int depth)
{
	for (unsigned int i = 0; !depth && i < value->u.object.length; i++)
	{
		read_config(value->u.object.values[i].value, depth + 1);
	}
	for (unsigned int i = 0; depth == 1 && i < value->u.object.length; i++)
	{
		json_value *temp = value->u.object.values[i].value;
		int temp_value = temp->u.integer;
		switch (str2int(value->u.object.values[i].name))
		{
		case str2int("kernel_1"):
			config.kernel_1 = temp_value;
			break;
			case str2int("threshold"): config.threshold = temp_value;
			break;
			case str2int("max_area"): config.max_area = temp_value;	  
			break;
			case str2int("sensitivity"): config.sensitivity = temp_value;
			break;
		case str2int("width"):
			width = temp_value;
			break;
		case str2int("height"):
			height = temp_value;
			break;
		case str2int("llimit"):
			llimit = temp_value;
			break;
		case str2int("hlimit"):
			hlimit = temp_value;
			break;
		case str2int("frame_rate"):
			frame_rate = temp_value;
			break;
		default:
		{
			fprintf(stderr, "Unknown configuration: %s\n", value->u.object.values[i].name);
			exit(1);
		}
		}
	}
}
