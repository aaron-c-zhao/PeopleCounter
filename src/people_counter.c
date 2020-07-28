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
extern rec hrects[RECTS_MAX_SIZE];
extern uint8_t *th_frame;
#endif


void background_substraction(uint16_t, ip_mat*, ip_mat*, ip_mat*);
void nthreshold(uint16_t, uint8_t, ip_mat*, ip_mat*);
void LoG(uint8_t, int8_t**, ip_mat*, ip_mat*);
uint8_t find_blob(ip_mat* src, recs*);
void enqueue(queue*, pixel); 
pixel dequeue(queue*);
rec bfs(uint8_t*, queue*, uint8_t);
void blob_filter(ip_mat*, recs*, uint8_t, uint8_t);

ip_status IpProcess(void *frame, void *background_image, void *count, void *log_kernel)
{	
	uint8_t log_frame[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT] = {0};
	ip_mat log_mat = {.data = log_frame };
	ip_mat* frame_mat = (ip_mat *)frame;
	ip_mat* frame_bak = (ip_mat *)background_image;
	int8_t **kernel = (int8_t **)log_kernel;
	background_substraction(SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT, background_image, frame, frame);
	LoG(LOG_KSIZE, kernel, frame_mat, &log_mat); 
	static recs blobs = {0, {}};
	uint8_t blob_num = find_blob(&log_mat, &blobs); 
#ifdef __TESTING_HARNESS
	memcpy(th_frame, log_frame, SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT * sizeof(uint8_t));
	rec_num = blob_num;
	memcpy(hrects, blobs.nodes, RECTS_MAX_SIZE * sizeof(rec));	
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


/**
 * @brief function for normal threshold(deprecated)
 * @param resolution the number of pixels in the image
 * @param thre threshold according to which to binarize the image
 * @param src source image
 * @param dst destination image
 */ 
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
	
	int16_t sum = 0;
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



/**
 * @brief This function will find all the blobs in the thresholded image and mark the pixels
 *        that belong to different blobs with a unique rid
 * @pram src the source image
 * @param blobs the struct that holds the result blobs
 * @return the number of blobs found in the image
 */
uint8_t find_blob(ip_mat* src, recs* blobs) {
	uint8_t *sframe = src->data;
	uint8_t blob_counter = 0;
	queue pqueue = {0, 0, 0, {{0, 0}}};
	/* initialize the rid to be a unique number other than 0 and 255 */
	uint8_t rid = 42;
	for (uint8_t i = 0; i < SENSOR_IMAGE_HEIGHT; ++i) { 
		for (uint8_t j = 0; j < SENSOR_IMAGE_WIDTH; ++j) {
			/* when the pixel has not been visited by bfs(otherwise it will be marked as a rid */
			if (sframe[i * SENSOR_IMAGE_WIDTH + j] == 255) {
					/* j is the x coordinate and i is the y coordinate */
					pixel temp = {.x = j, .y = i};
					enqueue(&pqueue, temp);		
					/* the first pixel of the blob should be marked to prevent infinite loop */
					sframe[i * SENSOR_IMAGE_WIDTH + j] = rid;
					/* do the BFS */
					blobs->nodes[blob_counter++] = bfs(sframe, &pqueue, rid++);
			}
		}
	}
	return blob_counter;
}


/**
 * @brief data structure operation, enqueue
 * @param q the queue to be operate on
 * @prarm p the pixel to be put in the back of the queue
 */
void enqueue(queue *q, pixel p) {
	if (q->count >= QUEUE_SIZE)
		return;
	q->count++;
	q->pixels[q->top] = p;
	/* circular queue */
	q->top = (q->top + 1) % QUEUE_SIZE;
} 

/**
 * @brief data structure operation, dqueue
 * @param q the queue to be operate on
 * TODO: add a error handling to deal with empty queue
 */
pixel dequeue(queue *q) {
	q->count--;
	pixel p = q->pixels[q->bottom];
	/* circular queue */
	q->bottom = (q->bottom + 1) % QUEUE_SIZE;	
	return p;
}
/**
 * @brief conduct a breath first search on the image
 * @param frame the image to be searched
 * @param q the queue data structure used by BFS algorithm
 */
rec bfs(uint8_t *frame, queue* q, uint8_t rid) {
	uint8_t min_x, min_y, max_x, max_y;
	/* initialize the min x and min y to be the number larger than the max coordinate(31)*/
	min_x = min_y = 33;
	max_x = max_y = 0;
	/* initialize the area of the blob to be 1, since there's at least 1 pixel in the blob */
	uint16_t area = 1;
	/* BFS */
	while (q->count) {
		pixel p = dequeue(q);
		/* extract the coordinate of the bounding box */
		min_x = (min_x < p.x)? min_x : p.x;
		min_y = (min_y < p.y)? min_y : p.y;
		max_x = (max_x > p.x)? max_x : p.x;
		max_y = (max_y > p.y)? max_y : p.y;
		/* loop over all of the 8 neighbours of a pixel*/		
		for (int8_t k = -1; k < 2; ++k) {
			for (int8_t l = -1; l < 2; l++) {
				uint8_t x = p.x + k;
				uint8_t y = p.y + l;
				/* skip the pixel itself and when it goes out of bound */
				if ((l == 0 && k == 0) || x < 0 || y < 0 || x >= SENSOR_IMAGE_WIDTH || y >= SENSOR_IMAGE_HEIGHT) continue; 
				if (frame[y * SENSOR_IMAGE_WIDTH + x] == 255) {
					pixel temp = {p.x + k, p.y + l};
					enqueue(q, temp);
					/* mark the pixel that has been visited to prevent the algorithm from infinite looping */
					frame[y * SENSOR_IMAGE_WIDTH + x] = rid;	
					area++;
				}
			}
		}
	}
	rec result= {min_x, min_y, max_x, max_y, rid, area};
	return result;
}					



/**
 * @brief This function filters the blobs according to their area, erosion and dilation will be conducte
 * 	  to adjust the blobs whose area is between amin and amax
 * @param frame thresholded image with each blob marked by different rids
 * @param blobs the struct that holds the rectangles
 * @param amax the maximum area that a single blob can be
 * @param amin the minimum area that a single blob can be 
 */
void blob_filter(ip_mat *frame, recs *blobs, uint8_t amax, uint8_t amin) {
		
}


void erosion(ip_mat *frame, uint8_t rid, uint8_t ksize) {
} 































	
