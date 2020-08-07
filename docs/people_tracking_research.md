# Options in centroid tracking algorithm

In the current pipeline, we use the Centroid tracking algorithm to track people. It associates blobs in consecutive frames based on their distance (the blobs with the smallest distance will be assigned the same ID). We take a greedy strategy to sort the distance matrix (the distance between n tracked objects and m newly detected rectangles) from the smallest to biggest and associate relevant blobs. It works perfectly when the distance matrix is small but when it comes to large distance matrix, it might not be the best option. Here we list several other algorithms we did research on which might produce better association. However, due to time constraints, we didn't implement all of them.


## 1. SoftAssign

### Introduction
The aim of the softassign algorithm is straightforward: retrieve the best match pairs with the smallest sum of distance. It achieves that by keeping normalizing values in each row and column until it coverges. In the end, the optimal pair will be the maximum value in each row and column.

### Advantages
Guarantee the optimal pairs will  be found with any distance matrix.

### Relevant materials
Softmax to Softassign: Neural Network Algorithms for Combinatorial Optimization  
[https://www.researchgate.net/publication/2458489_Softmax_to_Softassign_Neural_Network_Algorithms_for_Combinatorial_Optimization](https://www.researchgate.net/publication/2458489_Softmax_to_Softassign_Neural_Network_Algorithms_for_Combinatorial_Optimization)

The SoftAssign Procrustes Matching algorithm  
[https://www.cise.ufl.edu/~anand/pdf/ipsprfnl.pdf](https://www.cise.ufl.edu/~anand/pdf/ipsprfnl.pdf)

## 2. Network flow

### Introduction
 It is easy to associate the optimal pairing problem in our scenario to the Biparitite Matching problem in the network flow. With tracked objects as one set and newly detected rectangles as the other set, our job is to make a one-by-one association between these two sets. The thing needs to be noticed is that each edges between the two set would have some weight. The approach to solve this problem is the traditional Bellman-Ford algorithm, it can give us the match with the maximum weight. 

### Advantages
Find the optimal pairs with relative less time.

### Constraints
It can only work when the numeber of tracked objects equals the number of newly found rectangles. Since there are many cases when those two numbers are not equal, this algorithm was ignored. 


# Other tracking algorithms explored
Besides centroid tracking algorithm, we also explored several other alternative algorithms in people tracking, we list them here:

## 1. Kalman filter

### Introduction

Apart from centroid tracking algorithm, another algorithm we have looked into is the Kalman filter, it is an algorithm uses a series of measurements observed over time and produces estimates of unknown variables. It mainly consists of 3 phases, prediction, kalman gain and correction. In our scenario, we will predict the position of a blob based on its previous positions, in that way, we can easily associate the blob with the correct ID.

### Advantages
Be able to predict a blob's position when it disappears, making our pipeline more robust.

Better accuracy.

### Constraints
It is a rather complicated algorithm.

### Current progress

The prediction and correction were already finished. But due to the time constraint, the kalman gain was not completely implemented. 

### Relevant materials
Wikipedia page  
[https://en.wikipedia.org/wiki/Kalman_filter](https://en.wikipedia.org/wiki/Kalman_filter)

The Kalman Filter: An algorithm for making sense of fused sensor insight  
[https://towardsdatascience.com/kalman-filter-an-algorithm-for-making-sense-from-the-insights-of-various-sensors-fused-together-ddf67597f35e](https://towardsdatascience.com/kalman-filter-an-algorithm-for-making-sense-from-the-insights-of-various-sensors-fused-together-ddf67597f35e)
  





