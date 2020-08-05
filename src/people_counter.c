/**
 ******************************************************************************
 * @file           : people_counter.c
 * @brief          : Implementation of the image processing pipeline
 ******************************************************************************
 **/

#include "people_counter.h"
#include "string.h"

#define WHITE 255
/* the start number of the rid */
#define RID 42

/* struct that keeps the information of configurations */
extern ip_config config;

#ifdef __TESTING_HARNESS
// include for printf
#include <stdio.h>
// include for getting amount of instructions
#include <x86intrin.h>

/* the number of the blobs found by the pipeline */
extern uint8_t rec_num;
/* an array of rectangles, used to showcase the bounding boxes */
extern rec hrects[RECTS_MAX_SIZE];
/* a copy of the image which is going to be displayed in the threshold iamge window */
extern uint8_t *th_frame;
#endif

void background_substraction(uint16_t, ip_mat *, ip_mat *, ip_mat *);
void nthreshold(uint16_t, uint8_t, ip_mat *, ip_mat *);
void LoG(uint8_t, int8_t **, ip_mat *, ip_mat *);
void find_blob(uint8_t *src, recs *, uint8_t, uint8_t, uint8_t);
void enqueue(queue *, pixel);
pixel dequeue(queue *);
rec bfs(uint8_t *, queue *, uint8_t, uint8_t);
void blob_filter(uint8_t *, recs *, uint8_t, uint8_t);
void area_adjust(uint8_t *, rec *, recs *, uint8_t);
void erosion(uint8_t *, rec *);

ip_result IpProcess(void *frame, void *background_image, void *count, void *log_kernel)
{	
#ifdef __TESTING_HARNESS
	uint64_t start_tsc = readTSC();
	static uint64_t max_tsc = 0;
#endif

	uint8_t log_frame[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT] = {0};
	ip_mat log_mat = {.data = log_frame };
	ip_mat* frame_mat = (ip_mat *)frame;
	ip_mat* frame_bak = (ip_mat *)background_image;
	int8_t **kernel = (int8_t **)log_kernel;
	background_substraction(SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT, background_image, frame, frame);
	LoG(LOG_KSIZE, kernel, frame_mat, &log_mat); 
	static recs blobs = {0, {}};

#ifdef __TESTING_HARNESS
	uint64_t before_blob_tsc = readTSC();
#endif

	find_blob(log_frame, &blobs, 0, RID, WHITE); 
	blob_filter(log_frame, &blobs, REC_MAX_AREA, REC_MIN_AREA); 

#ifdef __TESTING_HARNESS
	uint64_t before_tracking_tsc = readTSC();
#endif

	//TODO put people tracking here!!

#ifdef __TESTING_HARNESS
	uint64_t end_tsc = readTSC();
	uint64_t total_tsc = end_tsc - start_tsc;
	uint64_t people_detection_without_blob_tsc = before_blob_tsc - start_tsc;
	uint64_t tracking_tsc = end_tsc - before_tracking_tsc;
	if (max_tsc < total_tsc) {
		max_tsc = total_tsc;
	}
	printf("total instructions: %lu\nPeople detection without blobfilter: %lu\
			\nmax: %lu\nPeople Tracking: %lu\n", total_tsc, people_detection_without_blob_tsc, max_tsc, tracking_tsc);

	memcpy(th_frame, log_frame, SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT * sizeof(uint8_t));
	rec_num = blobs.count;
	memcpy(hrects, blobs.nodes, RECTS_MAX_SIZE * sizeof(rec));	
#endif

	//TODO get the correct values to return when the tracking gets implemented
	ip_result return_result = {0, 0, 0};
	return return_result;
}

/**
 * @brief substract the background from the current image, absolute different is used.
 * @param resolution the number of pixels in the image
 * @param background the background image
 * @param src the source image
 * @param dst the destination image
 */
void background_substraction(uint16_t resolution, ip_mat *background, ip_mat *src, ip_mat *dst)
{
	uint8_t *bframe = background->data;
	uint8_t *sframe = src->data;
	uint8_t *dframe = dst->data;

	for (uint16_t i = 0; i < resolution; i++)
	{
		int8_t temp = sframe[i] - bframe[i];
		dframe[i] = (temp > 0) ? temp : -temp;
	}
}

