#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>

// Constants
#define FRAMEWIDTH 320
#define FRAMEHEIGHT 320
#define EXPANSIONPIXELS 0
#define BLURRYNESSTHRESHHOLD 100

#define YOLO8WEIGHTS "/home/nino/facemodels/res10_300x300_ssd_iter_140000_fp16.caffemodel"
#define YOLO4WEIGHTS "/home/nino/face-detection-yolov4-tiny-master/yolo/yolov4-tiny-3l_best.weights"
#define YOLO3WEIGHTS "/home/nino/facemodels/model-weights/yolov3-wider_16000.weights"

#define YOLO8CONFIG "/home/nino/facemodels/yolov8n-face.onnx"
#define YOLO4CONFIG "/home/nino/face-detection-yolov4-tiny-master/yolo/yolov4-tiny-3l.cfg"
#define YOLO3CONFIG "/home/nino/facemodels/cfg/yolov3-face.cfg"

// keys for what the files are called
#define SCANNINGKEY "scanningComplete"
#define STARTKEY "gameStart"
#define PLAYERSKEY "numPlayers"

// Display the webcam output or not
bool showFrame = true;

using namespace cv;
using namespace std;

// Class for handling file operations
class FileHandler
{
public:
    // Write data to a file
    static void writeToFile(const std::string &value, const std::string &name)
    {
        std::ofstream outFile(name + ".txt");
        if (outFile.is_open())
        {
            outFile << value;
            outFile.close();
            std::cout << "Value has been stored in " << name << ".txt" << std::endl;
        }
        else
        {
            std::cerr << "Unable to open the file for writing." << std::endl;
        }
    }

    // Read data from a file
    static std::string readFromFile(const std::string &name)
    {
        std::ifstream inFile(name + ".txt");
        std::string value;
        if (inFile.is_open())
        {
            inFile >> value;
            inFile.close();
        } // if the file is empty (in some edge cases) return 0
        else if (inFile.peek() == std::ifstream::traits_type::eof())
        {
            value = "0";
        }
        else
        {
            std::cerr << "Unable to open the file for reading." << std::endl;
        }
        return value;
    }
};

// Abstract YOLO Model Interface
class IYoloModel
{
public:
    virtual void loadModel(const std::string &config, const std::string &weights) = 0;
    virtual std::vector<cv::Rect> detectFaces(const cv::Mat &frame) = 0;
    virtual ~IYoloModel() {}
};

// YOLO3 Model
class YoloModelV3 : public IYoloModel
{
private:
    dnn::Net net;

public:
    void loadModel(const std::string &config, const std::string &weights) override
    {
        net = dnn::readNetFromDarknet(config, weights);
        net.setPreferableBackend(dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(dnn::DNN_TARGET_CPU);
    }

    std::vector<cv::Rect> detectFaces(const cv::Mat &frame) override
    {

        float confidenceThreshold = 0.5;
        float nmsThreshold = 0.4;

        // Prepare the frame for YOLO model
        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1 / 255.0, cv::Size(FRAMEWIDTH, FRAMEHEIGHT), cv::Scalar(0, 0, 0), true, false);

        // Set the blob as input to the network
        net.setInput(blob);

        // Forward pass to get the outputs
        std::vector<cv::Mat> outs;
        net.forward(outs, getOutputNames(net));

        // Initialize vectors to hold detection results
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

        // Process the output
        for (size_t i = 0; i < outs.size(); ++i)
        {
            // Scan through all the bounding boxes output from the network and keep only the ones with high confidence scores
            float *data = (float *)outs[i].data;
            for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols)
            {
                float confidence = data[4];
                if (confidence > confidenceThreshold)
                {
                    int centerX = (int)(data[0] * frame.cols);
                    int centerY = (int)(data[1] * frame.rows);
                    int width = (int)(data[2] * frame.cols);
                    int height = (int)(data[3] * frame.rows);
                    int left = centerX - width / 2;
                    int top = centerY - height / 2;

                    // Expand the bounding box by adding/subtracting expansionPixels
                    left = std::max(0, left - EXPANSIONPIXELS);
                    top = std::max(0, top - EXPANSIONPIXELS);
                    width = std::min(frame.cols - left, width + 2 * EXPANSIONPIXELS);
                    height = std::min(frame.rows - top, height + 2 * EXPANSIONPIXELS);

                    classIds.push_back(classIds.size()); // Assuming only one class (face)
                    confidences.push_back(confidence);
                    boxes.push_back(cv::Rect(left, top, width, height));
                }
            }
        }

