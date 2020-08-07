/** //TODO add more information? author, version, details, api etc?
 ******************************************************************************
 * @file           : people_counter.c
 * @brief          : Implementation of the image processing pipeline
 ******************************************************************************
 **/

#include "people_counter.h"
#include "string.h"

/** @brief the value for a white pixel */
#define WHITE 255
/** @brief the start number of the rid */
#define RID 42

/** @brief struct that keeps the information of configurations */
extern ip_config config;

// exclude this what it isn't run from the harness
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
ip_result people_tracking(recs *);
void deleteOldObjects(object_list *);
void bubbleSort(object_rect_pair *, uint8_t);

/** @brief list of objects (people) used for people tracking */
static object_list objects = {0, 0, {}};

/** 
 * @brief the image processing pipeline, including people detection and people tracking.
 * @details we first subtract the background from each frame to get the foreground image, then apply Laplacian of Gaussian to detect people. 
 *          In the end, centroid tracking algorithm is used to track people.
 * @param frame frame the image to be processed
 * @param background_image the background image chosed to do background subtraction
 * @param log_kernel the kernel of the convolution of the LOG operator in people detection
 * @return ip_result contains object length and the count of up and down
 */
ip_result IpProcess(void *frame, void *background_image, void *log_kernel)
{
#ifdef __TESTING_HARNESS
    uint64_t start_tsc = readTSC();
    static uint64_t max_tsc = 0;
#endif

    uint8_t log_frame[SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT] = {0};
    ip_mat log_mat = {.data = log_frame};
    ip_mat *frame_mat = (ip_mat *)frame;
    ip_mat *frame_bak = (ip_mat *)background_image;
    int8_t **kernel = (int8_t **)log_kernel;

	/* subtract the background image from the frame */
    background_substraction(SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT, frame_bak, frame_mat, frame_mat);

	/* apply Laplacian of Gaussian on the substracted frame */
    LoG(LOG_KSIZE, kernel, frame_mat, &log_mat);
    static recs blobs = {0, {}};

#ifdef __TESTING_HARNESS
    uint64_t before_blob_tsc = readTSC();
#endif

    /* find the blobs in the thresholded image */
    find_blob(log_frame, &blobs, 0, RID, WHITE);

    /* filter out the blob which is out of the area range */
    blob_filter(log_frame, &blobs, REC_MAX_AREA, REC_MIN_AREA);

#ifdef __TESTING_HARNESS
    uint64_t end_tsc = readTSC();
    uint64_t total_tsc = end_tsc - start_tsc;
    uint64_t without_blob_tsc = before_blob_tsc - start_tsc;
    if (max_tsc < total_tsc)
    {
        max_tsc = total_tsc;
    }
    /* printf("total instructions: %llu\nwithout blobfilter: %llu\nmax: %llu\n", total_tsc, without_blob_tsc, max_tsc); */

    memcpy(th_frame, log_frame, SENSOR_IMAGE_WIDTH * SENSOR_IMAGE_HEIGHT * sizeof(uint8_t));
    rec_num = blobs.count;
    memcpy(hrects, blobs.nodes, RECTS_MAX_SIZE * sizeof(rec));
#endif

    /* apply centroid tracking algorithm on the blobs detected */
    ip_result return_result = people_tracking(&blobs);

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
	/* get the pixel values for the background image */
	uint8_t *bframe = background->data;
	/* get the pixel values for the frame */
	uint8_t *sframe = src->data;
    /* get the pixel values for the substracted image */
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
	/* get the pixel values for the source image */
	uint8_t *sframe = src->data;
	/* get the pixel values for the destination image */
	uint8_t *dframe = src->data;
    /* apply threshold on the source image and put the result to the destination image */
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
		// printf("threshold is : %d\n", gen_threshold);

		for (uint8_t i = 0; i < SENSOR_IMAGE_HEIGHT; ++i) {
			for (uint8_t j = 0; j < SENSOR_IMAGE_WIDTH; ++j) {
				if (convolve_values[i * SENSOR_IMAGE_WIDTH + j] > (config.sensitivity * gen_threshold) / 10)
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
 * @param src the source image
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
 * @param p the pixel to be put in the back of the queue
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
 * //TODO: add a error handling to deal with empty queue
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
 * @param frame the binarized image where the blobs resides
 * @param blob the blob to be adjusted
 * @param blobs the data structure that holds the blobs
 * @param amax the maximum area that a single blob could have
 */
void area_adjust(uint8_t *frame, rec *blob, recs *blobs, uint8_t amax)
{
	//TODO coment for code readability
	while (blob->area > amax)
	{
		erosion(frame, blob);
	}
	/* find the newly borned blobs after erosion */
	find_blob(frame, blobs, blobs->count, RID + blobs->count, blob->rid);
	/* set the old blob to be ignored */
	blob->rid = REC_IGNORE;
};

/** //TODO add info about fit
 * @brief determine whether the structuring element fit inside the shape
 * @param frame 
 * @param rid 
 * @param x 
 * @param y 
 * @param kernel 
 * @return uint8_t 
 */
static inline uint8_t fit(uint8_t *frame, uint8_t rid, uint8_t x, uint8_t y, uint8_t kernel[ERO_KSIZE][ERO_KSIZE])
{
	//TODO add comment for code readability
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

/**
 * @brief perform people tracking given the blobs of people in every frame
 * @param rects list of blobs' bounding boxes
 * @return the list of objects being tracked and the number of people that went up and down.
 */
ip_result people_tracking(recs *original_rects)
{
    uint8_t total_up = 0, total_down = 0;

    /* create a temporary copy of the rectangles filtering out the ones to be ignored */
    uint8_t rec_length = 0;
    rec rects[original_rects->count];
    for(uint8_t i = 0; i < original_rects->count; ++i)
    {
        if(original_rects->nodes[i].rid != REC_IGNORE)
        {
            rects[rec_length++] = original_rects->nodes[i];
        }
    }

    /* if there are no blobs in the frame, then increase the count of disappeared frames of every tracked object */
    if (rec_length == 0)
    {
        /* increase disappered count of every object */
        for (uint8_t i = 0; i < objects.length; ++i)
        {
            ++objects.object[i].disappeared_frames_count;
        }

        deleteOldObjects(&objects);

        return ((ip_result){objects.length, 0, 0});
    }

    /* convert bounding boxes to their centroid points */
    pixel input_centroids[rec_length];

    for (uint8_t i = 0; i < rec_length; ++i)
    {
        /* use the bounding box coordinates to derive the centroid */
        input_centroids[i] = (pixel){(uint8_t)((rects[i].min_x + rects[i].max_x) >> 1),
                                     (uint8_t)((rects[i].min_y + rects[i].max_y) >> 1)};
    }

    /* if no objects are being tracked, then the new centroids are all new objects */
    if (objects.length == 0)
    {
        /* register all centroids as new objects */
        for (uint8_t i = 0; i < rec_length; ++i)
        {
            objects.object[objects.length] = (object){objects.next_id, input_centroids[i], 0};

            ++objects.length;
            ++objects.next_id;
        }

        return ((ip_result){objects.length, 0, 0});
    }

    /* create an object to new centroids map (matrix) as a 1D array */
    uint8_t dv_length = objects.length * rec_length;
    object_rect_pair distance_vector[dv_length];

    /* calculate the squared euclidean distance between each object and new centroid */
    for (uint8_t i = 0; i < objects.length; ++i)
    {
        for (uint8_t j = 0; j < rec_length; ++j)
        {
            int8_t delta_x = objects.object[i].centroid.x - input_centroids[j].x;
            int8_t delta_y = objects.object[i].centroid.y - input_centroids[j].y;

            /* calculate squared euclidean distance (skip the square root, because we just need to sort based on it) */
            uint16_t distance = delta_x * delta_x + delta_y * delta_y;

            distance_vector[i * rec_length + j] = (object_rect_pair){distance, i, j};
        }
    }

    /* sort the object-input centroid pairings based on their distance */
    bubbleSort(distance_vector, dv_length);

    /* use boolean flags in an integer to mark if an object or rect index has already been used */
    uint16_t objects_used = 0;
    uint16_t rects_used = 0;

    /* iterate at most max(#objects, #input_centroids) times */
    for (uint8_t i = 0; i < dv_length; ++i)
    {
        /* if the current pair's distance is higher than the max distance, then this and all following pairs are too far away */
        if (distance_vector[i].distance > CT_MAX_DISTANCE * CT_MAX_DISTANCE)
        {
            break;
        }

        /* if the input centroids has already been assigned to a previous object, then skip this object */
        if (objects_used & (1 << distance_vector[i].object_index) || rects_used & (1 << distance_vector[i].rect_index))
        {
            continue;
        }

        /* flag this object as used */
        objects_used = objects_used | (1 << distance_vector[i].object_index);
        rects_used = rects_used | (1 << distance_vector[i].rect_index);

        uint8_t object_id = distance_vector[i].object_index;
        uint8_t rect_id = distance_vector[i].rect_index;

        /* check if this object has crossed the middle line, if so increase the count depending on the direction */
#ifdef __ORIENTATION_VERTICAL
        if (objects.object[object_id].centroid.x < (SENSOR_IMAGE_WIDTH >> 1) && input_centroids[rect_id].x >= (SENSOR_IMAGE_WIDTH >> 1))
        {
            ++total_up;
        }
        else if (objects.object[object_id].centroid.x >= (SENSOR_IMAGE_WIDTH >> 1) && input_centroids[rect_id].x < (SENSOR_IMAGE_WIDTH >> 1))
        {
            ++total_down;
        }
#else
        if (objects.object[object_id].centroid.y < (SENSOR_IMAGE_HEIGHT >> 1) && input_centroids[rect_id].y >= (SENSOR_IMAGE_HEIGHT >> 1))
        {
            ++total_up;
        }
        else if (objects.object[object_id].centroid.y >= (SENSOR_IMAGE_HEIGHT >> 1) && input_centroids[rect_id].y < (SENSOR_IMAGE_HEIGHT >> 1))
        {
            ++total_down;
        }
#endif

        /* update the object centroid to the assigned closest input centroid */
        objects.object[object_id].centroid = input_centroids[distance_vector[i].rect_index];

        /* reset the disapperead counter of that object */
        objects.object[object_id].disappeared_frames_count = 0;
    }

    /* increase disappeared count of unused objects */
    if (objects_used < (1 << objects.length) - 1)
    {
        uint16_t temp = objects_used;
        uint8_t old_length = objects.length;
        for (uint8_t i = 0; i < old_length; ++i)
        {
            if (!(temp & 1))
            {
                ++objects.object[i].disappeared_frames_count;
            }
            temp = temp >> 1;
        }

        deleteOldObjects(&objects);
    }

    /* register all the unused rectangles as new objects. */
    if (rects_used < (1 << rec_length) - 1)
    {
        uint16_t temp = rects_used;
        for (uint8_t i = 0; i < rec_length; ++i)
        {
            if (!(temp & 1))
            {
                objects.object[objects.length] = (object){objects.next_id, input_centroids[i], 0};

                ++objects.length;
                ++objects.next_id;
            }
            temp = temp >> 1;
        }
    }

    return ((ip_result){objects.length, total_up, total_down});
}

/**
 * @brief deletes all objects that have disappeared for more than (CT_MAX_DISAPPEARED)
 * @param objects the list of objects
 * TODO: remove parameter objects since objects is static globally.
 */
void deleteOldObjects(object_list *objects)
{
    uint8_t temp = 0;
    uint8_t original_size = objects->length;
    for (uint8_t i = 0; i < original_size; ++i)
    {
        /* decrease objects' length if has disappeared for more than CT_MAX_DISAPPEARED and skip this object */
        if (objects->object[i].disappeared_frames_count > CT_MAX_DISAPPEARED)
        {
            --objects->length;
            continue;
        }
        /* if index i is higher than temp, then it means that some objects were deleted, therefore we need to swap the
        two objects to fill in the empty spot */
        if (i > temp)
        {
            objects->object[temp].id = objects->object[i].id;
            objects->object[temp].disappeared_frames_count = objects->object[i].disappeared_frames_count;
            objects->object[temp].centroid = objects->object[i].centroid;
        }
        ++temp;
    }
}

/**
 * @brief perform bubble sort on the array of object-rectangle pairs
 * @param array array of object-rectangle pairings
 * @param length length of the array
 */
void bubbleSort(object_rect_pair *array, uint8_t length)
{
    uint8_t i, j;
    for (i = 0; i < length - 1; ++i)
    {
        /* Last i elements are already in place */
        for (j = 0; j < length - i - 1; ++j)
        {
            if (array[j].distance > array[j + 1].distance)
            {
                object_rect_pair temp = array[j];
                array[j] = array[j + 1];
                array[j + 1] = temp;
            }
        }
    }
}

/**
 * @brief returns the memory address of the list of objects
 * @return memory address of the list of objects
 */
const object *getObjectsAddress()
{
    return objects.object;
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