/**
 * @brief function for normal threshold(deprecated)
 * @param resolution the number of pixels in the image
 * @param thre threshold according to which to binarize the image
 * @param src source image
 * @param dst destination image
 */
void nthreshold(uint16_t resolution, uint8_t thre, ip_mat *src, ip_mat *dst)
{
	uint8_t *sframe = src->data;
	uint8_t *dframe = src->data;

	for (uint16_t i = 0; i < resolution; i++)
	{
		dframe[i] = (sframe[i] > thre) ? 255 : 0;
	}
}

/**
 * @brief convolution 
 * @param ksize the size of the kernel of the convolution
 * @param kernel the kernel of the convolution
 * @param m the data matrix e.g. the pixels
 * @param x the x coordinate of the pixel to be convolved 
 * @param y the y coordinate of the pixel to be convolved
 * @param padding the padding length
 * @return the convolution result
 */
static inline int16_t convolve(uint8_t ksize, int8_t **kernel, uint8_t *m, uint8_t x, uint8_t y, uint8_t padding)
{
	/* decide whether padding will be applied at location (x, y) */
	uint8_t p_x = (x - padding < 0) ? padding - x : 0;
	uint8_t p_y = (y - padding < 0) ? padding - y : 0;
	uint8_t i_y, i_x;
	/* the result of the convolution */
	int16_t sum = 0;
	/* the calculation should ignore the padding pixels and stop when exceed the boundary */
	for (uint8_t k_x = p_x, i_x = x; k_x < ksize && i_x < SENSOR_IMAGE_HEIGHT; ++k_x)
	{
		for (uint8_t k_y = p_y, i_y = y; k_y < ksize && i_y < SENSOR_IMAGE_WIDTH; ++k_y)
		{
			sum += m[i_x * SENSOR_IMAGE_WIDTH + i_y] * kernel[k_x][k_y];
			i_y++;
		}
		i_x++;
	}
	return sum;
}

/**
 * @brief apply the laplacian of gaussian operator on the image for blob detection
 * @param ksize the kernel size of the LOG operator
 * @param kernel the kernel of the convolution of the LOG operator
 * @param src the source image
 * @param dst the destination that will hold the binarized image
 */
void LoG(uint8_t ksize, int8_t **kernel, ip_mat *src, ip_mat *dst)
{
	/* decide the length of padding on each side of the image */
	uint8_t pad_length = ksize / 2;
	uint8_t *sframe = src->data;
	uint8_t *dframe = dst->data;
	/*printf("  ");
	  for (uint8_t i = 0; i < SENSOR_IMAGE_WIDTH; ++i) printf("%5d", i);
	  printf("\n");
	  uint8_t print_count = 0;
	  */
	uint8_t count = 0;
	int32_t gen_threshold = 0;
	int16_t convolve_values[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT] = {0};
	for (uint8_t i = 0; i < SENSOR_IMAGE_HEIGHT; ++i)
	{
		/*printf("%2d " ,print_count++);*/
		for (uint8_t j = 0; j < SENSOR_IMAGE_WIDTH; ++j)
		{
			/* convolve the kernel with each pixel of the image */
			int16_t c = convolve(ksize, kernel, sframe, i, j, pad_length);
			/* binarization */
			if (c > -config.threshold) dframe[i * SENSOR_IMAGE_WIDTH + j] = 0;
			else  {
				gen_threshold += c;
				convolve_values[i * SENSOR_IMAGE_WIDTH + j] = c;
				count++;
				
			}
			/*if ( c < -config.threshold ) 
			  printf("\033[1;31m%5d\033[0m", c);
			  else 
			  printf("%5d", c);*/
		}
		/*printf("\n");*/
	}
	if (count) {
		gen_threshold = (int32_t)(gen_threshold / count);
		printf("threshold is : %d\n", gen_threshold);

		for (uint8_t i = 0; i < SENSOR_IMAGE_HEIGHT; ++i) {
			for (uint8_t j = 0; j < SENSOR_IMAGE_WIDTH; ++j) {
				if (convolve_values[i * SENSOR_IMAGE_WIDTH + j] > 0.9 * gen_threshold)
					dframe[i * SENSOR_IMAGE_WIDTH +j] = 0;
				else dframe[i * SENSOR_IMAGE_WIDTH +j] = 255;
			}
		}
	}

	/*printf("-------------------------------------------------------------------\n");*/
}

