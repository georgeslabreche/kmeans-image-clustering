#include <iostream>
#include <vector>
#include <dirent.h>
#include <cstring>
#include <sys/stat.h>

#include <dkm.hpp>
#include <dkm_utils.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include "mkdir_p.hpp"

using namespace std;

/** 
 * Flag indicating whether or not the pixel values should be normalized or not.
 * In terms of clustering there seems to be no obvious advantages or disadvantages to normalizing or not.
 */
#define NORMALIZE                                                                                       1

/** 
 * Training image dimension.
 * The images that will be used as training inputs will be resized to this dimension
 */
#define TRAINING_IMAGE_WIDTH                                                                           20
#define TRAINING_IMAGE_HEIGHT                                                                          20

/** 
 * Training image channels.
 * The images that will be used as trainig inputs will be have their channels changed to this.
 * Options are defined in stb_image.h: STBI_default, STBI_grey, STBI_grey_alpha, and STBI_rgb_alpha
 */
#define TRAINING_IMAGE_CHANNELS                                                                 STBI_grey

/** 
 * Training image size.
 * The images that will be used as trainig inputs will be resized to this size
 */
#define TRAINING_IMAGE_SIZE        TRAINING_IMAGE_WIDTH * TRAINING_IMAGE_HEIGHT * TRAINING_IMAGE_CHANNELS


typedef enum _error_codes {
    NO_ERROR             = 0,  /* No error */
    ERROR_RESIZING_IMAGE = 1   /* Error resizing the images */
} errorCodes;


int createTrainingDataBuffer(const char *inputImgFilePath, int trainingImgWidth, int trainingImgHeight, int trainingImgChannels, uint8_t* pTrainingImgData)
{
    int inputImgWidth;
    int inputImgHeight;
    int intputImgChannels;

    /* Decode the image file */
    /* Note that the desired number of channels is the value set for the training image data input */
    // TODO: Handle errors.
    uint8_t *inputImgData = (uint8_t*)stbi_load(inputImgFilePath, &inputImgWidth, &inputImgHeight, &intputImgChannels, trainingImgChannels);

    /* Downsample the image i.e., resize the image to a smaller dimension */
    int resizeRes = stbir_resize_uint8(inputImgData, inputImgWidth, inputImgHeight, 0, pTrainingImgData, trainingImgWidth, trainingImgHeight, 0, trainingImgChannels);

    /* Free the input image data buffer */
    stbi_image_free(inputImgData);

    /* Return error code in case of resize error */
    if(resizeRes == 0){
        return ERROR_RESIZING_IMAGE;
    }

    return 0;
}

int createTrainingDataVector(int K, int trainingImgWidth, int trainingImgHeight, int trainingImgChannels, string inputImgDirPath, vector<string> *pImgFileNameVector, vector<array<float, TRAINING_IMAGE_SIZE>> *pTrainingImgVector)
{
    /* Error code returned after decoding the input image */
    int imgDecodeRes;

    /* The data buffer that will contain a downsampled image data to be used as a training data point */
    const int trainingImgSize = trainingImgWidth * trainingImgHeight * trainingImgChannels;
    uint8_t trainingImgData[trainingImgSize];

    /* The array that will contain a downsampled image data to use as a training data point */
    array<float, TRAINING_IMAGE_SIZE> trainingImgDataArray;

    DIR *dir;
    struct dirent *ent;

    if((dir = opendir(inputImgDirPath.c_str())) != NULL)
    {
        /* Print all the files and directories within directory */
        while((ent = readdir(dir)) != NULL)
        {
            /* Only process regular image files */
            if(ent->d_type == DT_REG)
            {
                /* Verbosity */
                //printf("%s\n", ent->d_name);

                std::string inputImgFilePath(inputImgDirPath.c_str());
                inputImgFilePath.append("/");
                inputImgFilePath.append(ent->d_name);

                /* Keep track of all the image file names being processed */
                pImgFileNameVector->push_back(ent->d_name);

                imgDecodeRes = createTrainingDataBuffer(inputImgFilePath.c_str(), trainingImgWidth, trainingImgHeight, trainingImgChannels, trainingImgData);

                /* If input image was successfully decoded then transform it into an array */
                if(imgDecodeRes == NO_ERROR)
                {
                    /* Put image data into array */
                    for(int i = 0; i < trainingImgSize; i++)
                    {
                        trainingImgDataArray.at(i) = (NORMALIZE == 1) ? ((int)trainingImgData[i]) / 255.0 : (float)trainingImgData[i];
                    }

                    /* Put array into vector */
                    pTrainingImgVector->push_back(trainingImgDataArray);
                }
                else
                {
                    // TODO: log error in else.
                    cout << "Error " << imgDecodeRes << ": creating training data vector " << endl;
                }
            }
        }

        /* Close opened directory */
        closedir(dir);
    }
    else
    {
        /* Could not open directory */
        /* Terminate app with failure */
        return EXIT_FAILURE;
    }

    return NO_ERROR;
}

