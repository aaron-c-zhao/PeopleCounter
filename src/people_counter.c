/**
  ******************************************************************************
  * @file           : people_counter.c
  * @brief          : Implementation of the image processing pipeline
  ******************************************************************************
**/

#include "people_counter.h"
#include "string.h"

extern ip_config config;

#ifdef __TESTING_HARNESS
extern uint8_t rec_num;
extern ip_rect hrects[RECTS_MAX_SIZE];
extern uint8_t *th_frame;
#endif

ip_status IpProcess(void *frame, void *background_image, void *count)
{
  // stores all the bounding rectangles in the frame
  ip_rect rects[RECTS_MAX_SIZE];
  /* detect people in the frame */
  uint8_t rects_count = detectPeople((ip_mat *)frame, (ip_mat *)background_image, rects);

/* output the number of rectangles in the frame */
#ifdef __TESTING_HARNESS
  rec_num = rects_count;
  memcpy(hrects, rects, RECTS_MAX_SIZE * sizeof(ip_rect));
#endif

  /* update centroids location */
  ip_count count_update = updateObjects(rects, rects_count);

  ip_count *result = (ip_count *)count;

  result->direc = count_update.direc;
  result->num = count_update.num < 0 ? 0 : count_update.num;
  switch (count_update.num){
    case -1:
      return (IP_EMPTY);
    case 0:
      return (IP_STILL);
    default:
      return (IP_UPDATE);
  }
}

/* PEOPLE DETECTOR */

uint8_t detectPeople(ip_mat *frame, ip_mat *background_image, ip_rect *rects)
{
  // Don't do this, this will blur the background image every cycle.
  // If we use eigen background we don't need this step.
  // Otherwise we need to make sure that we blur the background only once.
  // Blur(background_image, KERNEL_1);

  stackBlur(frame->data, config.kernel_1);

#ifdef __TESTING_HARNESS
  memcpy(th_frame, frame->data, SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT *sizeof(uint8_t));
#endif 

  absDiff(frame, background_image);
  gaussianBlur(frame, config.kernel_3);
  // copy blurred image in case we need to re-apply the threshold.
  uint8_t data[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT];
  ip_mat blurred_image = {.data = data};
  for(int i = 0; i < SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT; ++i) {
      data[i] = frame->data[i];
  }
  threshold(frame, config.threshold);
/*
#ifdef __TESTING_HARNESS
  memcpy(th_frame, frame->data, SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT *sizeof(uint8_t));
#endif 
*/
  /* Blur(frame, config.kernel_4); */
  ip_rect temp_rects[RECTS_MAX_SIZE];
  uint8_t n_rects = findCountours(frame, temp_rects);

  uint8_t final_n_rects = 0;

  // filter blobs
  for (uint8_t i = 0; i < n_rects; ++i)
  {
    if (temp_rects[i].width < config.blob_width_min || temp_rects[i].height < config.blob_height_min)
    {
      continue;
    }
    if (temp_rects[i].width * temp_rects[i].height > config.max_area)
    {
      return (updatedDetection(&blurred_image, temp_rects, rects));
    }
    rects[final_n_rects++] = temp_rects[i];
  }

  return (final_n_rects);
}

uint8_t updatedDetection(ip_mat *frame, ip_rect *temp_rects, ip_rect *rects)
{
  threshold(frame, config.updated_threshold);
  uint8_t n_rects = findCountours(frame, temp_rects);

  uint8_t final_n_rects = 0;

  // filter blobs
  for (uint8_t i = 0; i < n_rects; ++i)
  {
    if (temp_rects[i].width < config.blob_width_min || temp_rects[i].height < config.blob_height_min)
    {
      continue;
    }

    rects[final_n_rects++] = temp_rects[i];
  }

  return (final_n_rects);
}

