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
#define NORMALIZE                                                                                     1

/** 
 * Training image dimension.
 * The images that will be used as training inputs will be resized to this dimension
 */
#define KMEANS_IMAGE_WIDTH                                                                           20
#define KMEANS_IMAGE_HEIGHT                                                                          20

/** 
 * Training image channels.
 * The images that will be used as trainig inputs will be have their channels changed to this.
 * Options are defined in stb_image.h: STBI_default, STBI_grey, STBI_grey_alpha, and STBI_rgb_alpha
 */
#define KMEANS_IMAGE_CHANNELS                                                                 STBI_grey

/** 
 * Training image size.
 * The images that will be used as trainig inputs will be resized to this size
 */
#define KMEANS_IMAGE_SIZE              KMEANS_IMAGE_WIDTH * KMEANS_IMAGE_HEIGHT * KMEANS_IMAGE_CHANNELS

/**
 * Specific to OPS-SAT SmartCam.
 * The images files produced by OPS-SAT SmartCam do not all have the same names.
 * The jpeg file names have a thumbnail affix whereas the otther file formats do not. E.g.:
 *  - img_msec_1621184871142_2_thumbnail.jpeg
 *  - img_msec_1621184903224_2.png
 *  - img_msec_1621184903224_2.ims_rgb
 * 
 * This needs to be taken into account when moving non-jpeg image files into their respective cluster folders.
 */
#define BUILD_FOR_OPSSAT_SMARTCAM                                                                    1
#define THUMBNAIL_AFFIX_LENGTH                                            string("_thumbnail").length()

typedef enum _error_codes {
    NO_ERROR               = 0, /* No error */
    ERROR_ARGS             = 1, /* Error: invalid program arguments */
    ERROR_MODE             = 2, /* Error: invalid program mode selected */
    ERROR_OPENING_DIR      = 3, /* Error: opening directory */
    ERROR_NO_IMAGES        = 4, /* Error: no images in given directory */
    ERROR_LOADING_IMAGE    = 5, /* Error: loading image */
    ERROR_RESIZING_IMAGE   = 6, /* Error: resizing the images */
    ERROR_WRITING_CENTROID = 7, /* Error: writing CSV output file for centroids */
    ERROR_UNKNOWN          = 8  /* Error: unknown */
} errorCodes;

/**
 * Check if file exists given a filepath.
 */ 
inline bool exists(const string& filepath)
{
  struct stat buffer;
  return (stat (filepath.c_str(), &buffer) == 0); 
}

/**
 * Process a comma separated string into a vector of strings.
 */
inline void commaSeparatedStringToVector(const char* commaSeparatedString, vector<string> *pOutputVector)
{
    stringstream ss(commaSeparatedString);

    while(ss.good()) 
    {
        string substr;
        getline(ss, substr, ',');
        pOutputVector->push_back(substr);
    }
}

/**
 * Invokes mkdir_p but with some extra checks.
 * Create clustered centroids CSV file path directories if they don't exist already.
 * Recursively creates directories if more than one directory doesn't exist.
 */
