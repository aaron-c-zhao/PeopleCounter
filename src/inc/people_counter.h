/**
  ******************************************************************************
  * @file           : people_counter.h
  * @brief          : Header file for storing the configurations of image processing pipeline.
  *                   This file contains all of the parameters of the image processing pipeline.
  ******************************************************************************
**/

#ifndef __PEOPLE_COUNTER
#define __PEOPLE_COUNTER

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "testing_harness.h"

// CT = Centroid Tracker
#define CT_MAX_DISAPPEARED 	2
#define CT_MAX_DISTANCE 	8

// Array max sizes
#define RECTS_MAX_SIZE 		28
// TODO consider a power 2 size for optimization sake. See: https://stackoverflow.com/questions/11040646/faster-modulus-in-c-c
#define TRACKABLE_OBJECT_MAX_SIZE 18

// Image size
#define SENSOR_IMAGE_WIDTH 	32
#define SENSOR_IMAGE_HEIGHT 	24 

/* Kernel size of the LoG */
#define LOG_KSIZE 	5
#define LOG_SIGMA	1.5
#define QUEUE_SIZE	100
/* Sensor frame rate */
#define FRAME_RATE	8
// Directions
#define DIRECTION_UP 	0x00
#define DIRECTION_DOWN 	0x01

  /* *******************************************************************************
 * Structure to host the frames fetched from MLX90640 sensor.
 * Note1: each pixel should be converted to 8 bit integer.
 * Note2: time stamp has a resolution of milliseconds.
 *
 *******************************************************************************/

/* IMAGE STRUCT */
typedef struct mat
{
uint8_t *data;
} ip_mat;


/* PEOPLE COUNT STRUCT */
typedef struct count
{
uint8_t direc;
int8_t num;
} ip_count;


typedef struct config
{
uint8_t kernel_1;
int16_t threshold;
uint8_t max_area;
} ip_config;


typedef struct pixel {
	/* coordinates of the pixel */
	uint8_t x;
	uint8_t y;
} pixel;

typedef struct queue {
	uint8_t count;
	uint8_t top;
	uint8_t bottom;
	pixel pixels[QUEUE_SIZE];
} queue;

typedef struct rec {
	uint8_t min_x;
	uint8_t min_y;
	uint8_t max_x;
	uint8_t max_y;
	uint8_t rid;
	uint16_t area;
} rec;

typedef struct recs {
	uint8_t count;
	rec nodes[RECTS_MAX_SIZE];
} recs;

typedef enum ip_status {
	IP_EMPTY,
	IP_STILL,
	IP_UPDATE
} ip_status;
	


/* -------------------------------------------------------function prototype----------------------------------------------------*/
  ip_status IpProcess(void *, void *, void *, void *);



#ifdef __cplusplus
}
#endif

#endif