uint8_t findCountours(ip_mat *frame, ip_rect *rects)
{
  uint8_t result_rects_length = 0;

  uint16_t found_pixels_indexes[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT];
  uint16_t found_pixels_count = 0;

  for (int i = 0; i < (SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT); ++i)
  {
    if (frame->data[i] == 255)
    {
      if (findValueIndex(found_pixels_indexes, found_pixels_count, i) != -1)
      {
        continue;
      }
      uint16_t blob_queue[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT];
      uint16_t blob_queue_start = 0;
      uint16_t blob_queue_end = 0;
      blob_queue[blob_queue_end++] = i;
      found_pixels_indexes[found_pixels_count++] = i;

      uint8_t min_x = SENSOR_IMAGE_WIDTH + 1, max_x = 0;
      uint8_t min_y = SENSOR_IMAGE_HEIGHT + 1, max_y = 0;

      while (blob_queue_start < blob_queue_end)
      {
        uint16_t pixel = blob_queue[blob_queue_start++];

        uint8_t pixel_x = pixel % SENSOR_IMAGE_WIDTH;
        uint8_t pixel_y = (uint8_t)(pixel / SENSOR_IMAGE_WIDTH);

        if (pixel_x < min_x)
        {
          min_x = pixel_x;
        }
        if (pixel_x > max_x)
        {
          max_x = pixel_x;
        }
        if (pixel_y < min_y)
        {
          min_y = pixel_y;
        }
        if (pixel_y > max_y)
        {
          max_y = pixel_y;
        }

        //  Find neighbours (left, right, up, down)
        uint16_t neighbours[4];
        uint8_t neighbours_length = 0;
        // TODO put into declare or something  like that
        static int8_t directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (int d = 0; d < 4; ++d)
        {
          int8_t new_x = pixel_x + directions[d][0];
          int8_t new_y = pixel_y + directions[d][1];

          if (new_x < 0 || new_x > SENSOR_IMAGE_WIDTH - 1 || new_y < 0 || new_y > SENSOR_IMAGE_HEIGHT - 1)
          {
            continue;
          }

          neighbours[neighbours_length++] = pixel + directions[d][0] + directions[d][1] * SENSOR_IMAGE_WIDTH;
        }

        // add neighbour to queue if it's white
        for (uint8_t n_idx = 0; n_idx < neighbours_length; ++n_idx)
        {
          if (frame->data[neighbours[n_idx]] != 255 ||
              findValueIndex(found_pixels_indexes, found_pixels_count, neighbours[n_idx]) != -1)
          {
            continue;
          }
          blob_queue[blob_queue_end++] = neighbours[n_idx];
          found_pixels_indexes[found_pixels_count++] = neighbours[n_idx];
        }
      }
      rects[result_rects_length++] = (ip_rect) {min_x, min_y, max_x - min_x + 1, max_y - min_y + 1};
    }
  }
  return (result_rects_length);
}

int16_t findValueIndex(uint16_t *array, uint16_t length, uint16_t value)
{
  for (int i = 0; i < length; ++i)
  {
    if (array[i] == value)
    {
      return (i);
    }
  }
  return (-1);
}

void threshold(ip_mat *frame, uint8_t threshold)
{

  for (int y = 0; y < SENSOR_IMAGE_HEIGHT; ++y)
  {
    for (int x = 0; x < SENSOR_IMAGE_WIDTH; ++x)
    {
      uint8_t *pixel = frame->data + (SENSOR_IMAGE_WIDTH * y + x);
      if (*pixel > threshold)
      {
        *pixel = 255;
      }
      else
      {
        *pixel = 0;
      }
    }
  }
}

void absDiff(ip_mat *frame, ip_mat *background)
{
  for (int i = 0; i < SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT; ++i)
  {
    if (frame->data[i] > background->data[i])
    {
      frame->data[i] = frame->data[i] - background->data[i];
    }
    else
    {
      frame->data[i] = background->data[i] - frame->data[i];
    }
  }
}