        // Apply Non-Maximum Suppression to eliminate redundant overlapping boxes
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confidenceThreshold, nmsThreshold, indices);

        // Filter boxes based on NMS indices
        std::vector<cv::Rect> faces;
        for (int idx : indices)
        {
            faces.push_back(boxes[idx]);
        }

        return faces; // Return the list of faces after NMS
    }

    // Get names of YOLO output layers
    vector<cv::String> getOutputNames(const cv::dnn::Net &net)
    {
        static vector<cv::String> names;
        if (names.empty())
        {
            // retrieve all layer names from the yolo network and resize the name vector accordingly
            vector<int> outLayers = net.getUnconnectedOutLayers();
            vector<cv::String> layersNames = net.getLayerNames();
            names.resize(outLayers.size());
            // put all the names in the names vector
            for (size_t i = 0; i < outLayers.size(); ++i)
            {
                names[i] = layersNames[outLayers[i] - 1];
            }
        }
        return names;
    }
};

// YOLO4 Model
class YoloModelV4 : public IYoloModel
{
private:
    dnn::Net net;

public:
    void loadModel(const std::string &config, const std::string &weights) override
    {
        net = dnn::readNetFromDarknet(config, weights);
        net.setPreferableBackend(dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(dnn::DNN_TARGET_CPU);
    }

    std::vector<cv::Rect> detectFaces(const cv::Mat &frame) override
    {

        float confidenceThreshold = 0.5;
        float nmsThreshold = 0.4;

        // Prepare the frame for YOLO model
        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1 / 255.0, cv::Size(FRAMEWIDTH, FRAMEHEIGHT), cv::Scalar(0, 0, 0), true, false);

        // Set the blob as input to the network
        net.setInput(blob);

        // Forward pass to get the outputs
        std::vector<cv::Mat> outs;
        net.forward(outs, getOutputNames(net));

        // Initialize vectors to hold detection results
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

        // Process the output
        for (size_t i = 0; i < outs.size(); ++i)
        {
            // Scan through all the bounding boxes output from the network and keep only the ones with high confidence scores
            float *data = (float *)outs[i].data;
            for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols)
            {
                float confidence = data[4];
                if (confidence > confidenceThreshold)
                {
                    int centerX = (int)(data[0] * frame.cols);
                    int centerY = (int)(data[1] * frame.rows);
                    int width = (int)(data[2] * frame.cols);
                    int height = (int)(data[3] * frame.rows);
                    int left = centerX - width / 2;
                    int top = centerY - height / 2;

                    // Expand the bounding box by adding/subtracting expansionPixels
                    left = std::max(0, left - EXPANSIONPIXELS);
                    top = std::max(0, top - EXPANSIONPIXELS);
                    width = std::min(frame.cols - left, width + 2 * EXPANSIONPIXELS);
                    height = std::min(frame.rows - top, height + 2 * EXPANSIONPIXELS);

                    classIds.push_back(classIds.size()); // Assuming only one class (face)
                    confidences.push_back(confidence);
                    boxes.push_back(cv::Rect(left, top, width, height));
                }
            }
        }

        // Apply Non-Maximum Suppression to eliminate redundant overlapping boxes
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confidenceThreshold, nmsThreshold, indices);

        // Filter boxes based on NMS indices
        std::vector<cv::Rect> faces;
        for (int idx : indices)
        {
            faces.push_back(boxes[idx]);
        }

        return faces; // Return the list of faces after NMS
    }

    // Get names of YOLO output layers
    vector<cv::String> getOutputNames(const cv::dnn::Net &net)
    {
        static vector<cv::String> names;
        if (names.empty())
        {
            // retrieve all layer names from the yolo network and resize the name vector accordingly
            vector<int> outLayers = net.getUnconnectedOutLayers();
            vector<cv::String> layersNames = net.getLayerNames();
            names.resize(outLayers.size());
            // put all the names in the names vector
            for (size_t i = 0; i < outLayers.size(); ++i)
            {
                names[i] = layersNames[outLayers[i] - 1];
            }
        }
        return names;
    }
};

// YOLOv8 Model
class YoloModelV8 : public IYoloModel
{
private:
    dnn::Net net;
    float confThreshold;
    float nmsThreshold;

public:
    YoloModelV8(float confThreshold = 0.45, float nmsThreshold = 0.5)
        : confThreshold(confThreshold), nmsThreshold(nmsThreshold) {}

