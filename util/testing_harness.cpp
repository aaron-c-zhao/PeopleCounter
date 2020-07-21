#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <opencv2/opencv.hpp>

#include "people_counter.h" 
#include "json.h"
#include "testing_harness.h"

#define RESOLUTION	(width * height)

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

/* pointer to the frame array */
static double** frames_ptr;

/* path to the configuration file */
static char *config_path = "../harness_config.json";

/* image pointer which indicates which image should be processed next */
static unsigned long img_ptr = 0;

/* how many frames are there in this video */
static unsigned long frame_count = 0; 

/* background image that will be passed into pipeline */
static uint8_t* background;

/* counting result */
ip_count count = {0, 0};

/* configuration of the pipeline */
ip_config config = {
	.kernel_1 = 5,
	.kernel_2 = 1,
	.kernel_3 = 1,
	.threshold = 32,
	.blob_width_min = 0,
	.blob_height_min = 0,
	.updated_threshold = 84, 
	.max_area = 18
};

/* number of rectangles detected in the frame */
uint8_t rec_num = 0; 

/* rectangles find by the pipeline */
ip_rect hrects[RECTS_MAX_SIZE] = {{0,0,0,0}};


/*---------------------------------------------------------------------------------------------*/


/*--------------------------------------------function prototypes------------------------------*/

static void parse_json(char*, void (*parse_value)(json_value *, int));
static void parse_frame(json_value*, int);
static uint8_t grey_map(int, int, double);
static void show_image(uint8_t*);
static void get_background(uint8_t *, unsigned long);
static void read_config(json_value* ,int);
static void frame_convert(uint8_t*);

/*---------------------------------------------------------------------------------------------*/

/* hash function that will map string to an int */
constexpr unsigned int str2int(const char* str, int h = 0)
{
    return !str[h] ? 5381 : (str2int(str, h+1) * 33) ^ str[h];
}


int main(int argc, char *argv[]) {
	parse_json(config_path, read_config);

	uint8_t *cur_frame = (uint8_t *)malloc(RESOLUTION * sizeof(uint8_t));
	uint8_t *buf_frame = (uint8_t *)malloc(RESOLUTION * sizeof(uint8_t));
	background = (uint8_t *)malloc(RESOLUTION * sizeof(uint8_t));
	ip_mat mat_background = {.data = background};

	/* Check whether the amount of arguments is correct */
	if (argc != 2) {
		fprintf(stderr, "Invalid argument(s) please only enter the name of the json file\n");
		exit(1);
	}

	char *file_name = argv[1];

	parse_json(file_name, parse_frame);
	printf("frame count is %d\n", frame_count);
	
	namedWindow("Thermal image", WINDOW_NORMAL);
	resizeWindow("Thermal image", 300, 300);
	while (img_ptr < frame_count) {
		/* first convert the raw thermal data into processable and displayable format, namely frame and Mat */
		frame_convert(cur_frame);
		memcpy(buf_frame, cur_frame, RESOLUTION * sizeof(uint8_t));
		/* construct the proper struct required by the pipeline */
		ip_mat mat = {.data = cur_frame};
		/* get the background TODO: should be done by the pipeline */
		get_background(cur_frame, img_ptr);
		ip_status status = IpProcess((void *)&mat, (void *)&mat_background, (void *)&count);
		/* the show_image should be called after the IpProcess to correctly display the rectangles 
		 * found by pipeline */
		show_image(buf_frame);
		if (status == IP_EMPTY) {
			printf("\033[1;31m");	
			printf("Frame[%ld]ame is empty\n", img_ptr);
			printf("\033[0m;");
		}
		else if (status == IP_STILL) {
			printf("\033[1;33m");	
			printf("Frame[%ld] is still\n", img_ptr);
			printf("\033[0m;");
		}
		else {
			printf("\033[1;32m");	
			printf("Frame[%ld], Dir: %s, Count: %d\n", img_ptr,  (count.direc == DIRECTION_UP)? "UP" : "DOWN", count.num);
			printf("\033[0m;");

		}
		img_ptr++;
	}
	free(cur_frame);
	free(buf_frame);
	free(background);
	for (int i = 0; i < frame_count; i++) {
		free(frames_ptr[i]);
	}
	free(frames_ptr);

}
		
/**
 * @brief Parse the json file into a two-demansinal array consits of temperatures retrived
 * 	  by the sensor.
 * @param file_name path to the json file
 * @param parse_value a function pointer that points to the function which parses the json_value
 */ 
