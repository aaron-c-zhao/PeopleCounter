#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <opencv2/opencv.hpp>

#include "people_counter.h" 
#include "json.h"

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
const static char *config_path = "../config.txt";

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

/*---------------------------------------------------------------------------------------------*/


/*--------------------------------------------function prototypes------------------------------*/

static void parse_json(char *file_name);
static void parse_value(json_value* value);
static uint8_t grey_map(int low, int high, double temp);
static void show_image(uint8_t* frame);
static void get_background(uint8_t *frame, unsigned long img_ptr);
static void read_config(void);

/*---------------------------------------------------------------------------------------------*/


int main(int argc, char *argv[]) {
	uint8_t cur_frame[RESOLUTION] = {0};
	background = (uint8_t *)malloc(RESOLUTION * sizeof(uint8_t));
	ip_mat mat_background = {.data = background};

	/* Check whether the amount of arguments is correct */
	if (argc != 2) {
		fprintf(stderr, "Invalid argument(s) please only enter the name of the json file\n");
		exit(1);
	}

	char *file_name = argv[1];

	parse_json(file_name);
	
	namedWindow("Thermal image", WINDOW_NORMAL);
	resizeWindow("Thermal image", 200, 200);
	while (img_ptr++ < frame_count) {
		show_image(cur_frame);
		ip_mat mat = {.data = cur_frame};
		get_background(cur_frame, img_ptr);
		ip_status status = IpProcess((void *)&mat, (void *)&mat_background, (void *)&count);
		if (status == IP_EMPTY) 
			printf("The frame is empty\n");
		else if (status == IP_STILL) 
			printf("The frame is still\n");
		else {
			printf("Dir: %s, Count: %d\n", (count.direc == DIRECTION_UP)? "UP" : "DOWN", count.num);

		}
	}
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
 *
 */ 
static void parse_json(char *file_name) {
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

	parse_value(value);	
}

/**
 * @brief Parse the json value into the two demansional array.
 * @param value the vaule parsed out from the json string
 */ 
static void parse_value(json_value* value) {
	if (value->type != json_object && value->u.object.length) {
		fprintf(stderr, "Json file has a wrong format\n");
		exit(1);
	}
	json_value *frames = value->u.object.values[0].value;
	
	if (frames->type != json_array) {
		fprintf(stderr, "Json file has a wrong format\n");
		exit(1);
	}
	
	frame_count = frames->u.array.length;
	frames_ptr = (double **)malloc( frame_count * sizeof(double *));	

	for (int i = 0; i < frame_count; i++) {

		double* frame_ptr = (double *)malloc(RESOLUTION * height * sizeof(double *));
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
 * @breif covert the data format of the frame into uint8, display the image in a named window
 * 	  and save it into cur_frame which will be passed into pipeline later.
 */
static void show_image(uint8_t* frame) {
	for (int i = 0; i < RESOLUTION; i++) {
		frame[i] = grey_map(llimit, hlimit, frames_ptr[img_ptr][i]);
	}

	Mat image;
	image = Mat(width, height, CV_8UC1, frame); 

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
	if (frame_count == 1)
		memcpy(background, frame, RESOLUTION * sizeof(uint8_t));
}



/**
 * @brief Read the configuration from the config file and load the parameters
 */
static void read_config(void) {
}















