# People detection algorithms

In the current pipeline, we use the Laplacian of Gaussian (LoG) to detect people. However, several other algorithms were also explored by the team to detect people in the frame effectively and efficiently. Due to different reasons (mainly time constraints), they are not included in the pipeline. We list them here to make it an inspiration for other developers.


## 1. Gaussian mixture model

### Introduction
Modeling each pixel as a mixture of Gaussians and using an online approximation to update the model. The Gaussian distributions of the adaptive mixture model are then evaluated to determine which are most likely to result from a background process. Each pixel is classified based on whether the Gaussian distribution which represents it most effectively is considered part of the background model. To put it simple, each pixel will be classified as background or forebackground based on its previous value. In addition, instead of keeping only one previous value, we keep several gaussian distribution generated from the previous value, in that way if the newly pixel value is not covered in the tracked gaussian distribution (implies large change), it will be taken as foreground. 

### Advantages

Robust to lighting changes.

Robust to long-term scene changes.

### Constraints

Tests based on opencv's Background Subtractor MOG (implemented with Gaussian mixture model) found that it merged two people's blob when it comes very close to each other.

Computation intensive.

### Relevant materials
Adaptive background mixture models for real-time tracking  
http://www.ai.mit.edu/projects/vsam/Publications/stauffer_cvpr98_track.pdf

Link for OpenCV and its source code  
https://docs.opencv.org/master/d7/d7b/classcv_1_1BackgroundSubtractorMOG2.html
https://android.googlesource.com/platform/external/opencv/+/6acb9a7ea3d7564944e12cbc73a857b88c1301ee/cvaux/src/cvbgfg_gaussmix.cpp



# Background subtraction method comparasion

Background subtraction is the first phase in most people detection algorithms. In our pipeline, we simply take the first frame as the background and use each frame to subtract it. We choose to do in this way mainly because our PIR sensor can help in detecting whether there are people in the targeting area. However, we also explored some other algorithms, they could be used in this phase to properly retrieve the background image without the help of PIR sensor, they are listed below.

## 1. Eigen background

### Introduction

Frame based background model. The image is efficiently represented by a set of appearance models with few significant dimensions. The technique obtains good background with one eigen vector corresponding one largest eigen value. 

### Advantages
Robust to lighting switch.

Easy to solve the time of day problems (the background could be different in different times of a day)

### Constraints

Computation intensive (could be reduced by online incremental eigen analysis algorithm)

### Relevant materials
Adaptive eigen-background for object detection  
http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.99.4948&rep=rep1&type=pdf

Background subtraction with eigen background method using matlab  
https://pdfs.semanticscholar.org/1075/3b35f955885cc3c171c893145c875ab4e804.pdf?_ga=2.85050078.654736628.1596722918-388301654.1594697158