static void parse_json(char *file_name, void (*parse_value)(json_value *, int)) {
	FILE *json_file;
	int file_size;
	struct stat filestatus;
	char* json;
	json_char* json_chars;
	json_value* value;
	
	/* Check whether the file exists as well as retrive the size information */
	if (stat(file_name, &filestatus)) {
		fprintf(stderr, "File %s not found\n", file_name);
		exit(1);		
	}	

	file_size = filestatus.st_size;
	json = (char *)malloc(file_size);
	if (json == NULL) {
		fprintf(stderr, "Memory error: unable to allocate %d bytes:\n", file_size);
		exit(1);
	}

	json_file = fopen(file_name, "rt");

	if (json_file == NULL) {
		fprintf(stderr, "Unable to open %s\n", file_name);
		fclose(json_file);
		free(json);
		exit(1);
	}

	if (!fread(json, file_size, 1, json_file)) {
		fprintf(stderr, "Unable to read content of %s\n", file_name);
		fclose(json_file);
		free(json);
		exit(1);
	}

	fclose(json_file);
	json_chars = (json_char*)json;
	value = json_parse(json_chars, file_size);
	
	if (value == NULL) {
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
static void parse_frame(json_value* value, int depth) {
	if (value->type != json_object && value->u.object.length) {
		fprintf(stderr, "Json file has a wrong format\n");
		exit(1);
	}

	json_value *frames;
	for (int i = 0; i < value->u.object.length; i++) {
		if (!strcmp(value->u.object.values[i].name, "frames"))
			frames = value->u.object.values[i].value;
	}
	if (frames->type != json_array) {
		fprintf(stderr, "Json file has a wrong format\n");
		exit(1);
	}
	
	frame_count = frames->u.array.length;
	frames_ptr = (double **)malloc( frame_count * sizeof(double *));	

	for (int i = 0; i < frame_count; i++) {

		double* frame_ptr = (double *)malloc(RESOLUTION * sizeof(double));
		frames_ptr[i] = frame_ptr;

		json_value* frame = frames->u.array.values[i];
		if (frame->type != json_array) {
			fprintf(stderr, "Invalid frame\n");
			exit(1);
		}
		for (int j = 0; j < frame->u.array.length; j++) {
			json_value* value_dbl = frame->u.array.values[j];
			if (value_dbl->type != json_double) {
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
static uint8_t grey_map(int low, int high, double temp) {
	temp = (temp - low) * 255 / (high - low);
	temp = (temp < 0)? 0 : temp;
	temp = (temp > 255)? 255 : temp; 
	return (uint8_t)temp;
}


/**
 * @brief this function also convert the raw temperature values within the fame to 8bit greyscale values that are processable
 * 	  by the pipeline. This function should be called befor the IpProcess.
 * @param frame frame retrieved from the sensor
 */
static void frame_convert(uint8_t* frame) {
	for (int i = 0; i < RESOLUTION; i++) {
		frame[i] = grey_map(llimit, hlimit, frames_ptr[img_ptr][i]);
	}
}



/**
 * @breif covert the data format of the frame into uint8, display the image in a named window
 * 	  and save it into cur_frame which will be passed into pipeline later.
 */
static void show_image(uint8_t *frame) {
	Mat image = Mat(width, height, CV_8UC1, frame); 

	/* draw rectangles found by the pipeline */
	for (int i = 0; i < rec_num; i++) {
		ip_rect temp = hrects[i];
		Point pt_min;
		pt_min.x = temp.x;
		pt_min.y = temp.y;
		Point pt_max;
		pt_max.x = temp.x + temp.width;
		pt_max.y = temp.y + temp.height;
		rectangle(image, pt_min, pt_max, Scalar(255));
	}

	imshow("Thermal image", image);

	waitKey(0);
}


/**
 * @brief Wrapper function, get the background of the pipeline, the way that the background
 * 	  is retrived can be replaced by overwritten this function 
 * @param frame background frame
 */
static void get_background(uint8_t* frame, unsigned long frame_count) {
	/* take the first frame as the background frame */
	if (frame_count == 0)
		memcpy(background, frame, RESOLUTION * sizeof(uint8_t));
}




/**
 * @brief Read the configuration from the config file and load the parameters
 * @param depth a counter that counts how deep the recursion goes
 */
static void read_config(json_value* value, int depth) {	
	for (int i = 0; !depth && i < value->u.object.length; i++)  {
		read_config(value->u.object.values[i].value, depth + 1);
	}
	for (int i = 0; depth == 1 && i < value->u.object.length; i++) {
		json_value* temp = value->u.object.values[i].value;
		int temp_value = temp->u.integer;	
		switch (str2int(value->u.object.values[i].name)) {	
			case str2int("kernel_1"): config.kernel_1 = temp_value;
			break;
			case str2int("kernel_2"): config.kernel_2 = temp_value; 
			break;
			case str2int("kernel_3"): config.kernel_3 = temp_value;
			break;
			case str2int("threshold"): config.threshold = temp_value;
			break;
			case str2int("blob_width_min"): config.blob_width_min = temp_value;
			break;
			case str2int("blob_height_min"): config.blob_height_min = temp_value;
			break;
			case str2int("updated_threshold"): config.updated_threshold = temp_value;
			break;
			case str2int("max_area"): config.max_area = temp_value;	  
			break;
			case str2int("width"): width = temp_value;
			break;
			case str2int("height"): height = temp_value;
			break;
			case str2int("llimit"): llimit = temp_value;
			break;
			case str2int("hlimit"): hlimit = temp_value;
			break;
			case str2int("frame_rate"): frame_rate = temp_value;
			break;
			default: {
					 fprintf(stderr, "Unknown configuration: %s\n", value->u.object.values[i].name);
					 exit(1);
				 }
		}
	}
}