void blur(ip_mat *frame, uint8_t kernel_size)
{
  uint8_t radius = kernel_size >> 1;

  //get the target pixel
  for (int y = 0; y < SENSOR_IMAGE_WIDTH; y++)
  {
    for (int x = 0; x < SENSOR_IMAGE_HEIGHT; x++)
    {
      uint16_t sum = 0;
      uint8_t count = 0;

      //loop through all directions of that pixel
      for (int d_y = -radius; d_y <= radius; ++d_y)
      {
        for (int d_x = -radius; d_x <= radius; ++d_x)
        {
          if (y + d_y > -1 && y + d_y < SENSOR_IMAGE_HEIGHT && x + d_x > -1 && x + d_x < SENSOR_IMAGE_WIDTH)
          {
            sum += frame->data[SENSOR_IMAGE_WIDTH * (y + d_y) + (x + d_x)];
            ++count;
          }
        }
      }
      /* TODO: fake code */
      count = (!count) ? 1 : count;
      frame->data[SENSOR_IMAGE_WIDTH * y + x] = (uint8_t)(sum / count);
    }
  }
}

void gaussianBlur(ip_mat *frame, uint8_t kernel_size)
{
  static const uint8_t kernel_3[3][3] = {{1, 2, 1},
                                         {2, 4, 2},
                                         {1, 2, 1}};

  static const uint8_t kernel_5[5][5] = {{1, 4, 7, 4, 1},
                                         {4, 16, 26, 16, 4},
                                         {7, 26, 41, 26, 7},
                                         {4, 16, 26, 16, 4},
                                         {1, 4, 7, 4, 1}};

  // don't apply blur if kernel is not 3 or 5
  //  if(kernel_size == 1 || kernel_size % 2 == 0 || (kernel_size != 3 && kernel_size != 5))
  if (kernel_size != 3 && kernel_size != 5)
  {
    return;
  }

  uint8_t radius = kernel_size >> 1;
  uint8_t result[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT] = {};

  if (kernel_size == 3)
  {
  //get the target pixel
    for (uint8_t y = 0; y < SENSOR_IMAGE_HEIGHT; ++y)
    {
      for (uint8_t x = 0; x < SENSOR_IMAGE_WIDTH; ++x)
      {
	uint32_t sum = 0;
	uint16_t count = 0;

	//loop through all directions of that pixel
	for (int8_t d_y = -radius; d_y <= radius; ++d_y)
	{
	  for (int8_t d_x = -radius; d_x <= radius; ++d_x)
	  {
	    if (y + d_y > -1 && y + d_y < SENSOR_IMAGE_HEIGHT && x + d_x > -1 && x + d_x < SENSOR_IMAGE_WIDTH)
	    {
	      sum += frame->data[SENSOR_IMAGE_WIDTH * (y + d_y) + (x + d_x)] * kernel_3[radius + d_y][radius + d_x];
	      count += kernel_3[radius + d_y][radius + d_x];
	    }
	  }
	}

	result[SENSOR_IMAGE_WIDTH * y + x] = (count == 0) ? 0 : (uint8_t)(sum / count);
      }
    }
  }

  // duplicate code, find better way to optimise this
  else if (kernel_size == 5) {
    for (uint8_t y = 0; y < SENSOR_IMAGE_HEIGHT; ++y)
    {
      for (uint8_t x = 0; x < SENSOR_IMAGE_WIDTH; ++x)
      {
	uint32_t sum = 0;
	uint16_t count = 0;

	//loop through all directions of that pixel
	for (int8_t d_y = -radius; d_y <= radius; ++d_y)
	{
	  for (int8_t d_x = -radius; d_x <= radius; ++d_x)
	  {
	    if (y + d_y > -1 && y + d_y < SENSOR_IMAGE_HEIGHT && x + d_x > -1 && x + d_x < SENSOR_IMAGE_WIDTH)
	    {
	      sum += frame->data[SENSOR_IMAGE_WIDTH * (y + d_y) + (x + d_x)] * kernel_5[radius + d_y][radius + d_x];
	      count += kernel_5[radius + d_y][radius + d_x];
	    }
	  }
	}

	result[SENSOR_IMAGE_WIDTH * y + x] = (count == 0) ? 0 : (uint8_t)(sum / count);
      }
    }
  }

  // TODO optimise this?
  for (uint16_t i = 0; i < SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT; ++i)
  {
    frame->data[i] = result[i];
  }
}

