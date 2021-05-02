# K-Means Image Clustering
K-Means image clustering that just works.
## Build
1. Initialize and update the Git submodules: `git submodule init && git submodule update`.
2. Compile with `make`. Can also compile for ARM architecture with `make TARGET=arm`.
## Getting Started
Compile the project with `make`. There are 4 modes: collect, train, predict, and live train.
 - **Mode 0 – collect**: save training data in a .txt file in `kmeans/<id>/training_data.txt`.
 - **Mode 1 – train**: read training_data.txt and build clusters. Write centroids in a .txt file in `kmeans/<id>/centroids.txt`.
 - **Mode 2 – predict**: calculate distances from each centroid and apply nearest cluster to the image ipunt.
 - **Mode 3 – live train**: train with existing images in given directory without persisting the training data in a .txt file. Optionally enable copying the input image files into cluster/label directories.

### Collect (Mode 0)
_Not yet implemented._

### Train (Mode 1)
_Not yet implemented._

### Predict (Mode 2)
_Not yet implemented._
### Live Train (Mode 3)

A total of 5 arguments are expected:
 - Mode id i.e., the "live train" mode in this case.
 - K number of clusters.
 - Flag indicating whether or not the thumbnails should be copied over to clustered directories.
 - Image directory where the images to be clustered are located.
 - Cluster directory where the images will be copied to (if the flag to do so is set to true).


If enabled to do so, the input images are copied into cluster folders in `kmeans/clustered/`:
```bash
./K_Means 3 4 1 examples/earth/ kmeans/clustered/earth/
```

Also try:
```
./K_Means 3 4 1 examples/edge/ kmeans/clustered/edge/
```

And:
```
./K_Means 3 4 1 examples/bad/ kmeans/clustered/bad/
```