inline int mkdir_p_x(string filepath)
{
    /* Don't create any directories if the give filepath is just a filename without any directory paths */
    if (filepath.find("/") != string::npos)
    {
        string dirPath = filepath.substr(0, filepath.find_last_of("\\/"));
        int mkdirRes = mkdir_p(dirPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        
        return mkdirRes;
    }

    return NO_ERROR;
}

int createImgDataBuffer(const char *inputImgFilePath, int imgWidth, int imgHeight, int imgChannels, uint8_t* pImgDataBuffer)
{
    int inputImgWidth;
    int inputImgHeight;
    int intputImgChannels;

    /* Decode the image file */
    /* Note that the desired number of channels is the value fixed for the training and prediction image data input */
    uint8_t *inputImgData = (uint8_t*)stbi_load(inputImgFilePath, &inputImgWidth, &inputImgHeight, &intputImgChannels, imgChannels);

    /* NULL on an allocation failure or if the image is corrupt or invalid */
    if(inputImgData == NULL)
    {
        std::cout << "Error: allocation failure of image file is corrupt or invalid: " << inputImgFilePath << endl;
        return ERROR_LOADING_IMAGE;
    }

    /* Downsample the image i.e., resize the image to a smaller dimension */
    int resizeRes = stbir_resize_uint8(inputImgData, inputImgWidth, inputImgHeight, 0, pImgDataBuffer, imgWidth, imgHeight, 0, imgChannels);

    /* Free the input image data buffer */
    stbi_image_free(inputImgData);

    /* Return error code in case of resize error */
    if(resizeRes == 0)
    {
        return ERROR_RESIZING_IMAGE;
    }

    return NO_ERROR;
}

int createTrainingDataVector(int trainingImgWidth, int trainingImgHeight, int trainingImgChannels,\
    string inputImgDirPath, vector<string> *pTrainingImgTypeVector, vector<string> *pImgFileNameVector,\
    vector<array<float, KMEANS_IMAGE_SIZE>> *pTrainingImgVector)
{
    /* Error code returned after decoding the input image */
    int imgDecodeRes;

    /* The data buffer that will contain a downsampled image data to be used as a training data point */
    const int trainingImgSize = trainingImgWidth * trainingImgHeight * trainingImgChannels;
    uint8_t trainingImgDataBuffer[trainingImgSize];

    /* The array that will contain a downsampled image data to use as a training data point */
    array<float, KMEANS_IMAGE_SIZE> trainingImgDataArray;

    DIR *dir;
    struct dirent *ent;
    string filename;

    if((dir = opendir(inputImgDirPath.c_str())) != NULL)
    {   
        /* Print all the files and directories within directory */
        while((ent = readdir(dir)) != NULL)
        {
            /* Only process regular image files */
            if(ent->d_type == DT_REG)
            {
                /* Get filename and its extension */
                filename = string(ent->d_name);
                string fileExtension = filename.substr(filename.find_last_of(".") + 1);

                /* Process image as training data if the image is the desired type i.e., it has the desired extension */
                if (find(pTrainingImgTypeVector->begin(), pTrainingImgTypeVector->end(), fileExtension) != pTrainingImgTypeVector->end())
                {
                    string inputImgFilePath(inputImgDirPath.c_str());
                    inputImgFilePath.append("/");
                    inputImgFilePath.append(ent->d_name);

                    /* Create buffer containing image training data */
                    imgDecodeRes = createImgDataBuffer(inputImgFilePath.c_str(), trainingImgWidth, trainingImgHeight, trainingImgChannels, trainingImgDataBuffer);

                    /* If input image was successfully decoded then transform it into an array */
                    if(imgDecodeRes == NO_ERROR)
                    {
                        /* Put image data into array */
                        for(int i = 0; i < trainingImgSize; i++)
                        {
                            trainingImgDataArray.at(i) = (NORMALIZE == 1) ? ((int)trainingImgDataBuffer[i]) / 255.0 : (float)trainingImgDataBuffer[i];
                        }

                        /* Put array into vector */
                        pTrainingImgVector->push_back(trainingImgDataArray);

                        /* Keep track of all the image file names being processed */
                        pImgFileNameVector->push_back(ent->d_name);
                    }
                    else
                    {
                        /* Skip problematic image file */
                        std::cout << "Skipping invalid or corrupt image: " << ent->d_name << endl;
                    }
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
        return ERROR_OPENING_DIR;
    }

    return NO_ERROR;
}

int cpyImgsToLabelDirs(tuple<vector<array<float, KMEANS_IMAGE_SIZE>>, vector<uint32_t>> *pClusterData,\
    vector<string> *pImgFileNameVector, string inputImgDirPath, string labelDirPath, vector<string> *pImgTypesToCopyVector)
{
    int i = 0;
    for (const int label : std::get<1>(*pClusterData))
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
            std::cout << "Error: failed to create directory for file path: " << clusteredImgFilePath << endl;
            return mkdirRes;
        }

        /* Path of the labeled image */
        clusteredImgFilePath.append(pImgFileNameVector->at(i).c_str());

        /* Path the the imput image file, without the image file type extension */
        string inputImgFilePathWithoutExtension;

        /* Path of the clustered image file, without the image file type extension. */
        string clusteredImgFilePathWithoutExtension;

        /* Move all the image types that need to be moved */
        for(size_t j=0; j < pImgTypesToCopyVector->size(); j++)
        {
            /* Build file paths without file type extension */
            inputImgFilePathWithoutExtension = inputImgFilePath.substr(0, inputImgFilePath.find_last_of("."));
            clusteredImgFilePathWithoutExtension = clusteredImgFilePath.substr(0, clusteredImgFilePath.find_last_of("."));

#if BUILD_FOR_OPSSAT_SMARTCAM
            if (pImgTypesToCopyVector->at(j) != "jpeg")
            {
                inputImgFilePathWithoutExtension = inputImgFilePathWithoutExtension\
                    .replace(inputImgFilePathWithoutExtension.end()-THUMBNAIL_AFFIX_LENGTH, inputImgFilePathWithoutExtension.end(), "");
            }
#endif

            /* The src image that we want to copy */
            string src = inputImgFilePathWithoutExtension + "." + pImgTypesToCopyVector->at(j);

            /* Copy image file from input image directory into cluster/label directory */
            /* Only proceeed with a copy if the source image exists */
            if(exists(src))
            {

#if BUILD_FOR_OPSSAT_SMARTCAM
                if (pImgTypesToCopyVector->at(j) != "jpeg")
                {
                    clusteredImgFilePathWithoutExtension = clusteredImgFilePathWithoutExtension\
                        .replace(clusteredImgFilePathWithoutExtension.end()-THUMBNAIL_AFFIX_LENGTH, clusteredImgFilePathWithoutExtension.end(), "");
                }
#endif

                ifstream srcStream(src.c_str());
                ofstream dstStream((clusteredImgFilePathWithoutExtension + "." + pImgTypesToCopyVector->at(j)).c_str());

                dstStream << srcStream.rdbuf();
            }
        }

        /* Increment index */
        i++;
    }

    return NO_ERROR;
}


int appendTrainingDataToCsvFile(int trainingImgWidth, int trainingImgHeight, int trainingImgChannels, string inputImgDirPath, vector<string> *pTrainingImgTypeVector, string trainingDataCsvFilePath, int *pNewTrainingDataCount)
{
    /* Error code returned after decoding the input image */
    int imgDecodeRes;

    /* The training data is made out of image pixe values */
    float pixel;

    /* The data buffer that will contain a downsampled image data to be used as a training data point */
    const int trainingImgSize = trainingImgWidth * trainingImgHeight * trainingImgChannels;
    uint8_t trainingImgDataBuffer[trainingImgSize];

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
                /* Get filename and its extension */
                string filename = string(ent->d_name);
                string fileExtension = filename.substr(filename.find_last_of(".") + 1);

                /* Process image as training data if the image is the desired type i.e., it has the desired extension */
                if (find(pTrainingImgTypeVector->begin(), pTrainingImgTypeVector->end(), fileExtension) != pTrainingImgTypeVector->end())
                {
                    string inputImgFilePath(inputImgDirPath.c_str());
                    inputImgFilePath.append("/");
                    inputImgFilePath.append(ent->d_name);

                    imgDecodeRes = createImgDataBuffer(inputImgFilePath.c_str(), trainingImgWidth, trainingImgHeight, trainingImgChannels, trainingImgDataBuffer);

                    /* If input image was successfully decoded then transform it into an array */
                    if(imgDecodeRes == NO_ERROR)
                    {
                        /* Create CSV row containing all pixel values */
                        for(int i = 0; i < trainingImgSize; i++)
                        {
                            pixel = (NORMALIZE == 1) ? ((int)trainingImgDataBuffer[i]) / 255.0 : (float)trainingImgDataBuffer[i];
                            csvRow.append(to_string(pixel));
                            csvRow.append(",");
                        }

                        /* Write row to the CSV file */
                        csvRow.append("\n");
                        trainingDataCsvFile << csvRow;

                        /* Count number of training data appended to the CSV file */
                        *pNewTrainingDataCount = *pNewTrainingDataCount + 1;
                    }
                    else
                    {
                        /* Skip problematic image file */
                        std::cout << "Skipping invalid or corrupt image: " << ent->d_name << endl;
                    }
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
        return ERROR_OPENING_DIR;
    }

    return NO_ERROR;
}


int writeCentroidsToCsvFile(tuple<vector<array<float, KMEANS_IMAGE_SIZE>>, vector<uint32_t>> *pClusterData, string clusterCentroidsCsvFilePath)
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
        std::cout << "Error: an unknown error occured while writing the CSV output file for the cluster centroids: " << clusterCentroidsCsvFilePath << endl;
        return ERROR_WRITING_CENTROID;
    }

    return NO_ERROR;
}

