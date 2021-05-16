# K-Means Image Clustering
K-Means image clustering that just works. 
## OPS-SAT
The `opssat` branch includes customizations meant for the [SmartCam App](https://github.com/georgeslabreche/opssat-smartcam) on-board the [OPS-SAT Spacecraft](https://opssat1.esoc.esa.int/). These changes add filtering capabilities to selectively process multiple image file types during training and clustering operations. Although customized for the OPS-SAT SmartCam, the code can be re-used for clustering with the same filtering requirements.
### Particularity
There is a particularity with image acquisition by the OPS-SAT SmartCam in that the produced images file types for a single picture do not all have the same name. The name of the JPEG thumbnail file differs from the PNG and IMS_RGB files in that it is affixed with "_thumbnail", for instance:
 - img_msec_1621184871142_2_thumbnail.jpeg
 - img_msec_1621184903224_2.png
 - img_msec_1621184903224_2.ims_rgb

The `#define` directives for `BUILD_FOR_OPSSAT_SMARTCAM` and `THUMBNAIL_AFFIX_LENGTH` are set to take into account this particularity and `BUILD_FOR_OPSSAT_SMARTCAM` should be set to `0` when building the project for use-cases outside the context of the OPS-SAT SmarCam.

## Build
1. Initialize and update the Git submodules: `git submodule init && git submodule update`.
2. Compile with `make`. Can also compile for ARM architecture with `make TARGET=arm`.
## Getting Started
Compile the project with `make`. There are 4 modes: collect, train, predict, and live train.
 - **Mode 0 – train now**: train with existing images in given directory without persisting the training data in a file. Optionally enable copying the input image files into 
 - **Mode 1 – collect**: save training data in a CSV file in case image files are transient. This file can be read later to build the clusters when enough training data has been collected.
 - **Mode 2 – train**: read training_data CSV file and build clusters. Write centroids in CSV file at the given file path.
 - **Mode 3 – predict**: calculate distances from each cluster centroid and return the nearest cluster id for the image input.
 - **Mode 4 – batch predict**: calculate distances from each cluster centroid for each image in a given directory and **moves** the images into their respective cluster/label directory.

### Train Now (Mode 0)

A total of 5 or 7 arguments are expected:
 - Mode id i.e., the "train now" mode in this case.
 - K number of clusters.
 - Output CSV file where the cluster centroids will be written to.
 - Image directory where the images to be clustered are located.
 - Comma separated list of image file types to use as training data.
 - Optional: 
    - Cluster directory where the images will be copied to.
    - Comma separated list of image file types to move to their respective cluster/label directories.

**Example \#1:** train a model without copying the image files into their respective cluster folders.
```bash
./K_Means 0 4 kmeans/centroids_earth.csv examples/earth/ jpeg
```

**Example \#2**:
 - Each image in the `examples/earth/` directory is saved in three different file formats: JPEG, PNG, and IMS_RGB.
 - We want to train a model using only the JPEG images types as training data.
 - When clustering, we want to move all image types into their respective cluster folder in the `kmeans/clustered/earth/` direcory.
 
```
./K_Means 0 4 kmeans/centroids_earth.csv examples/earth/ jpeg kmeans/clustered/earth/ jpeg,png,ims_rgb
```

**Example \#3:** train with JPEG image file types but when clustering only move the PNG image type into their respective cluster folder in the `kmeans/clustered/earth/` directory.
```
./K_Means 0 4 kmeans/centroids_earth.csv examples/earth/ jpeg kmeans/clustered/earth/ png
```

**Example \#4:** train a model using both JPEG and PNG images types as training images but when clustering only move IMS_RGB image types into their respective cluster folder in the `kmeans/clustered/earth/` direcory. 
```
./K_Means 0 4 kmeans/centroids_earth.csv examples/earth/ jpeg,png kmeans/clustered/earth/ ims_rgb
```

Other example image folders to try:
- examples/edge/
- examples/bad/

### Collect (Mode 1)

A total of 4 arguments are expected:
 - Mode id i.e., the "collect" mode in this case.
 - Image directory where the images to be clustered are located.
 - Comma separated list of image file types to use as training data.
 - CSV file path where training data will be written to.

**Example \#1:** collect image training from JPEG images types.
```
./K_Means 1 examples/earth/ jpeg kmeans/training_data_earth.csv
```

**Example \#2:** collect image training from both JPEG and PNG images types.
```
./K_Means 1 examples/earth/ jpeg,png kmeans/training_data_earth.csv
```

### Train (Mode 2)

A total of 4 arguments are expected:
 - Mode id i.e., the "train" mode in this case.
 - K number of clusters.
 - CSV file path of the training data.
 - Output CSV file where the cluster centroids will be written to.

**Example:**
```bash
./K_Means 2 4 kmeans/training_data_earth.csv kmeans/centroids_earth.csv
```
### Predict (Mode 3)

A total of 3 arguments are expected:
 - Mode id i.e., the "predict" mode in this case.
 - File path of the image to label.
 - CSV file path of the centroid file used to determine which label to apply to the given image.

**Examples:**
```bash
./K_Means 3 examples/earth/img_msec_1606835961336_2_thumbnail.jpeg kmeans/centroids_earth.csv
./K_Means 3 examples/earth/img_msec_1609324358567_2_thumbnail.jpeg kmeans/centroids_earth.csv
./K_Means 3 examples/earth/img_msec_1606876023471_2_thumbnail.jpeg kmeans/centroids_earth.csv
./K_Means 3 examples/earth/img_msec_1609362399310_2_thumbnail.jpeg kmeans/centroids_earth.csv
```

### Batch Predict (Mode 4)

A total of 6 arguments are expected:
 - Mode id i.e., the "batch predict" mode in this case.
 - Directory path of images to label.
 - Comma separated list of image file types to process with the clustering model.
 - Directory path to move the clustered/labeled imaged to.
 - Comma separated list of image file types to move to their respective cluster/label directories.
 - CSV file of the centroid file used to determine the labels to apply to the given images.

**Example:**
```bash
./K_Means 4 examples/earth/ jpeg kmeans/clustered/earth/ jpeg,png kmeans/centroids_earth.csv
```