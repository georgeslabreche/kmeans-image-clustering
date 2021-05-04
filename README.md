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

A total of 4 or 5 arguments are expected:
 - Mode id i.e., the "train now" mode in this case.
 - K number of clusters.
 - Output CSV file where the cluster centroids will be written to.
 - Image directory where the images to be clustered are located.
 - (Optional) Cluster directory where the images will be copied to.

Example:
```bash
./K_Means 0 4 kmeans/centroids_earth.csv examples/earth/
```

Also try:
```bash
./K_Means 0 4 kmeans/centroids_earth.csv examples/earth/ kmeans/clustered/earth/
```

Other example image folders to try:
- examples/edge/
- examples/bad/

### Collect (Mode 1)

A total of 3 arguments are expected:
 - Mode id i.e., the "collect" mode in this case.
 - Image directory where the images to be clustered are located.
 - CSV file path where training data will be written to.

 Example:
```bash
./K_Means 1 examples/earth/ kmeans/training_data_earth.csv
```

### Train (Mode 2)

A total of 4 arguments are expected:
 - Mode id i.e., the "train" mode in this case.
 - K number of clusters.
 - CSV file path of the training data.
 - Output CSV file where the cluster centroids will be written to.

Example:
```bash
./K_Means 2 4 kmeans/training_data_earth.csv kmeans/centroids_earth.csv
```
### Predict (Mode 3)

A total of 3 arguments are expected:
 - Mode id i.e., the "predict" mode in this case.
 - File path of the image to label.
 - CSV file path of the centroid file used to determine which label to apply to the given image.

Also try:
```bash
./K_Means 3 examples/earth/img_msec_1606835961336_2_thumbnail.jpeg kmeans/centroids_earth.csv
./K_Means 3 examples/earth/img_msec_1609324358567_2_thumbnail.jpeg kmeans/centroids_earth.csv
./K_Means 3 examples/earth/img_msec_1606876023471_2_thumbnail.jpeg kmeans/centroids_earth.csv
./K_Means 3 examples/earth/img_msec_1609362399310_2_thumbnail.jpeg kmeans/centroids_earth.csv
```