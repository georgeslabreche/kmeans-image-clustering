# K-Means Image Clustering
K-Means image clustering that just works.
## Build
1. Initialize and update the Git submodules: `git submodule init && git submodule update`.
2. Compile with `make`. Can also compile for ARM architecture with `make TARGET=arm`.
## Getting Started
Compile the project with `make`. There are 4 modes: collect, train, predict, and live train.
 - **Mode 0 – train now**: train with existing images in given directory without persisting the training data in a .txt file. Optionally enable copying the input image files into 
 - **Mode 1 – collect**: save training data in a .csv file in case image files are transient. This file can be read later to build the clusters when enough training data has been collected.
 - **Mode 2 – train**: read training_data.csv and build clusters. Write centroids in a .txt file in `kmeans/<id>/centroids.txt`.
 - **Mode 3 – predict**: calculate distances from each centroid and apply nearest cluster to the image ipunt.
cluster/label directories.

### Train Now (Mode 0)

A total of 5 arguments are expected:
 - Mode id i.e., the "train now" mode in this case.
 - K number of clusters.
 - Flag indicating whether or not the thumbnails should be copied over to clustered directories.
 - Image directory where the images to be clustered are located.
 - Cluster directory where the images will be copied to (if the flag to do so is set to true).

**TODO: Save centroids file.**

Example:
```bash
./K_Means 0 4 1 examples/earth/ kmeans/clustered/earth/
```

Also try:
```bash
./K_Means 0 4 1 examples/edge/ kmeans/clustered/edge/
```

And:
```bash
./K_Means 0 4 1 examples/bad/ kmeans/clustered/bad/
```

### Collect (Mode 1)

A total of 3 arguments are expected:
 - Mode id i.e., the "collect" mode in this case.
 - Image directory where the images to be clustered are located.
 - CSV file path where training data will be written to.

 Example:
 ```bash
 ./K_Means 1 examples/earth/ kmeans/earth/training_data.csv
 ```

### Train (Mode 2)
_Not yet implemented._

### Predict (Mode 3)
_Not yet implemented._
