\mainpage

# People counter

**C IS THE MOST BEAUTIFUL LANGUAGE, ANY OTHER LANGUAGE SUCKS!**

This repo consist of 2 main parts, the people counting module and the test harness to run it. The people counter module in people_counter.c is supposed to be hardware independent and ready to be implemented on an embedded system. Because the team didn't have access to the hardware and firmware to test the module, a testing harness was created. This harness will run through recorded sensor data in the test footages saved in ./data folder and will call the module on each frame, according to the api. This way we could test and improve the pipeline.

## TESTING HARNESS
This program was made to run and test the image processing pipeline of the people counter project. 

### HOW TO INSTALL AND RUN THE HARNESS

#### SUPPORTED ENVIRONMENT
* Mac Darwin
* Linux

#### DEPENDENCIES
1. CMake: > 3.15
2. OpenCV
* GCC 4.4.x or later
* Git
* GTK+2.x or higher
* pkg-config
* Python2.6 or later
* ffmpeg or libav development packages: libavcodec-dev, libavformat-dev, libswscale-dev
3. Doxygen 1.8.15(optional)


### INSTRUCTIONS

#### CONFIGURATION

After cloning this repository onto your local machine, first you need to configure the harness code and header file of the pipeline which could be accomplished by modify these two files:
* harness_config.json
	* parameters: the parameters of the ip pipeline that will affect its effectiveness(you can play around with these four parameters(with slide bars or change the values in harness_config.json) to achieve the best performance with your current setup, ML could be used here)
		* kernel_1: gaussian blur kernel 1. Range 1 ~ 255, only odd numbers. Default value 5.(NOTE: this parameter could **only** be changed at the first black frame when using slide bars. Anywhere after that frame will crash the program)
		* threshold: the threshold applied when binarize the image. Range 1 ~ 20000. Default value 2500
		* max_area: the maximum area of a blob that is allowed, used as a trigger of updating threshold to detect overly large blobs. Range 1 ~ 255. Default value 18
		* sensitivity: it controls the sensitivity of the adaptive threshold, in other words, it's the coefficient applied to the average pixel value. Range 1 ~ 255. Default value 9
	* harness_setup: configurations of the harness code, this won't affect the pipeline
		* width: image width which will affect the image shown in the window
		* height: image height which will affect the image shown in the window
		* llimit: the lower limit applied when map the floating point temperature figure to 8bit grey value
		* hlimit: the upper limit applied when map the floating point temperature figure to 8bit grey value
		* frame_rate: the frame rate of the video, this combined with the FRAME_RATE seeting in header_config.txt will decide the step of the image pointer.
* header_config.txt(NOTE: all the configurations here should be relatively fixed. Unless when the hardware configurations changes.)
	* SENSOR_IMAGE_WIDTH: the width of the image to be processed by the pipeline, default value 32
	* SENSOR_IMAGE_HEIGHT: the height of the image to be processed by the pipeline, default valut 24
	* FRAME_RATE: the frame rate that the pipeline will work with. NOTE: this figure could be lower than the actual frame rate that the video is recorded with, which will result in skip one or several frames in between the two frames that are being processed by the pipeline. 
	* RECTS_MAX_SIZE: the maximum array size of the rects array
	* QUEUE_SIZE: the maximum array size of the queue for breach first search
	* CT_MAX_DISAPPEARED: the maximum frames in which a blob is allowed to disappear, after that the BID(blob id) will be reallocated
	* CT_MAX_DISTANCE: the maximum distance that a blob is allowed to jump between frames
	* TRACKABLE_OBJECT_MAX_SIZE: the maximum number of blobs or people that the pipeline could trace at the same time

#### RUN THE PIPELINE LINE

If you agree with the current configuration, then you can run the pipeline by following the instructions below:
1. change directory(cd) into the source folder of the project(PeopleCounter/)
2. run the script make.sh(./make.sh)
> $ ./make.sh
3. run the executable(PeopleCounter) with the argument of the path of the json file(./PeopleCounter ../data/mlx90640/vertical/.....)
> $ ./PeopleCounter.executable ./data/mlx90640/vertical/Two_people_walking_side_by_side_25c_10_17_44.json

#### INTERACTION WITH THE HARNESS 

The harness will run the pipeline while displaying the images frame by frame. Press any key to go to the next frame. Press Ctrl+C on the terminal to exit.

#### OUTPUT OF THE HARNESS

The harness will output the following:
* list of currently tracked objects, including its ID, centroid position and disappeared frame count
* current frame number
* the number of people that went up and down in this frame
* the estimate count of people in the room

### Documentation
Doxygen is used to document the code in this repo. It will generate an interactive html documentation. The generated html can be found in "./docs/html/html/index.html". This can also be regenerated by running the doxygen command in the root folder.

## RECOMMANDATIONS FOR FURTHER DEVELOPING

In @ref trackingResearch and @ref detectionResearch all the possible research that could still be done is listed.

### CODE OPTIMAZATION
1. Hardcode the LoG kernel
For the sake of easy testing with new parameters, the LoG kernel is generated by the harness code each time with the kernel size that have been set. In the final product, it should be hardcoded into the pipeline to reduce the computation.
2. Use the inplace calculation
The functions in the pipleline are designed to be able to do inplace calculations. So no extra data structure is needed other than the one that is passed in by the firmware. But now, for displaying the intermedia result, an extra log_mat was introduced which should be replaced by the frame_mat to reduce the space requirement.
3. Ohters
During the processing of the image, each pixel will be iterated several times. But not all of the iterations are necessary, due to the time constraint, we can not optimize it to the optimal. So coners could be cut there. 

### ALGORITHM OPTIMIZATION
1. People detection
The pipeline now uses the Laplacian of Gaussian algorithm to extract and detect blobs which is twice efficient than the previous thresholding + border tracing. But algorithms like [Watershed](./docs/watershed.pdf), and [Gaussian mixture](./docs/people_detection_research.md) have the potential to be more efficient.
2. People tracking
The algorithm currently being used to track blobs is the nearest neighbour method, which is a basic tracking method. More advanced methods like [Kalman filter](./docs/people_tracking_research.md) will be more robust than the current one. But on the other hand, the robustness comes with a price that it will add a considerable amount of complexity to the system.
3. Background substraction
For now the pipeline takes the first frame as the background (in the final product, this should be the first frame of each time the device is waken up by the PIR). This way has a fatal flaw which is that it relays on the assumption that the Melexis sensor could be waken up early enough that there's still no person in the FOV. To solve this [Eigen backgound](./docs/people_detection_research.md), [Morphological background substraction](./docs/people_detection.pdf) might be worthwhile to look into.