/* PEOPLE TRACKER */

ip_count updateObjects(ip_rect *rects, uint8_t rects_count)
{
  static ip_object_list objects = {0, {}, 0, 0};

  // TODO find a way to properly return these values.
  uint8_t total_up = 0, total_down = 0;

  // if there are no blobs in the frame, then increase the count of disappeared frames of every object
  if (rects_count == 0)
  {
    // increase disappered count of every object
    for (int i = objects.start_index; i < objects.start_index + objects.length; ++i)
    {
      ++objects.object[i % TRACKABLE_OBJECT_MAX_SIZE].disappeared_frames_count;
    }

    // shift forward the starting index by "removing" old objects
    for (int i = objects.start_index; i < objects.start_index + objects.length; ++i)
    {
      if (objects.object[i % TRACKABLE_OBJECT_MAX_SIZE].disappeared_frames_count > CT_MAX_DISAPPEARED)
      {
        ++objects.start_index;
        --objects.length;
      }
      else
      {
        break;
      }
    }

    // loop back index if necessary
    objects.start_index %= TRACKABLE_OBJECT_MAX_SIZE;
    if (objects.length == 0)
    {
	return ((ip_count){DIRECTION_UP, -1});
    }

    if(objects.length == 0)
    {
      return ((ip_count){DIRECTION_UP, -1});
    }

    return ((ip_count){DIRECTION_UP, 0});
  }

  ip_point input_centroids[rects_count];

  // loop over the bounding box rectangles
  // TODO check if it's necessary to reverse order (as c++ code)
  for (int i = 0; i < rects_count; ++i)
  {
    //use the bounding box coordinates to derive the centroid
    input_centroids[i] = (ip_point) {(uint8_t)(rects[i].x + (uint8_t)(rects[i].width / 2)),
                          (uint8_t)(rects[i].y + (uint8_t)(rects[i].height / 2))};
  }

  if (objects.length == 0)
  {
    // Register centroids
    for (int i = 0; i < rects_count; ++i)
    {
      // uint8_t next_index = objects.start_index+objects.length;
      objects.object[objects.next_id % TRACKABLE_OBJECT_MAX_SIZE] = (ip_object) {objects.next_id, input_centroids[i], 0};

      ++objects.length;
      ++objects.next_id;
    }

    // Return early since there is no need to update old object centroids
    return ((ip_count){DIRECTION_UP, 0});
  }

  uint16_t distance_vector[objects.length * rects_count];

  //object_centroids is a list of centroids we already have, input_centroids is the new centroid
  for (uint8_t i = 0; i < objects.length; ++i)
  {
    for (uint8_t j = 0; j < rects_count; ++j)
    {
      uint8_t object_index = (objects.start_index + i) % TRACKABLE_OBJECT_MAX_SIZE;
      int8_t delta_x = objects.object[object_index].centroid.x - input_centroids[j].x;
      int8_t delta_y = objects.object[object_index].centroid.y - input_centroids[j].y;

      // calculate euclidean distance (skip the square root, because we just need to sort based on it)
      uint16_t distance = delta_x * delta_x + delta_y * delta_y;

      distance_vector[i * rects_count + j] = distance;
    }
  }

  /*in order to perform this matching we must(1) find the
		smallest value in each rowand then (2) sort the row
		indexes based on their minimum values so that the row
		with the smallest value as at the* front* of the index list*/

  // temporary struct to make algorithm easier

  ip_closest_centroid closest_centroids[objects.length];

  for (uint8_t i = 0; i < objects.length; ++i)
  {
    // set minimum to largest uint16_t value by converting complement 2 -1 int to unsigned int
    uint16_t minimum = -1;
    uint8_t temp_index = 0;

    for (uint8_t j = 0; j < rects_count; ++j)
    {
      if (distance_vector[i * rects_count + j] < minimum)
      {
        minimum = distance_vector[i * rects_count + j];
        temp_index = j;
      }
    }

    closest_centroids[i] = (ip_closest_centroid) {minimum, (objects.start_index + i) % TRACKABLE_OBJECT_MAX_SIZE, temp_index};
  }

  // sort based on length
  bubbleSort(closest_centroids, objects.length);

  uint8_t used_count = 0;

  for (uint8_t i = 0; i < ((objects.length < rects_count) ? objects.length : rects_count); ++i)
  {

    if (isCentroidUsed(closest_centroids, i) ||
        closest_centroids[i].distance > CT_MAX_DISTANCE * CT_MAX_DISTANCE)
    {
      continue;
    }

    ++used_count;
    uint8_t object_id = closest_centroids[i].object_index;

    if (input_centroids[closest_centroids[i].rect_index].y < SENSOR_IMAGE_HEIGHT / 2 && objects.object[object_id].centroid.y >= SENSOR_IMAGE_HEIGHT / 2)
    {
      ++total_up;
    }
    else if (input_centroids[closest_centroids[i].rect_index].y >= SENSOR_IMAGE_HEIGHT / 2 && objects.object[object_id].centroid.y < SENSOR_IMAGE_HEIGHT / 2)
    {
      ++total_down;
    }

    // update the object id centroid location to the closest input centroid
    objects.object[object_id].centroid = input_centroids[closest_centroids[i].rect_index];

    // reset the disapperead counter of that
    objects.object[object_id].disappeared_frames_count = 0;
  }

  if (objects.length >= rects_count)
  {
    for (uint8_t i = used_count; i < objects.length; ++i)
    {
      uint8_t object_id = closest_centroids[i].object_index;
      ++objects.object[object_id].disappeared_frames_count;
    }

    // TODO duplicate code as at the start of function, refactor?
    // Deregister old objects
    for (int i = objects.start_index; i < objects.start_index + objects.length; ++i)
    {
      // shift forward the starting index to "remove" old objects
      if (objects.object[i % TRACKABLE_OBJECT_MAX_SIZE].disappeared_frames_count > CT_MAX_DISAPPEARED)
      {
        ++objects.start_index;
        --objects.length;
      }
      else
      {
        break;
      }
    }

    // loop back index if necessary
    objects.start_index %= TRACKABLE_OBJECT_MAX_SIZE;
  }
  else
  {
    // Register centroids
    // TODO optimise this double loop
    for (int i = 0; i < rects_count; ++i)
    {
      for (int j = 0; j < objects.length; ++j)
      {
        if (closest_centroids[j].rect_index == i)
        {
          continue;
        }
      }

      objects.object[objects.next_id % TRACKABLE_OBJECT_MAX_SIZE] = (ip_object) {objects.next_id, input_centroids[i], 0};

      ++objects.length;
      ++objects.next_id;
    }
  }

  int8_t delta_people_count = total_up - total_down;
  if (delta_people_count < 0)
  {
    return ((ip_count){DIRECTION_DOWN, -delta_people_count});
  }
  else
  {
    return ((ip_count){DIRECTION_UP, delta_people_count});
  }
}