/**
 * @brief This function will find all the blobs in the thresholded image and mark the pixels
 *        that belong to different blobs with a unique rid
 * @pram src the source image
 * @param blobs the struct that holds the result blobs
 * @param start_i the start index of the blob_counter
 * @param rid the start point of the rid
 * @param tvalue target value of which the BFS will searh for
 */
void find_blob(uint8_t *src, recs *blobs, uint8_t start_i, uint8_t rid, uint8_t tvalue)
{
	uint8_t *sframe = src;
	uint8_t blob_counter = start_i;
	queue pqueue = {0, 0, 0, {{0, 0}}};
	/* initialize the rid to be a unique number other than 0 and 255 */
	for (uint8_t i = 0; i < SENSOR_IMAGE_HEIGHT; ++i)
	{
		for (uint8_t j = 0; j < SENSOR_IMAGE_WIDTH; ++j)
		{
			/* when the pixel has not been visited by bfs(otherwise it will be marked as a rid */
			if (sframe[i * SENSOR_IMAGE_WIDTH + j] == tvalue)
			{
				/* j is the x coordinate and i is the y coordinate */
				pixel temp = {.x = j, .y = i};
				enqueue(&pqueue, temp);
				/* the first pixel of the blob should be marked to prevent infinite loop */
				sframe[i * SENSOR_IMAGE_WIDTH + j] = rid;
				/* do the BFS */
				blobs->nodes[blob_counter++] = bfs(sframe, &pqueue, rid++, tvalue);
			}
		}
	}
	blobs->count = blob_counter;
}

/**
 * @brief data structure operation, enqueue
 * @param q the queue to be operate on
 * @prarm p the pixel to be put in the back of the queue
 */