int batchPredict(string inputImgDirPath, vector<string> *pImgTypesToInferVector, string outputImgDirPath, vector<string> *pImgTypesToMoveVector, int imgWidth, int imgHeight, int imgChannels, string clusterCentroidsCsvFilePath)
{
    /* Error code returned after decoding the input image */
    int imgDecodeRes;

    /* Error code moving the image file from the input directory to the label output directory */
    int renameRes;

    /* Error code returned after creating the directories for the labeled image output file path */
    int mkdirRes;

    /* The cluster id that an image will be labeld with */
    int clusterId;

    /* Read the cluster centroids CSV file */
    std::vector<std::array<float, KMEANS_IMAGE_SIZE>> clusterCentroidsVector;
    clusterCentroidsVector = dkm::load_csv<float, KMEANS_IMAGE_SIZE>(clusterCentroidsCsvFilePath.c_str());

    /* The data buffer that will contain a downsampled image data to process */
    const int imgSize = imgWidth * imgHeight * imgChannels;
    uint8_t imgDataBuffer[imgSize];

    /* The array that will contain a downsampled image data to process */
    array<float, KMEANS_IMAGE_SIZE> imgDataArray;

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
                /* Get filename and its extension */
                string filename = string(ent->d_name);
                string fileExtension = filename.substr(filename.find_last_of(".") + 1);

                /* Process and move the image if it is a image type that we want to move i.e., it has the desired extension */
                if (find(pImgTypesToInferVector->begin(), pImgTypesToInferVector->end(), fileExtension) != pImgTypesToInferVector->end())
                {
                    string inputImgFilePath(inputImgDirPath.c_str());
                    inputImgFilePath.append("/");
                    inputImgFilePath.append(ent->d_name);

                    /* Create buffer containing image data */
                    imgDecodeRes = createImgDataBuffer(inputImgFilePath.c_str(), imgWidth, imgHeight, imgChannels, imgDataBuffer);

                    /* If input image was successfully decoded then transform it into an array */
                    if(imgDecodeRes == NO_ERROR)
                    {
                        /* Put image data into array */
                        for(int i = 0; i < imgSize; i++)
                        {
                            imgDataArray.at(i) = (NORMALIZE == 1) ? ((int)imgDataBuffer[i]) / 255.0 : (float)imgDataBuffer[i];
                        }

                        /* Use the centroids data to predict which cluster/label applies to the image */
                        /* Return the cluster id to which the input image belongs to */
                        clusterId = dkm::predict<float, KMEANS_IMAGE_SIZE>(clusterCentroidsVector, imgDataArray);

                        /* Build file path of output image (located in cluster/label directory) */
                        string outputImgFilePath(outputImgDirPath.c_str());
                        outputImgFilePath.append("/");
                        outputImgFilePath.append(to_string(clusterId));
                        outputImgFilePath.append("/");
                        outputImgFilePath.append(ent->d_name);

                        /* Create the directories for the labeled image output file path (if they don't exist) */
                        mkdirRes = mkdir_p_x(outputImgFilePath);

                        /* Check for error creating directories */
                        if(mkdirRes != NO_ERROR)
                        {
                            std::cout << "Error: failed to create directory for file path: " << outputImgFilePath << endl;
                            return mkdirRes;
                        }

                        /* Path the the imput image file, without the image file type extension */
                        string inputImgFilePathWithoutExtension;

                        /* Path of the ouput image file, without the image file type extension. */
                        string outputImgFilePathWithoutExtension;

                        /* Move all the image types that need to be moved */
                        for(size_t j=0; j < pImgTypesToMoveVector->size(); j++)
                        {
                            /* Build file paths without file type extension */
                            inputImgFilePathWithoutExtension = inputImgFilePath.substr(0, inputImgFilePath.find_last_of("."));
                            outputImgFilePathWithoutExtension = outputImgFilePath.substr(0, outputImgFilePath.find_last_of("."));

#if BUILD_FOR_OPSSAT_SMARTCAM
                            if (pImgTypesToMoveVector->at(j) != "jpeg")
                            {
                                inputImgFilePathWithoutExtension = inputImgFilePathWithoutExtension\
                                    .replace(inputImgFilePathWithoutExtension.end()-THUMBNAIL_AFFIX_LENGTH, inputImgFilePathWithoutExtension.end(), "");
                            }
#endif
                            /* The src image file path with the target image file type extension */
                            string src = inputImgFilePathWithoutExtension + "." + pImgTypesToMoveVector->at(j);

                            /* Move the file if it exists */
                            if(exists(src))
                            {
#if BUILD_FOR_OPSSAT_SMARTCAM
                                if (pImgTypesToMoveVector->at(j) != "jpeg")
                                {
                                    outputImgFilePathWithoutExtension = outputImgFilePathWithoutExtension\
                                        .replace(outputImgFilePathWithoutExtension.end()-THUMBNAIL_AFFIX_LENGTH, outputImgFilePathWithoutExtension.end(), "");
                                }
#endif
                                /* The dst image file path with the target image file type extension */
                                string dst = outputImgFilePathWithoutExtension + "." + pImgTypesToMoveVector->at(j);

                                /* Move the image to its label directory */
                                renameRes = rename(src.c_str(), dst.c_str());

                                /* Check for errors */
                                if(renameRes != NO_ERROR)
                                {
                                    /* Skip problematic image file */
                                    std::cout << "Error: failed to move file: " << src << " --> " << dst << endl;
                                }
                            }
                        }
                    }
                    else
                    {
                        /* Skip problematic image file */
                        std::cout << "Skipping invalid or corrupt image: " << ent->d_name << endl;
                    }
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
        return ERROR_OPENING_DIR;
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
int main(int argc, char **argv)
{
    try
    {
        /* Check that at least the "mode" argument is given */
        if(argc < 2)
        {
            std::cerr << "Error: command-line argument count mismatch." << endl;
            return ERROR_ARGS;
        }

        /* Get the mode id */
        int mode = atoi(argv[1]);

        /* Process the selected mode */
        if(mode == 0)
        {
            /** 
             * Mode: train now.
             * 
             * A total of 5 or 7 arguments are expected:
             *  - the mode id i.e., the "train now" mode in this case.
             *  - the K number of clusters.
             *  - the output CSV file where the cluster centroids will be written to.
             *  - the image directory where the images to be clustered are located.
             *  - the comma separate list of image file types to use as training data.
             *  - Optional: 
             *      - the cluster directory where the images will be copied to.
             *      - the comma separated list of image file types to also copy over.
             * 
             */

            if(argc != 6 && argc != 8)
            {
                std::cerr << "Error: command-line argument count mismatch for \"train now\" mode." << endl;
                return ERROR_ARGS;
            }

            /* Fetch arguments */
            int K = atoi(argv[2]);
            string clusterCentroidsCsvFilePath = argv[3];
            string inputImgDirPath = argv[4];

            /* Create vector of image types to use as training data */
            vector<string> trainingImgTypesVector;

            /* Process a comma separated string into a vector of strings */
            commaSeparatedStringToVector(argv[5], &trainingImgTypesVector);
 
            /* Create clustered centroids CSV file path directories if they don't exist already */
            int mkdirRes = mkdir_p_x(clusterCentroidsCsvFilePath);

            /* Exit program if directories were not created as expected */
            if(mkdirRes != NO_ERROR)
            {
                std::cerr << "Error: failed to create directory for file path: " << clusterCentroidsCsvFilePath << endl;
                return mkdirRes;
            }

            /* The vector that will contain all the filenames */
            vector<string> imgFileNameVector;

            /* The vector that will contain all the downsampled image data points as training data points */
            vector<array<float, KMEANS_IMAGE_SIZE>> trainingImgVector;

            /* Populate the training image data vector */
            createTrainingDataVector(KMEANS_IMAGE_WIDTH, KMEANS_IMAGE_HEIGHT, KMEANS_IMAGE_CHANNELS, inputImgDirPath, &trainingImgTypesVector, &imgFileNameVector, &trainingImgVector);

            /* Check if images were loaded or not */
            if(trainingImgVector.size() == 0)
            {
                std::cerr << "Error: No image files found in given directory: " << inputImgDirPath << endl;
                return ERROR_NO_IMAGES;
            }

            /* Use K-Means Lloyd algorithm to build clusters */
            tuple<std::vector<std::array<float, KMEANS_IMAGE_SIZE>>, vector<uint32_t>> clusterData;
            clusterData = dkm::kmeans_lloyd<float, KMEANS_IMAGE_SIZE>(trainingImgVector, K);

            /* Copy the input images to their respective cluster image directory (if this option has been selected by providing a label directory path). */
            if(argc == 8)
            {
                /* The cluster/label directory path */
                string labelDirPath = argv[6];

                /* Vector of image types to move to the cluster folders */
                vector<string> imgTypesToCopyVector;

                /* Process a comma separated string into a vector of strings */
                commaSeparatedStringToVector(argv[7], &imgTypesToCopyVector);

                /* Copy images to cluster/label directories */
                int cpyRes = cpyImgsToLabelDirs(&clusterData, &imgFileNameVector, inputImgDirPath, labelDirPath, &imgTypesToCopyVector);

                /* Exit program if images were not copied to cluster/label directories */
                if(cpyRes != NO_ERROR)
                {
                    std::cerr << "Error: failed to create directory for file path: " << clusterCentroidsCsvFilePath << endl;
                    return cpyRes;
                }
            }

            /* Write CSV output file for cluster centroids */
            int centroidsRes = writeCentroidsToCsvFile(&clusterData, clusterCentroidsCsvFilePath);
            if(centroidsRes != NO_ERROR)
            {
                std::cerr << "Error: an unknown error occured while writing the CSV output file for the cluster centroids: " << clusterCentroidsCsvFilePath << endl;
                return centroidsRes;
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
             *  - the comma separated list of image file types to use as training data.
             *  - the CSV file path where training data will be written to.
             */
            if(argc != 5)
            {
                std::cerr << "Error: command-line argument count mismatch for \"collect\" mode." << endl;
                return ERROR_ARGS;
            }

            /* Fetch arguments */
            string inputImgDirPath = argv[2];

            /* Process a comma separated string into a vector of strings */
            vector<string> trainingImgTypesVector;
            commaSeparatedStringToVector(argv[3], &trainingImgTypesVector);

            /* Fetch last argument */
            string trainingDataCsvFilePath = argv[4];

            /* Create CSV file path directories if they don't exist already */
            int mkdirRes = mkdir_p_x(trainingDataCsvFilePath);

            /* Exit program if directories were not created as expected */
            if(mkdirRes != NO_ERROR)
            {
                std::cerr << "Error: failed to create directory for file path: " << trainingDataCsvFilePath << endl;
                return mkdirRes;
            }

            /* Decode all images and write their pixel data into a CSV file */
            int newTrainingDataCount;
            int appendRes = appendTrainingDataToCsvFile(KMEANS_IMAGE_WIDTH, KMEANS_IMAGE_HEIGHT, KMEANS_IMAGE_CHANNELS, inputImgDirPath, &trainingImgTypesVector, trainingDataCsvFilePath, &newTrainingDataCount);

            /* Exit program if no training data was written (e.g. image folder is empty) */
            if(appendRes != NO_ERROR)
            {
                std::cerr << "Error: failed to create training data CSV file, maybe the image directory is empty: " << inputImgDirPath << endl;
                return appendRes;
            }

            std::cout << newTrainingDataCount;
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
                std::cerr << "Error: command-line argument count mismatch for \"train\" mode." << endl;
                return ERROR_ARGS;
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
                std::cerr << "Error: failed to create directory for file path: " << clusterCentroidsCsvFilePath << endl;
                return mkdirRes;
            }

            /* Read training data CSV and create the training data vector */
            std::vector<std::array<float, KMEANS_IMAGE_SIZE>> trainingImgVector;
            trainingImgVector = dkm::load_csv<float, KMEANS_IMAGE_SIZE>(trainingDataCsvFilePath.c_str());

            /* Use K-Means Lloyd algorithm to build clusters */
            tuple<std::vector<std::array<float, KMEANS_IMAGE_SIZE>>, vector<uint32_t>> clusterData;
            clusterData = dkm::kmeans_lloyd<float, KMEANS_IMAGE_SIZE>(trainingImgVector, K);

            /* Write the cluster centroids to a CSV file */
            int centroidsRes = writeCentroidsToCsvFile(&clusterData, clusterCentroidsCsvFilePath);
            if(centroidsRes != NO_ERROR)
            {
                return centroidsRes;
            }
        }
        else if(mode == 3)
        {
            /** 
             * Mode: predict.
             * 
             * A total of 3 arguments are expected:
             *  - the mode id i.e., the "predict" mode in this case.
             *  - the file path of the image to label.
             *  - the centroid CSV file used to determine the label to apply to the given image.
             */
            if(argc != 4)
            {
                std::cerr << "Error: command-line argument count mismatch for \"predict\" mode." << endl;
                return ERROR_ARGS;
            }

            /* Fetch arguments */
            string inputImgFilePath = argv[2];
            string clusterCentroidsCsvFilePath = argv[3];

            /* Create buffer containing image input data */
            uint8_t imgDataBuffer[KMEANS_IMAGE_SIZE];
            int imgDecodeRes = createImgDataBuffer(inputImgFilePath.c_str(), KMEANS_IMAGE_WIDTH, KMEANS_IMAGE_HEIGHT, KMEANS_IMAGE_CHANNELS, imgDataBuffer);

            /* Exit program if failed to load input image. */
            if(imgDecodeRes != NO_ERROR)
            {
                std::cerr << "Error: failed to load input image: " << inputImgFilePath << endl;
                return imgDecodeRes;
            }

            /* Need to put the image data buffer into an array */
            array<float, KMEANS_IMAGE_SIZE> imgDataArray;

            /* Put image data into array */
            for(int i = 0; i < KMEANS_IMAGE_SIZE; i++)
            {
                imgDataArray.at(i) = (NORMALIZE == 1) ? ((int)imgDataBuffer[i]) / 255.0 : (float)imgDataBuffer[i];
            }

            /* Read the cluster centroids CSV file */
            std::vector<std::array<float, KMEANS_IMAGE_SIZE>> clusterCentroidsVector;
            clusterCentroidsVector = dkm::load_csv<float, KMEANS_IMAGE_SIZE>(clusterCentroidsCsvFilePath.c_str());

            /* Return the cluster id to which the input image belongs to */
            int clusterId = dkm::predict<float, KMEANS_IMAGE_SIZE>(clusterCentroidsVector, imgDataArray);

            /* Return the cluster id label applied to the input image */
            std::cout << clusterId;
        }
        else if(mode == 4)
        {
            /** 
             * Mode: batch predict.
             * 
             * A total of 6 arguments are expected:
             *  - the mode id i.e., the "batch predict" mode in this case.
             *  - the directory path of images to label.
             *  - the comma separated list of image file types to process with the clustering model.
             *  - the directory path to move the labeled imaged to.
             *  - the comma separated list of image file types to move to their respective cluster/label directories.
             *  - the CSV file of the centroid file used to determine the labels to apply to the given images.
             */
            if(argc != 7)
            {
                std::cerr << "Error: command-line argument count mismatch for \"batch predict\" mode." << endl;
                return ERROR_ARGS;
            }

            /* Fetch arguments */
            string inputImgDirPath = argv[2];

            /* Vector of image types to move to the cluster folders */
            vector<string> imgTypesToInferVector;

            /* Process a comma separated string into a vector of strings */
            commaSeparatedStringToVector(argv[3], &imgTypesToInferVector);

            /* Fetch argument */
            string outputImgDirPath = argv[4];

            /* Vector of image types to move to the cluster folders */
            vector<string> imgTypesToMoveVector;

            /* Process a comma separated string into a vector of strings */
            commaSeparatedStringToVector(argv[5], &imgTypesToMoveVector);

            /* Fetch argument */
            string clusterCentroidsCsvFilePath = argv[6];

            /* Cluster all images in the given directory */
            int batchPredRes = batchPredict(inputImgDirPath, &imgTypesToInferVector, outputImgDirPath, &imgTypesToMoveVector, KMEANS_IMAGE_WIDTH, KMEANS_IMAGE_HEIGHT, KMEANS_IMAGE_CHANNELS, clusterCentroidsCsvFilePath);

            /* Exit program if failed to load input image. */
            if(batchPredRes != NO_ERROR)
            {
                std::cerr << "Error: failed to cluster images in: " << inputImgDirPath << endl;
                return batchPredRes;
            }
        }
        else
        {
            std::cerr << "Error: invalid mode id." << endl;
            return ERROR_MODE;
        }
    }
    catch(const std::exception& e)
    {
        /* Unknown exception */
        std::cerr << e.what() << '\n';
        return ERROR_UNKNOWN;
    }

    return NO_ERROR;
}