int cpyImgToLabelDirs(auto *pClusterData, vector<string> *pImgFileNameVector, string inputImgDirPath, string labelDirPath)
{
    int i = 0;
    for (const auto& label : std::get<1>(*pClusterData))
    {
        /* Path of the input image file */
        string inputImgFilePath(inputImgDirPath.c_str());
        inputImgFilePath.append(pImgFileNameVector->at(i).c_str());

        /* Path of the cluster label directory */
        string clusteredImgFilePath(labelDirPath);
        clusteredImgFilePath.append(to_string(label));
        clusteredImgFilePath.append("/");

        /* Create cluster parent directory if it doesn't exist already */
        /* Recursevely creates directories if more than one directory doesn't exist */
        // TODO: Error check and handle mkdir_p response */
        mkdir_p(clusteredImgFilePath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        /* Path of the labeled image */
        clusteredImgFilePath.append(pImgFileNameVector->at(i).c_str());

        /* Copy image file from input image directory into cluster/label directory */
        std::ifstream src(inputImgFilePath.c_str());
        std::ofstream dst(clusteredImgFilePath.c_str());
        dst << src.rdbuf();

        i++;
    }

    return NO_ERROR;
}


int liveTrain(int K, int trainingImgWidth, int trainingImgHeight, int trainingImgChannels, uint8_t cpyImgToClusterDir, string inputImgDirPath, string labelDirPath)
{
    /* The vector that will contain all the filenames */
    vector<string> imgFileNameVector;

    /* The vector that will contain all the downsampled image data points as training data points */
    vector<array<float, TRAINING_IMAGE_SIZE>> trainingImgVector;

    /* Populate the training image data vector */
    createTrainingDataVector(K, trainingImgWidth, trainingImgHeight, trainingImgChannels, inputImgDirPath, &imgFileNameVector, &trainingImgVector);

    /* Use K-Means Lloyd algorithm to build clusters */
    auto clusterData = dkm::kmeans_lloyd(trainingImgVector, K);

    /* Copy the input images to their respective cluster image directory (if the flag to do so is set to true). */
    if(cpyImgToClusterDir == true)
    {
        cpyImgToLabelDirs(&clusterData, &imgFileNameVector, inputImgDirPath, labelDirPath);
    }

    return NO_ERROR;
}

/**
 * There are 4 modes: collect, train, predict, and live train.
 *      mode 0 -    collect: save training data in a .txt file in kmeans/<label>/training_data.txt
 *      mode 1 -      train: read training_data.txt and build clusters. Write centroids in a .txt file in kmeans/<label>/centroids.txt
 *      mode 2 -    predict: calculate distances from each centroid apply nearest cluster to the image ipunt.
 *      mode 3 - live train: train with the available thumbnails without persisting the training data in a .txt file.
 *                           in practical terms this mode only really serves for testing and debugging during development.
 *                           can optionally enable copying the input image files into clustered directors in kmeans/clusters/<label>/
 */
int main(int argc, char **argv){

    /* Number of clusters. The K in K-Means. */
    int K;

     /* Image directory and image label from  which clustering will be trained */
    uint8_t cpyImgToLabelDir;
    string inputImgDirPath;
    string labelDirPath;

    if(argc < 2)
    {
        cout << "Error: command-line argument count mismatch.";
        return 1;
    }

    /* Get the mode id */
    int mode = atoi(argv[1]);

    if(mode == 0)
    {
        /** 
         * Mode: collect training data.
         * 
         * A total of 8 arguments are expected:
         *  - the mode id i.e., "live train" mode in this case.
         *  - the K number of clusters.
         *  - the number of training iterations.
         *  - the width of the training image input (thumbnail image data will be downsampled to this dimension).
         *  - the height of the training image input (thumbnail image data will be downsampled to this dimension).
         *  - the number of channels of the given image (will be greyscaled either way).
         *  - the label for which subclassication clusters will be trained
         *  - the image directory where the thumbnail images are located.
         */
        if(argc != 9)
        {
            cout << "Error: command-line argument count mismatch for \"collect training data\" mode.";
            return 1;
        }

        /* Fetch arguments */
        K = atoi(argv[2]);
    }
    else if(mode == 1)
    {
        cout << "Error: not yet implemented the \"train clusters\" mode.";
        return 1;

    }
    else if(mode == 2)
    {
        cout << "Error: not yet implemented the \"predict\" mode.";
        return 1;
    }
    else if(mode == 3)
    {
        /** 
         * Mode: live train.
         * 
         * A total of 5 arguments are expected:
         *  - the mode id i.e., "live train" mode in this case.
         *  - the K number of clusters.
         *  - the flag indicating whether or not the thumbnails should be copied over to clustered directories.
         *  - the image directory where the images to be clustered are located.
         *  - the cluster directory where the images will be copied to (if the flag to do so is set to true).
         */
        if(argc != 6)
        {
            cout << "Error: command-line argument count mismatch for \"live train clusters\" mode.";
            return 1;
        }

        /* Fetch arguments */
        K = atoi(argv[2]);
        cpyImgToLabelDir = atoi(argv[3]);
        inputImgDirPath = argv[4];
        labelDirPath = argv[5];
        
        liveTrain(K, TRAINING_IMAGE_WIDTH, TRAINING_IMAGE_HEIGHT, TRAINING_IMAGE_CHANNELS, cpyImgToLabelDir, inputImgDirPath, labelDirPath);
    }
    else
    {
        cout << "Error: invalid mode id.";
        return 1;
    }

    return 0;
}