/*
 * Checks whether the current centroid has already been assigned before
 */
uint8_t isCentroidUsed(ip_closest_centroid *closest_centroids, uint8_t current_reached_index)
{
  uint8_t index_to_find = closest_centroids[current_reached_index].rect_index;

  for (uint8_t i = 0; i < current_reached_index; ++i)
  {
    if (closest_centroids[i].rect_index == index_to_find)
    {
      return (1);
    }
  }
  return (0);
}

// A function to implement bubble sort
void bubbleSort(ip_closest_centroid *array, uint8_t length)
{
  uint8_t i, j;
  for (i = 0; i < length - 1; ++i)
  {
    // Last i elements are already in place
    for (j = 0; j < length - i - 1; ++j)
    {
      if (array[j].distance > array[j + 1].distance)
      {
        ip_closest_centroid temp = array[j];
        array[j] = array[j+1];
        array[j+1] = temp;
      }
    }
  }
}

/* The Stack Blur Algorithm was invented by Mario Klingemann, 
mario@quasimondo.com and described here:
http://incubator.quasimondo.com/processing/fast_blur_deluxe.php

Copyright (c) 2010 Mario Klingemann
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

This is the C monochrome (8 bit grayscale) single threaded version
Based heavily on http://vitiy.info/Code/stackblur.cpp by Victor Laskin (victor.laskin@gmail.com)
More details: http://vitiy.info/stackblur-algorithm-multi-threaded-blur-for-cpp
*/