    void loadModel(const std::string &modelPath, const std::string & /*configPath*/) override
    {
        this->net = cv::dnn::readNet(modelPath); // Only uses the model path
        net.setPreferableBackend(dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(dnn::DNN_TARGET_CPU);
    }
    std::vector<cv::Rect> detectFaces(const cv::Mat &frame) override
    {
        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;

        // Create a blob from the input frame
        Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1 / 255.0, Size(640, 640), Scalar(0, 0, 0), true, false);
        net.setInput(blob);

        // Forward pass to get the outputs
        std::vector<Mat> outs;
        net.forward(outs, net.getUnconnectedOutLayersNames());

        for (auto &out : outs)
        {
            std::cout << "Output size: " << out.size << std::endl;
            for (int i = 0; i < out.rows; ++i)
            {
                float *data = out.ptr<float>(i);
                float confidence = data[1];
                std::cout << "Detection " << i << ": Confidence = " << confidence << std::endl;
            }
        }

        // Process each output layer
        for (auto &out : outs)
        {
            // The output should have the shape [number_of_detections, 6]
            // Each detection has the format [classId, confidence, x, y, width, height]
            for (int i = 0; i < out.rows; ++i)
            {
                float *detection = out.ptr<float>(i);
                float confidence = detection[1];

                // Filter out weak detections by ensuring the confidence is greater than a minimum threshold
                if (confidence > this->confThreshold)
                {
                    int centerX = static_cast<int>(detection[2] * frame.cols);
                    int centerY = static_cast<int>(detection[3] * frame.rows);
                    int width = static_cast<int>(detection[4] * frame.cols);
                    int height = static_cast<int>(detection[5] * frame.rows);
                    int left = centerX - width / 2;
                    int top = centerY - height / 2;

                    // Add the bounding box and confidence to their respective vectors
                    boxes.push_back(cv::Rect(left, top, width, height));
                    confidences.push_back(confidence);
                }
            }
        }

        // Apply Non-Maximum Suppression to eliminate redundant overlapping boxes
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, this->confThreshold, this->nmsThreshold, indices);

        // Use indices to add the final set of bounding boxes to faces
        std::vector<cv::Rect> faces;
        for (int idx : indices)
        {
            faces.push_back(boxes[idx]);
        }

        return faces;
    }
};

// WebcamHandler to manage webcam capture
class WebcamHandler
{
protected:
    VideoCapture cap;
    std::unique_ptr<IYoloModel> model;
    std::thread processingThread; // Thread for asynchronous processing

public:
    explicit WebcamHandler(int camIndex, std::unique_ptr<IYoloModel> model)
        : cap(camIndex, CAP_V4L), model(std::move(model))
    {
        if (!cap.isOpened())
        {
            cerr << "Error: Unable to open the webcam." << endl;
            exit(-1);
        }
    }

    void captureAndProcess()
    {
        Mat frame;
        while (true)
        {
            cap >> frame;
            if (frame.empty())
                break;

            processFrame(frame);

            // if (showFrame)
            // {
            //     imshow("Frame", frame);
            //     if (waitKey(1) == 27)
            //         break; // ESC to quit
            // }
        }
    }

    virtual void processFrame(Mat &frame)
    {
        // Default implementation does nothing
        // Override in derived classes
    }
};

// FaceRecognitionHandler for processing and recognizing faces
class FaceRecognitionHandler : public WebcamHandler
{
public:
    int numberPlayers = 0;
    int gameStart = 0;
    bool readyToStart = false;
    bool facesCaptured = false;

    using WebcamHandler::WebcamHandler;

    void processFrame(Mat &frame) override
    {
        if (readyToStart)
        {
            // Detect faces in the frame
            auto faces = model->detectFaces(frame);

            // Iterate over all detected faces and draw rectangles around them
            for (const auto &face : faces)
            {
                rectangle(frame, face, Scalar(0, 255, 0), 2); // Green rectangle with thickness of 2
            }

            CheckAndSafeFaces(faces, frame);
        }

        logisch();

        // If desired, show the frame with detected faces in a window
        if (showFrame)
        {
            imshow("Detected Faces", frame);
            waitKey(1); // Wait for a key press for a short duration to update the window
        }
    }

    // Check if the correct amount of faces have been detected
    void CheckAndSafeFaces(vector<cv::Rect> boxes, const cv::Mat &frame)
    {
        if (boxes.size() >= numberPlayers && !facesCaptured)
        {
            std::cout << "Number of faces found: " << boxes.size() << std::endl;

            // Crop and process each detected face
            for (size_t i = 0; i < boxes.size(); ++i)
            {
                // Adjust the rectangle to be slightly smaller to avoid saving the green box
                cv::Rect adjustedFace = boxes[i];
                int shrinkAmount = 3; // Shrink the rectangle by 1 pixel on all sides
                adjustedFace.x += shrinkAmount;
                adjustedFace.y += shrinkAmount;
                adjustedFace.width -= 2 * shrinkAmount; // 2 * shrinkAmount because we're shrinking from both sides
                adjustedFace.height -= 2 * shrinkAmount;

                // Ensure the adjusted rectangle remains within the frame boundaries
                adjustedFace.width = std::max(0, adjustedFace.width);
                adjustedFace.height = std::max(0, adjustedFace.height);

                // Perform cropping with the adjusted rectangle
                cv::Mat croppedFace = frame(adjustedFace);

                // Generate a unique filename
                stringstream filename;
                // UNCOMMENT IF YOU WANT TO PUT IN FOLDER INSTEAD
                // filename << OUTPUTIMAGESLOCATION << "/face_" << i << ".jpg";
                filename << "face_" << i << ".jpg";

                // Save the cropped face to a file
                cv::imwrite(filename.str(), croppedFace);
            }
            facesCaptured = true;
        };
    }

