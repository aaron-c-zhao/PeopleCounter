/**
  ******************************************************************************
  * @file           : people_counter.c
  * @brief          : Implementation of the image processing pipeline
  ******************************************************************************
**/

#include "people_counter.h"
#include "string.h"

/* struct that keeps the information of configurations */
extern ip_config config;

#ifdef __TESTING_HARNESS
extern uint8_t rec_num;
extern ip_rect hrects[RECTS_MAX_SIZE];
extern uint8_t *th_frame;
#endif

ip_status IpProcess(void *frame, void *background_image, void *count)
{
	return IP_EMPTY;

}

