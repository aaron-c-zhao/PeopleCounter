/**
  ******************************************************************************
  * @file           : people_counter.c
  * @brief          : Implementation of the image processing pipeline
  ******************************************************************************
**/

#include "people_counter.h"
#include "string.h"
#include <stdio.h>

/* struct that keeps the information of configurations */
extern ip_config config;

#ifdef __TESTING_HARNESS
extern uint8_t rec_num;
extern ip_rect hrects[RECTS_MAX_SIZE];
extern uint8_t *th_frame;
#endif



void background_substraction(uint16_t, ip_mat*, ip_mat*, ip_mat*);
void nthreshold(uint16_t, uint8_t, ip_mat*, ip_mat*);
void LoG(uint8_t, int8_t**, ip_mat*, ip_mat*);

ip_status IpProcess(void *frame, void *background_image, void *count, void *log_kernel)
{	
	uint8_t log_frame[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT] = {0};
	ip_mat log_mat = {.data = log_frame };
	ip_mat* frame_mat = (ip_mat *)frame;
	ip_mat* frame_bak = (ip_mat *)background_image;
	int8_t **kernel = (int8_t **)log_kernel;
	background_substraction(SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT, background_image, frame, frame);
	LoG(LOG_KSIZE, kernel, frame_mat, &log_mat); 
	//nthreshold(SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT, 25, frame, frame);
#ifdef __TESTING_HARNESS
	memcpy(th_frame, log_frame, SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT * sizeof(uint8_t));
#endif
	return IP_EMPTY;

}


void background_substraction(uint16_t resolution, ip_mat* background, ip_mat* src, ip_mat* dst) {
	uint8_t *bframe = background->data;
	uint8_t *sframe = src->data;
	uint8_t *dframe = dst->data;
	
	for (uint16_t i = 0; i < resolution; i++) {
		int8_t temp = sframe[i] - bframe[i];
		dframe[i] = (temp > 0)? temp : -temp;
	}
}



void nthreshold(uint16_t resolution, uint8_t thre, ip_mat* src, ip_mat* dst) {
	uint8_t* sframe = src->data;
	uint8_t* dframe = src->data;
	
	for (uint16_t i = 0; i < resolution; i++) {
		dframe[i] = (sframe[i] > thre)? 255 : 0;
	}
}


static inline int convolve(uint8_t ksize,  int8_t **kernel, uint8_t *m, uint8_t x, uint8_t y, uint8_t padding) {
	/* decide whether padding will be applied at location (x, y) */
	uint8_t p_x = (x - padding < 0)? padding - x : 0;
	uint8_t p_y = (y - padding < 0)? padding - y : 0;
	uint8_t	i_y, i_x; 
	
	int sum = 0;
	for (uint8_t k_x = p_x, i_x = x; k_x < ksize && i_x < SENSOR_IMAGE_HEIGHT; ++k_x) {
		for (uint8_t k_y = p_y, i_y = y; k_y < ksize && i_y < SENSOR_IMAGE_WIDTH; ++k_y) {
			sum += m[i_x * SENSOR_IMAGE_WIDTH + i_y] * kernel[k_x][k_y]; 
			i_y++;
		}
		i_x++;
	}
	return sum;
}

void LoG(uint8_t ksize,  int8_t **kernel, ip_mat *src, ip_mat *dst) {
	uint8_t pad_length = ksize / 2;
	uint8_t *sframe = src->data;
	uint8_t *dframe = dst->data;
	printf("  ");
	for (uint8_t i = 0; i < SENSOR_IMAGE_WIDTH; ++i) printf("%5d", i);
	printf("\n");
	uint8_t count = 0;
	for (uint8_t i = 0; i < SENSOR_IMAGE_HEIGHT; ++i) {
		printf("%2d " , count++);
		for (uint8_t j = 0; j < SENSOR_IMAGE_WIDTH; ++j) {
			int16_t c = convolve(ksize, kernel, sframe, i, j, pad_length);
			dframe[i * SENSOR_IMAGE_WIDTH + j] = (c < -config.threshold)? 255 : 0;
			if ( c < -config.threshold ) 
				printf("\033[1;31m%5d\033[0m", c);
			else 
				printf("%5d", c);
		}
		printf("\n");
	}
	printf("-------------------------------------------------------------------\n");
}



	



















































	