void enqueue(queue *q, pixel p)
{
	if (q->count >= QUEUE_SIZE)
	{
		return;
	}
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
pixel dequeue(queue *q)
{
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
 * @param rid the rid of the newly found blob
 * @param white the value of which is considered to be "white" i.e. that target of the searching
 */
rec bfs(uint8_t *frame, queue *q, uint8_t rid, uint8_t white)
{
	uint8_t min_x, min_y, max_x, max_y;
	/* initialize the min x and min y to be the number larger than the max coordinate(31)*/
	min_x = min_y = 33;
	max_x = max_y = 0;
	/* initialize the area of the blob to be 1, since there's at least 1 pixel in the blob */
	uint16_t area = 1;
	/* BFS */
	while (q->count)
	{
		pixel p = dequeue(q);
		/* extract the coordinate of the bounding box */
		min_x = (min_x < p.x) ? min_x : p.x;
		min_y = (min_y < p.y) ? min_y : p.y;
		max_x = (max_x > p.x) ? max_x : p.x;
		max_y = (max_y > p.y) ? max_y : p.y;
		/* loop over all of the 8 neighbours of a pixel*/
		for (int8_t k = -1; k < 2; ++k)
		{
			for (int8_t l = -1; l < 2; l++)
			{
				uint8_t x = p.x + k;
				uint8_t y = p.y + l;
				/* skip the pixel itself and when it goes out of bound */
				if ((l == 0 && k == 0) || x < 0 || y < 0 || x >= SENSOR_IMAGE_WIDTH || y >= SENSOR_IMAGE_HEIGHT)
				{
					continue;
				}
				if (frame[y * SENSOR_IMAGE_WIDTH + x] == white)
				{
					pixel temp = {p.x + k, p.y + l};
					enqueue(q, temp);
					/* mark the pixel that has been visited with the rid of the blob to prevent the algorithm from infinite looping */
					frame[y * SENSOR_IMAGE_WIDTH + x] = rid;
					area++;
				}
			}
		}
	}
	rec result = {min_x, min_y, max_x, max_y, rid, area};
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
void blob_filter(uint8_t *frame, recs *blobs, uint8_t amax, uint8_t amin)
{
	uint8_t num = blobs->count;
	for (uint8_t i = 0; i < num; ++i)
	{
		rec *temp = &blobs->nodes[i];
		/* filter out tiny blobs */
		if (temp->area < amin)
		{
			temp->rid = REC_IGNORE;
			continue;
		}
		if (temp->area > amax)
		{
			area_adjust(frame, temp, blobs, amax);
		}
	}
}

/**
 * @brief adjust the area of the blob for the sake of seprating oversized blobs
 * @param frame the bninarized image where the blobs resides
 * @param blob the blob to be adjusted
 * @param blobs the data structure that holds the blobs
 * @param amax the maximum area that a single blob could have
 */
void area_adjust(uint8_t *frame, rec *blob, recs *blobs, uint8_t amax)
{
	while (blob->area > amax)
	{
		erosion(frame, blob);
	}
	/* find the newly borned blobs after erosion */
	find_blob(frame, blobs, blobs->count, RID + blobs->count, blob->rid);
	/* set the old blob to be ignored */
	blob->rid = REC_IGNORE;
};

static inline uint8_t fit(uint8_t *frame, uint8_t rid, uint8_t x, uint8_t y, uint8_t kernel[ERO_KSIZE][ERO_KSIZE])
{
	uint8_t radius = ERO_KSIZE / 2;
	for (uint8_t i = 0; i < ERO_KSIZE; ++i)
	{
		for (uint8_t j = 0; j < ERO_KSIZE; ++j)
		{
			/* position the centroid of the kernel to the pixel to be examined */
			int8_t i_x = x + (i - radius);
			int8_t i_y = y + (j - radius);
			/* if it fits then continue, otherwise it does not fit */
			if ((i_x >= 0 && i_y >= 0) && ( !kernel[i][j] ||  (kernel[i][j] && frame[i_y * SENSOR_IMAGE_WIDTH + i_x] == rid)))
			{
				continue;
			}
			else
			{
				return 0;
			}
		}
	}
	/* the pixels fits if the kernel matches the pixels around it */
	return 1;
}

/**
 * @brief conduct erosion on the a specific blob
 * @param frame the binarized frame where the blobs resides 
 * @param blob the blob to be eroded
 */
void erosion(uint8_t *frame, rec *blob)
{
	/* initialize the kernel , TODO: choose a better kernel, could be hardcoded in the final product to reduce computation*/
	uint8_t kernel[ERO_KSIZE][ERO_KSIZE];
	for (uint8_t i = 0; i < ERO_KSIZE; ++i)
	{
		for (uint8_t j = 0; j < ERO_KSIZE; ++j)
		{
			kernel[i][j] = 1;
		}
	}

	uint16_t buf[blob->area];
	/* a counter for the pixels that does not fit */
	uint16_t ufcount = 0;
	for (uint8_t i = 0; i < SENSOR_IMAGE_HEIGHT; ++i)
	{
		for (uint8_t j = 0; j < SENSOR_IMAGE_WIDTH; ++j)
		{
			if (frame[i * SENSOR_IMAGE_WIDTH + j] != blob->rid)
			{
				continue;
			}
			/* determine whether the pixel fits */
			uint8_t f = fit(frame, blob->rid, j, i, kernel);
			/* if the pixel does not fit, then save the coordinate of the pixel */
			uint16_t temp = (uint16_t)j << 8 | i;
			if (!f)
			{
				buf[ufcount++] = temp;
			}
		}
	}
	/* set all pixels that does not fit to 0 */
	for (uint16_t i = 0; i < ufcount; ++i)
	{
		uint16_t temp = buf[i];
		uint8_t y = (uint8_t)temp;
		uint8_t x = temp >> 8 | 0;
		frame[y * SENSOR_IMAGE_WIDTH + x] = 0;
		/* update the area of the blob which is the criteria of when to stop the area adjustment */
		blob->area--;
	}
}

#ifdef __TESTING_HARNESS
// reads instructions
inline
uint64_t readTSC() {
	// _mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
	uint64_t tsc = __rdtsc();
	// _mm_lfence();  // optionally block later instructions until rdtsc retires
	return tsc;
}
#endif
