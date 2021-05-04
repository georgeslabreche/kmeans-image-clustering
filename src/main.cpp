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
    NO_ERROR               = 0, /* No error */
    ERROR_LOADING_IMAGE    = 1, /* Error loading image */
    ERROR_RESIZING_IMAGE   = 2, /* Error resizing the images */
    ERROR_WRITING_CENTROID = 3  /* Error writing CSV output file for centroids */
} errorCodes;


/**
 * Invokes mkdir_p but with some extra checks.
 * Create clustered centroids CSV file path directories if they don't exist already.
 * Recursively creates directories if more than one directory doesn't exist.
 */
int mkdir_p_x(string filepath)
{
    /* Don't create any directories if the give filepath is just a filename without any directory paths */
    if (filepath.find("/") != std::string::npos)
    {
        string dirPath = filepath.substr(0, filepath.find_last_of("\\/"));
        int mkdirRes = mkdir_p(dirPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        
        return mkdirRes;
    }

    return NO_ERROR;
}

int createTrainingDataBuffer(const char *inputImgFilePath, int trainingImgWidth, int trainingImgHeight, int trainingImgChannels, uint8_t* pTrainingImgData)
{
    int inputImgWidth;
    int inputImgHeight;
    int intputImgChannels;

    /* Decode the image file */
    /* Note that the desired number of channels is the value set for the training image data input */
    // TODO: Handle errors.
    uint8_t *inputImgData = (uint8_t*)stbi_load(inputImgFilePath, &inputImgWidth, &inputImgHeight, &intputImgChannels, trainingImgChannels);

    /* NULL on an allocation failure or if the image is corrupt or invalid */
    if(inputImgData == NULL)
    {
        cout << "Error: allocation failure of image file is corrupt or invalid: " << inputImgFilePath << endl;
        return ERROR_LOADING_IMAGE;
    }

    /* Downsample the image i.e., resize the image to a smaller dimension */
    int resizeRes = stbir_resize_uint8(inputImgData, inputImgWidth, inputImgHeight, 0, pTrainingImgData, trainingImgWidth, trainingImgHeight, 0, trainingImgChannels);

    /* Free the input image data buffer */
    stbi_image_free(inputImgData);

    /* Return error code in case of resize error */
    if(resizeRes == 0)
    {
        return ERROR_RESIZING_IMAGE;
    }

    return NO_ERROR;
}

int createTrainingDataVector(int K, int trainingImgWidth, int trainingImgHeight, int trainingImgChannels,\
    string inputImgDirPath, vector<string> *pImgFileNameVector,\
    vector<array<float, TRAINING_IMAGE_SIZE>> *pTrainingImgVector)
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

                std::string inputImgFilePath(inputImgDirPath.c_str());
                inputImgFilePath.append("/");
                inputImgFilePath.append(ent->d_name);

                /* Create buffer containing image training data */
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

                    /* Keep track of all the image file names being processed */
                    pImgFileNameVector->push_back(ent->d_name);
                }
                else
                {
                    /* Skip problematic image file */
                    cout << "Skipping invalid or corrupt image: " << ent->d_name << endl;
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

int cpyImgToLabelDirs(tuple<vector<array<float, TRAINING_IMAGE_SIZE>>, vector<uint32_t>> *pClusterData,\
    vector<string> *pImgFileNameVector, string inputImgDirPath, string labelDirPath)
{
    int i = 0;
    for (const auto& label : std::get<1>(*pClusterData))
    {
        /* Path of the input image file */
        string inputImgFilePath(inputImgDirPath.c_str());
        inputImgFilePath.append("/");
        inputImgFilePath.append(pImgFileNameVector->at(i).c_str());

        /* Path of the cluster label directory */
        string clusteredImgFilePath(labelDirPath);
        clusteredImgFilePath.append("/");
        clusteredImgFilePath.append(to_string(label));
        clusteredImgFilePath.append("/");

        /* Create cluster parent directory if it doesn't exist already */
        /* Create CSV file path directories if they don't exist already */
        int mkdirRes = mkdir_p_x(clusteredImgFilePath);

        /* Exit program if directories were not created as expected */
        if(mkdirRes != NO_ERROR)
        {
            cout << "Error: failed to create directory for file path: " << clusteredImgFilePath << endl;
            return mkdirRes;
        }

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


int appendTrainingDataToCsvFile(int trainingImgWidth, int trainingImgHeight, int trainingImgChannels, string inputImgDirPath, string trainingDataCsvFilePath)
{
    /* Error code returned after decoding the input image */
    int imgDecodeRes;

    /* The training data is made out of image pixe values */
    float pixel;

    /* The data buffer that will contain a downsampled image data to be used as a training data point */
    const int trainingImgSize = trainingImgWidth * trainingImgHeight * trainingImgChannels;
    uint8_t trainingImgData[trainingImgSize];

    DIR *dir;
    struct dirent *ent;

    if((dir = opendir(inputImgDirPath.c_str())) != NULL)
    {
        /* Create a new CSV file if it doesn't exist or append to it if it already exists */
        ofstream trainingDataCsvFile(trainingDataCsvFilePath.c_str(), std::ios::app);

        /* Print all the files and directories within directory */
        while((ent = readdir(dir)) != NULL)
        {
            /* Initialize the CSV data row */
            string csvRow("");

            /* Only process regular image files */
            if(ent->d_type == DT_REG)
            {
                std::string inputImgFilePath(inputImgDirPath.c_str());
                inputImgFilePath.append("/");
                inputImgFilePath.append(ent->d_name);

                imgDecodeRes = createTrainingDataBuffer(inputImgFilePath.c_str(), trainingImgWidth, trainingImgHeight, trainingImgChannels, trainingImgData);

                /* If input image was successfully decoded then transform it into an array */
                if(imgDecodeRes == NO_ERROR)
                {
                    /* Create CSV row containing all pixel values */
                    for(int i = 0; i < trainingImgSize; i++)
                    {
                        pixel = (NORMALIZE == 1) ? ((int)trainingImgData[i]) / 255.0 : (float)trainingImgData[i];
                        csvRow.append(to_string(pixel));
                        csvRow.append(",");
                    }

                    /* Write row to the CSV file */
                    csvRow.append("\n");
                    trainingDataCsvFile << csvRow;
                }
                else
                {
                    /* Skip problematic image file */
                    cout << "Skipping invalid or corrupt image: " << ent->d_name << endl;
                }
            }
        }

        /* Close CSV file */
        trainingDataCsvFile.close();

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


int writeCentroidsToCsvFile(tuple<vector<array<float, TRAINING_IMAGE_SIZE>>, vector<uint32_t>> *pClusterData, string clusterCentroidsCsvFilePath)
{
    try{
        /* Create a new CSV file and write centroid rows to it */
        ofstream clusterCentroidsCsvFile(clusterCentroidsCsvFilePath.c_str());

        /* For each means vector */
        for (const auto& means : std::get<0>(*pClusterData))
        {
            /* Initialize the CSV data row */
            string csvRow("");

            /* Write all mean values in a CSV row */
            for(float m : means)
            {
                csvRow.append(to_string(m));
                csvRow.append(",");
            }

            /* Write row to the CSV file */
            csvRow.append("\n");
            clusterCentroidsCsvFile << csvRow;
        }

        /* Close CSV file */
        clusterCentroidsCsvFile.close();
    }
    catch(...)
    {
        cout << "Error: an unknown error occured while writing the CSV output file for the cluster centroids: " << clusterCentroidsCsvFilePath << endl;
        return ERROR_WRITING_CENTROID;
    }

    return NO_ERROR;
}

/**
 * There are 4 modes: train now, collect, train, and predict.
 *      mode 0 -  train now: train with the available images without persisting the training data in a .txt file.
 *                           in practical terms this mode only really serves for testing and debugging during development.
 *                           can optionally enable copying the input image files into clustered directors in kmeans/clusters/<label>/
 *      mode 1 -    collect: save training data in a .txt file in kmeans/<label>/training_data.txt
 *      mode 2 -      train: read training_data.txt and build clusters. Write centroids in a .txt file in kmeans/<label>/centroids.txt
 *      mode 3 -    predict: calculate distances from each centroid apply nearest cluster to the image ipunt.
 */
int main(int argc, char **argv){

    if(argc < 2)
    {
        cout << "Error: command-line argument count mismatch." << endl;
        return 1;
    }

    /* Get the mode id */
    int mode = atoi(argv[1]);


    if(mode == 0)
    {
        /** 
         * Mode: train now.
         * 
         * A total of 4 or 5 arguments are expected:
         *  - the mode id i.e., the "train now" mode in this case.
         *  - the K number of clusters.
         *  - the output CSV file where the cluster centroids will be written to.
         *  - the image directory where the images to be clustered are located.
         *  - (Optional) the cluster directory where the images will be copied to.
         */

        if(argc < 5 && argc > 6)
        {
            cout << "Error: command-line argument count mismatch for \"train now\" mode." << endl;
            return 1;
        }

        /* Fetch arguments */
        int K = atoi(argv[2]);
        string clusterCentroidsCsvFilePath = argv[3];
        string inputImgDirPath = argv[4];

        /* Create clustered centroids CSV file path directories if they don't exist already */
        int mkdirRes = mkdir_p_x(clusterCentroidsCsvFilePath);

        /* Exit program if directories were not created as expected */
        if(mkdirRes != NO_ERROR)
        {
            cout << "Error: failed to create directory for file path: " << clusterCentroidsCsvFilePath << endl;
            return 1;
        }

        /* The vector that will contain all the filenames */
        vector<string> imgFileNameVector;

        /* The vector that will contain all the downsampled image data points as training data points */
        vector<array<float, TRAINING_IMAGE_SIZE>> trainingImgVector;

        /* Populate the training image data vector */
        createTrainingDataVector(K, TRAINING_IMAGE_WIDTH, TRAINING_IMAGE_HEIGHT, TRAINING_IMAGE_CHANNELS, inputImgDirPath, &imgFileNameVector, &trainingImgVector);

        /* Check if images were loaded or not */
        if(trainingImgVector.size() == 0)
        {
            cout << "Error: No image files found in given directory: " << inputImgDirPath << endl;
            return 1;
        }

        /* Use K-Means Lloyd algorithm to build clusters */
        auto clusterData = dkm::kmeans_lloyd(trainingImgVector, K);

        /* Copy the input images to their respective cluster image directory (if this option has been selected by providing a label directory path). */
        if(argc == 6)
        {
            /* The cluster/label directory path */
            string labelDirPath = argv[5];

            /* Copye images to cluster/label directories */
            int cpyRes = cpyImgToLabelDirs(&clusterData, &imgFileNameVector, inputImgDirPath, labelDirPath);

            /* Exit program if images were not copied to cluster/label directories */
            if(cpyRes != NO_ERROR)
            {
                cout << "Error: failed to create directory for file path: " << clusterCentroidsCsvFilePath << endl;
                return 1;
            }
        }

        /* Write CSV output file for cluster centroids */
        int centroidsRes = writeCentroidsToCsvFile(&clusterData, clusterCentroidsCsvFilePath);
        if(centroidsRes != NO_ERROR)
        {
            return 1;
        }

    }
    else if(mode == 1)
    {
        /** 
         * Mode: collect.
         * 
         * A total of 4 arguments are expected:
         *  - the mode id i.e., the "collect" mode in this case.
         *  - the image directory where the images to be clustered are located.
         *  - the CSV file path where training data will be written to.
         */
        if(argc != 4)
        {
            cout << "Error: command-line argument count mismatch for \"collect\" mode." << endl;
            return 1;
        }

        /* Fetch arguments */
        string inputImgDirPath = argv[2];
        string trainingDataCsvFilePath = argv[3];

        /* Create CSV file path directories if they don't exist already */
        int mkdirRes = mkdir_p_x(trainingDataCsvFilePath);

        /* Exit program if directories were not created as expected */
        if(mkdirRes != NO_ERROR)
        {
            cout << "Error: failed to create directory for file path: " << trainingDataCsvFilePath << endl;
            return 1;
        }

        /* Decode all images and write their pixel data into a CSV file */
        int res = appendTrainingDataToCsvFile(TRAINING_IMAGE_WIDTH, TRAINING_IMAGE_HEIGHT, TRAINING_IMAGE_CHANNELS, inputImgDirPath, trainingDataCsvFilePath);

        /* Exit program if no training data was written (e.g. image folder is empty) */
        if(res != NO_ERROR)
        {
            cout << "Error: failed to create training data CSV file, maybe the image directory is empty: " << inputImgDirPath << endl;
            return 1;
        }
    }
    else if(mode == 2)
    {
        /** 
         * Mode: train.
         * 
         * A total of 4 arguments are expected:
         *  - the mode id i.e., the "train" mode in this case.
         *  - the K number of clusters.
         *  - the training data CSV file.
         *  - the training output CSV file where the cluster centroids will be written to.
         */
        if(argc != 5)
        {
            cout << "Error: command-line argument count mismatch for \"train\" mode." << endl;
            return 1;
        }

        /* Fetch arguments */
        int K = atoi(argv[2]);
        string trainingDataCsvFilePath = argv[3];
        string clusterCentroidsCsvFilePath = argv[4];

        /* Create clustered centroids CSV file path directories if they don't exist already */
        int mkdirRes = mkdir_p_x(clusterCentroidsCsvFilePath);

        /* Exit program if directories were not created as expected */
        if(mkdirRes != NO_ERROR)
        {
            cout << "Error: failed to create directory for file path: " << clusterCentroidsCsvFilePath << endl;
            return 1;
        }

        /* Read training data CSV and create the training data vector */
        std::vector<std::array<float, TRAINING_IMAGE_SIZE>> trainingImgVector;
        trainingImgVector = dkm::load_csv<float, TRAINING_IMAGE_SIZE>(trainingDataCsvFilePath.c_str());

        /* Use K-Means Lloyd algorithm to build clusters */
        auto clusterData = dkm::kmeans_lloyd(trainingImgVector, K);

        /* Write the cluster centroids to a CSV file */
        int centroidsRes = writeCentroidsToCsvFile(&clusterData, clusterCentroidsCsvFilePath);
        if(centroidsRes != NO_ERROR)
        {
            return 1;
        }

    }
    else if(mode == 3)
    {
        cout << "Error: not yet implemented the \"predict\" mode." << endl;
        return 1;
    }
    else
    {
        cout << "Error: invalid mode id." << endl;
        return 1;
    }

    return 0;
}