static uint16_t const stackblur_mul[255] =
{
		512,512,456,512,328,456,335,512,405,328,271,456,388,335,292,512,
		454,405,364,328,298,271,496,456,420,388,360,335,312,292,273,512,
		482,454,428,405,383,364,345,328,312,298,284,271,259,496,475,456,
		437,420,404,388,374,360,347,335,323,312,302,292,282,273,265,512,
		497,482,468,454,441,428,417,405,394,383,373,364,354,345,337,328,
		320,312,305,298,291,284,278,271,265,259,507,496,485,475,465,456,
		446,437,428,420,412,404,396,388,381,374,367,360,354,347,341,335,
		329,323,318,312,307,302,297,292,287,282,278,273,269,265,261,512,
		505,497,489,482,475,468,461,454,447,441,435,428,422,417,411,405,
		399,394,389,383,378,373,368,364,359,354,350,345,341,337,332,328,
		324,320,316,312,309,305,301,298,294,291,287,284,281,278,274,271,
		268,265,262,259,257,507,501,496,491,485,480,475,470,465,460,456,
		451,446,442,437,433,428,424,420,416,412,408,404,400,396,392,388,
		385,381,377,374,370,367,363,360,357,354,350,347,344,341,338,335,
		332,329,326,323,320,318,315,312,310,307,304,302,299,297,294,292,
		289,287,285,282,280,278,275,273,271,269,267,265,263,261,259
};

static uint8_t const stackblur_shr[255] =
{
		9, 11, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17,
		17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19,
		19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20,
		20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21,
		21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
		21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22,
		22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
		22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23,
		23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
		23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
		23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
		23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
		24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
		24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
		24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
		24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24
};