    // Function to check the bluriness of a face
    double checkBluriness(const cv::Mat &image)
    {
        // Convert the image to grayscale
        cv::Mat gray;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

        // Compute the Laplacian of the grayscale image
        cv::Mat laplacian;
        cv::Laplacian(gray, laplacian, CV_64F);

        // Compute the variance of Laplacian
        cv::Scalar mean, stddev;
        cv::meanStdDev(laplacian, mean, stddev);

        double variance = stddev.val[0] * stddev.val[0];

        return variance;
    }

    // Check game state from files
    void CheckGameState()
    {
        // Read content from files
        std::string temp1 = FileHandler::readFromFile(PLAYERSKEY);
        std::string temp2 = FileHandler::readFromFile(STARTKEY);

        // Trim leading and trailing whitespace
        temp1.erase(std::remove_if(temp1.begin(), temp1.end(), ::isspace), temp1.end());
        temp2.erase(std::remove_if(temp2.begin(), temp2.end(), ::isspace), temp2.end());

        try
        { // Another edge case scenario where the file might be empty, if so, return 0
            if (temp1.empty())
            {
                numberPlayers = 0;
            }
            else
            { // The value in the file is a string so make it an int
                numberPlayers = std::stoi(temp1);
            }
            if (temp2.empty())
            {
                gameStart = 0;
            }
            else
            {
                gameStart = std::stoi(temp2);
            }
        }
        catch (const std::invalid_argument &e)
        {
            // Handle invalid argument exception
            std::cerr << "Invalid argument: " << e.what() << std::endl;
            return;
        }

        // Check if game is ready to start
        if (gameStart != 0 && numberPlayers != 0)
        {
            readyToStart = true;
        }
        else
        {
            readyToStart = false;
        }
        // std::cout << "Number of players: " << numberPlayers << " Game Started? " << gameStart << " Ready To start? " << readyToStart << std::endl;
    }

    void logisch()
    {
        // Check if the game has started and how many players there are
        this->CheckGameState();
        if (facesCaptured)
        {
            // When there are the correct amount of faces detected check if they are usable

            bool isAFaceBlurry = false;

            for (int i = 0; i < numberPlayers; i++)
            {
                std::string filename;
                float blurrness;
                filename = "face_" + std::to_string(i) + ".jpg";
                cv::Mat face = cv::imread(filename);
                blurrness = checkBluriness(face);
                std::cout << "blurryness of face nr " << i << " is " << blurrness << std::endl;
                if (blurrness <= BLURRYNESSTHRESHHOLD)
                {
                    // If one face is too blurry stop checking the rest.
                    isAFaceBlurry = true;
                    break;
                }
            }
            if (isAFaceBlurry)
            {
                // Delete captured faces when they are not up to standard
                for (int i = 0; i < numberPlayers; i++)
                {
                    std::string filename;
                    filename = "face_" + std::to_string(i) + ".jpg";
                    std::remove(filename.c_str());
                } // Reset the variable to retry capturing all faces
                facesCaptured = false;
            }
            else
            {
                // If the faces where captured correctly, tell the system that it has been completed and reset the an idle state
                FileHandler::writeToFile("1", SCANNINGKEY);
                FileHandler::writeToFile("0", PLAYERSKEY);
                FileHandler::writeToFile("0", STARTKEY);

                facesCaptured = false;
                numberPlayers = 0;
                gameStart = 0;
                readyToStart = false;
                // TO:DO, Do creepy AI stuff
            }
        }
    }
};

int main()
{
    try
    {
        // Setup YOLO model
        auto yoloModel = std::make_unique<YoloModelV4>();
        yoloModel->loadModel(YOLO4CONFIG, YOLO4WEIGHTS);

        // Start webcam and face recognition
        FaceRecognitionHandler handler(0, std::move(yoloModel)); // Use camera index 0
        handler.captureAndProcess();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
