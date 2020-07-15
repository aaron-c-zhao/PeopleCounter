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

#include <time.h>
#include "cmsis_os.h"

/* constants -------------------------------------------------------------------*/
/* base address of where the configurations are written on flash ---------------*/
#define CONFIG_BASE_ADDRESS (uint32_t)0x8067FF0
/* offsets for parameters ------------------------------------------------------*/

// PD = People Detector
#define KERNEL_1 		0
#define KERNEL_2 		1
#define KERNEL_3 		2
#define THRESHOLD 		3
#define BLOB_WIDTH_MIN 		4
#define BLOB_HEIGHT_MIN 	5
#define UPDATED_THRESHOLD	6
#define MAX_AREA		7
#define COUNT			8

// CT = Centroid Tracker
#define CT_MAX_DISAPPEARED 	2
#define CT_MAX_DISTANCE 	3

// Array max sizes
#define RECTS_MAX_SIZE 		8
// TODO consider a power 2 size for optimization sake. See: https://stackoverflow.com/questions/11040646/faster-modulus-in-c-c
#define TRACKABLE_OBJECT_MAX_SIZE 10

// Image size
#define SENSOR_IMAGE_WIDTH 	32
#define SENSOR_IMAGE_HEIGHT 	24

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

  /* FRAME STRUCT */

  typedef struct frame
  {
    ip_mat pixels;
    uint32_t stamp;
  } ip_frame;

  /* PEOPLE COUNT STRUCT */
  typedef struct count
  {
    uint8_t direc;
    int8_t num;
  } ip_count;

  /* RECTANGLE STRUCT */
  typedef struct rect
  {
    uint8_t x, y;
    uint8_t width, height;
  } ip_rect;

  /* POINT STRUCT */
  typedef struct point
  {
    uint8_t x, y;
  } ip_point;

  /* IMAGE STRUCT */

  typedef struct object
  {
    uint16_t id;
    ip_point centroid;
    uint8_t disappeared_frames_count;
  } ip_object;

  typedef struct object_list
  {
    uint16_t next_id;
    ip_object object[TRACKABLE_OBJECT_MAX_SIZE];
    uint8_t start_index;
    uint8_t length;
  } ip_object_list;

  typedef struct closest_centroid
  {
    uint16_t distance;
    uint8_t object_index;
    uint8_t rect_index;
  } ip_closest_centroid;

  typedef struct config
  {
    uint8_t kernel_1;
    uint8_t kernel_2;
    uint8_t kernel_3;
    uint8_t threshold;
    uint8_t blob_width_min;
    uint8_t blob_height_min;
    uint8_t updated_threshold;
    uint8_t max_area;
  } ip_config;

  typedef enum
  {
    IP_UPDATE,
    IP_STILL,
    IP_EMPTY
  } ip_status;

  /* Function prototypes -------------------------------------------------------- */

  ip_status IpProcess(void *, void *, void *, void *);

  /* PEOPLE DETECTOR */

  // Returns the number of rectangles
  uint8_t detectPeople(ip_mat *, ip_mat *, ip_rect *);

  uint8_t updatedDetection(ip_mat *, ip_rect *, ip_rect *);

  // Returns the number of rectangles
  uint8_t findCountours(ip_mat *, ip_rect *);

  int16_t findValueIndex(uint16_t *, uint16_t, uint16_t);

  void threshold(ip_mat *, uint8_t);

  void absDiff(ip_mat *, ip_mat *);

  void blur(ip_mat *, uint8_t);

  void gaussianBlur(ip_mat *, uint8_t);

  /* PEOPLE TRACKER */

  ip_count updateObjects(ip_rect *, uint8_t);

  uint8_t isCentroidUsed(ip_closest_centroid *, uint8_t);

  void bubbleSort(ip_closest_centroid *, uint8_t);

  void IpInit();

#ifdef __cplusplus
}
#endif

#endif