void stackBlur(uint8_t *src, uint8_t radius)
{
  /* skip blur if blur is < 1*/
  if(radius < 1)
  {
    return;
  }

  uint16_t x, y, xp, yp, i;

  uint8_t stack[radius * (radius + 2) + 1];
  uint16_t sp;
  uint16_t stack_start;
  uint8_t *stack_ptr;

  uint8_t *src_ptr;
  uint8_t *dst_ptr;

  uint32_t sum_a;
  uint32_t sum_in_a;
  uint32_t sum_out_a;

  uint8_t wm = SENSOR_IMAGE_WIDTH - 1;
  uint8_t hm = SENSOR_IMAGE_HEIGHT - 1;

  uint16_t div = (radius * 2) + 1;
  uint16_t mul_sum = stackblur_mul[radius];
  uint8_t shr_sum = stackblur_shr[radius];

  /* step 1*/

  for (y = 0; y < SENSOR_IMAGE_HEIGHT; ++y)
  {
    sum_a = sum_in_a = sum_out_a = 0;

    src_ptr = src + SENSOR_IMAGE_WIDTH * y; // start of line (0,y)

    for (i = 0; i <= radius; i++)
    {
      stack_ptr = &stack[i];
      stack_ptr[0] = src_ptr[0];
      sum_a += src_ptr[0] * (i + 1);
      sum_out_a += src_ptr[0];
    }

    for (i = 1; i <= radius; i++)
    {
      if (i <= wm)
      {
        ++src_ptr;
      }
      stack_ptr = &stack[(i + radius)];
      stack_ptr[0] = src_ptr[0];
      sum_a += src_ptr[0] * (radius + 1 - i);
      sum_in_a += src_ptr[0];
    }

    sp = radius;
    xp = radius;
    if (xp > wm)
    {
      xp = wm;
    } 
    src_ptr = src + (xp + y * SENSOR_IMAGE_WIDTH);  // img.pix_ptr(xp, y);
    dst_ptr = src + y * SENSOR_IMAGE_WIDTH;         // img.pix_ptr(0, y);
    for (x = 0; x < SENSOR_IMAGE_WIDTH; x++)
    {
      dst_ptr[0] = (sum_a * mul_sum) >> shr_sum;
      ++dst_ptr;

      sum_a -= sum_out_a;

      stack_start = sp + div - radius;
      if (stack_start >= div)
        stack_start -= div;
      stack_ptr = &stack[stack_start];
      
      sum_out_a -= stack_ptr[0];

      if (xp < wm)
      {
        ++src_ptr;
        ++xp;
      }

      stack_ptr[0] = src_ptr[0];

      sum_in_a += src_ptr[0];
      sum_a += sum_in_a;

      ++sp;
      if (sp >= div)
      {
        sp = 0;
      }
      stack_ptr = &stack[sp];

      sum_out_a += stack_ptr[0];
      sum_in_a -= stack_ptr[0];
    }
  }

  /* step 2 */

  for (x = 0; x < SENSOR_IMAGE_WIDTH; ++x)
  {
    sum_a = sum_in_a = sum_out_a = 0;

    src_ptr = src + x; // x,0
    for (i = 0; i <= radius; i++)
    {
      stack_ptr = &stack[i];
      stack_ptr[0] = src_ptr[0];
      sum_a += src_ptr[0] * (i + 1);
      sum_out_a += src_ptr[0];
    }
    for (i = 1; i <= radius; i++)
    {
      if (i <= hm)
      {
        src_ptr += SENSOR_IMAGE_WIDTH; // +stride
      }

      stack_ptr = &stack[(i + radius)];
      stack_ptr[0] = src_ptr[0];
      sum_a += src_ptr[0] * (radius + 1 - i);
      sum_in_a += src_ptr[0];
    }

    sp = radius;
    yp = radius;
    if (yp > hm)
    {
      yp = hm;
    }
    src_ptr = src + (x + yp * SENSOR_IMAGE_WIDTH); // img.pix_ptr(x, yp);
    dst_ptr = src + x;                             // img.pix_ptr(x, 0);
    for (y = 0; y < SENSOR_IMAGE_HEIGHT; y++)
    {
      dst_ptr[0] = (sum_a * mul_sum) >> shr_sum;
      dst_ptr += SENSOR_IMAGE_WIDTH;

      sum_a -= sum_out_a;

      stack_start = sp + div - radius;
      if (stack_start >= div)
      {
        stack_start -= div;
      }
      stack_ptr = &stack[stack_start];

      sum_out_a -= stack_ptr[0];

      if (yp < hm)
      {
        src_ptr += SENSOR_IMAGE_WIDTH; // stride
        ++yp;
      }

      stack_ptr[0] = src_ptr[0];

      sum_in_a += src_ptr[0];
      sum_a += sum_in_a;

      ++sp;
      if (sp >= div)
      {
        sp = 0;
      }
      stack_ptr = &stack[sp];

      sum_out_a += stack_ptr[0];
      sum_in_a -= stack_ptr[0];
    }
  }
}