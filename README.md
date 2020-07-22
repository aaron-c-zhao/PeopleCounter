# PEOPLE COUNTER IP PIPELINE TESTING HARNESS
This program was made to run and test the image processing pipeline of the people counter project. 

## HOW TO INSTALL AND RUN THE HARNESS

### SUPPORTED ENVIRONMENT
* Mac Darwin
* Linux

### DEPENDENCIES
1. CMake: > 3.15
2. OpenCV
* GCC 4.4.x or later
* Git
* GTK+2.x or higher
* pkg-config
* Python2.6 or later
* ffmpeg or libav development packages: libavcodec-dev, libavformat-dev, libswscale-dev


### INSTRUCTIONS

#### CONFIGURATION

After clone this repository onto your local machine, first you need to configure the harness code and header file of the pipeline which could be accomplished by modify these two files:
* harness_config.json
	* parameters: the parameters of the ip pipeline that will affect its affectiveness
		* kernel_1: gaussian blur kernel 1, default value 5
		* kernel_2: gaussian blur kernel 2, default value 1
		* kernel_3: gaussian blur kernel 3, default value 1
		* threshold: the threshold applied when binarize the image, defaule value 32
		* blob_width_min: the minimum blob width used to filter out the noise, default value 0(ineffective)
		* blob_height_min: the minimum blob height used to filter out the noise, default value 0(ineffective)
		* updated_threshold: the threshold applied when threshold updating is triggered, default value 84
		* max_area: the maximum area of a blob that is allowed, used as a trigger of updating threshold to detect overly large blobs, default value 18
	* harness_setup: configurations of the harness code, this won't affect the pipeline
		* width: image width which will affect the image shown in the window
		* height: image height which will affect the image shown in the window
		* llimit: the lower limit applied when map the floating point temperature figure to 8bit grey value
		* hlimit: the upper limit applied when map the floating point temperature figure to 8bit grey value
		* frame_rate: the frame rate of the video, this combined with the FRAME_RATE seeting in header_config.txt will decide the step of the image pointer.
* header_config.txt
	* SENSOR_IMAGE_WIDTH: the width of the image to be processed by the pipeline, default value 32
	* SENSOR_IMAGE_HEIGHT: the height of the image to be processed by the pipeline, default valut 24
	* CT_MAX_DISAPPEARED: the maximum frames in which a blob is allowed to disappear, after that the BID(blob id) will be reallocated
	* CT_MAX_DISTANCE: the maximum distance that a blob is allowed to jump between frames
	* RECTS_MAX_SIZE: the maximum size that a blob is allowd to be regared as a single blob, values above that indicates that the blob consists of two or more small blobs that merging together
	* TRACKABLE_OBJECT_MAX_SIZE: the maximum number of blobs that the pipeline could trace
	* FRAME_RATE: the frame rate that the pipeline will work with. 



#### RUN THE PIPELINE LINE

If you agree with the current configuration, then you can run the pipeline by following the instructions below:
1. change directory(cd) into the source folder of the project(PeopleCounter/)
2. run the script make.sh(./make.sh)
3. cd into the build/ folder. 
4. run the executable(PeopleCounter) with the argument of the path of the json file(./PeopleCounter ../data/mlx90640/vertical/.....)

#### INTERACTION WITH THE HARNESS 

The harness will run the pipeline while displaying the images frame by frame. (TODO: add auto play) Press any key to go to the next frame. Press Ctrl+C to exit.

#### OUTPUT OF THE HARNESS

The harness will output the frame number of the current frame and the result produced by the pipeline. (TODO: add intermedia result inspection)


