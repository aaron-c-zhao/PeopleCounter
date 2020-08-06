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


  


