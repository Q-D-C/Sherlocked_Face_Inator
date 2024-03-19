// g++ -o herken herken.cpp `pkg-config --cflags --libs opencv4` -std=c++11

#include "opencv2/opencv.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <unistd.h>
#include <cstdio>

// Values for the size of the frame
#define FRAMEWIDTH 320
#define FRAMEHEIGHT 320

// Define how much room (in pixels) you want around the found face
#define EXPANSIONPIXELS 50

// Define a value for how blurry the picture is allowed to be, the lower the blurrier
#define BLURRYNESSTHRESHHOLD 100

// keys for what the files are called
#define SCANNINGKEY "scanningComplete"
#define STARTKEY "gameStart"
#define PLAYERSKEY "numPlayers"

// paths to the YOLO configs and weighsts
#define YOLO8CONFIG "/home/nino/facemodels/deploy.prototxt"
#define YOLO4CONFIG "/home/nino/face-detection-yolov4-tiny-master/yolo/yolov4-tiny-3l.cfg"
#define YOLOCONFIG "/home/nino/facemodels/cfg/yolov3-face.cfg"

#define YOLO8WEIGHTS "/home/nino/facemodels/res10_300x300_ssd_iter_140000_fp16.caffemodel"
#define YOLO4WEIGHTS "/home/nino/face-detection-yolov4-tiny-master/yolo/yolov4-tiny-3l_best.weights"
#define YOLOWEIGHTS "/home/nino/facemodels/model-weights/yolov3-wider_16000.weights"

// Path to where to final faces will be saved
#define OUTPUTIMAGESLOCATION "/home/nino/Sherlocked_Face_Inator/FACES"

// Do you want to see what the webcam sees, yay or nay?
bool showFrame = false;

using namespace std;
using namespace cv;

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

// Class for handling webcam operations
class WebcamHandler
{
public:
    vector<Rect> faces; // Vector to store the face locations

    // Making sure that you can init the webcam handler with a hardware cam and also a IP cam

    WebcamHandler() : isIPCamera(false), cameraIndex(0), cap(0, CAP_V4L) {}

    WebcamHandler(int cameraIndex) : isIPCamera(false), cameraIndex(cameraIndex), cap(cameraIndex, CAP_V4L) {}

    WebcamHandler(const string &cameraIP) : isIPCamera(true), cameraIP(cameraIP), cap(cameraIP, CAP_V4L) {}

    void startCapture()
    {
        // Start capturing frames in a separate thread
        captureThread = thread(&WebcamHandler::captureFrames, this);
    }

    void stopCapture()
    {
        // Stop capturing frames if thread is joinable
        if (captureThread.joinable())
        {
            captureThread.join();
        }
    }

    void setCameraIndex(int index)
    {
        // if there isNT an ip camera then we are setting up a hardware cam
        if (!isIPCamera)
        {
            cameraIndex = index;
            cap.open(cameraIndex);
        }
    }

    void setCameraIP(const string &ip)
    { // if there is an IP camera, set that up
        if (isIPCamera)
        {
            cameraIP = ip;
            cap.open(cameraIP);
        }
    }

protected:
    bool isIPCamera;      // Flag to identify camera type
    int cameraIndex;      // Camera index for hardware webcam
    string cameraIP;      // Camera IP address
    VideoCapture cap;     // Video capture object
    thread captureThread; // Thread for capturing frames

    // making a polymorphic solution to process framez, this way it can be overwritten by children classes
    virtual void processFrame(Mat &frame) = 0;

private:
    // Function to capture frames for EVER
    void captureFrames()
    {
        for (;;)
        {
            Mat frame;    // creating a mat
            cap >> frame; // pushing the current frame to said mat
            if (frame.empty())
                break;           // stop if fails
            processFrame(frame); // process the frame
            if (waitKey(10) == 27)
                break; // stop capturing by pressing ESC, hopefully never needed but just in case
        }
    }
};

// Class for handling face recognition
class FaceRecognitionHandler : public WebcamHandler
{

public:
    // Constructor initializes YOLO model
    FaceRecognitionHandler(int cameraIndex) : WebcamHandler(cameraIndex)
    {
        // confirm the correct weights and configs
        yolo_net = cv::dnn::readNetFromDarknet(YOLOCONFIG, YOLOWEIGHTS);
        yolo_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        yolo_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        if (yolo_net.empty())
        {
            cerr << "Error: Unable to load YOLO model." << std::endl;
            exit(-1);
        }
    }

    // Function to perform face recognition logic
    void logisch()
    {
        while (1)
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
                    facesCaptured = false;
                    numberPlayers = 0;
                    gameStart = 0;
                    readyToStart = false;
                    // TO:DO, Do creepy AI stuff
                }
            }
        }
    }

private:
    int numberPlayers = 0;
    int gameStart = 0;
    bool readyToStart = false;
    bool facesCaptured = false;

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


    // Initialize YOLO neural network and process each frame for face recognition
    cv::dnn::Net yolo_net;
    void processFrame(Mat &frame) override
    {
        if (readyToStart)
        {
            // remove past found faces from memory
            faces.clear();

            Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, Size(FRAMEWIDTH, FRAMEHEIGHT), Scalar(0, 0, 0), true, false);
            yolo_net.setInput(blob);

            // Convert frame to YOLO input blob
            vector<Mat> outs;
            yolo_net.forward(outs, getOutputNames(yolo_net));

            // post process just the frames that have faces
            postprocess(frame, outs, faces);

            // std::cout << "Number of faces found: " << faces.size() << std::endl;

            for (const auto &face : faces)
            {
                // Draw rectangle around each detected face
                rectangle(frame, face, Scalar(255, 0, 0), 2);
            }
        }

        // Display the frame, if wanted
        if (showFrame)
            imshow("YOLO Face Recognition", frame);
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

    // Post-process YOLO output for face recognition
    void postprocess(const cv::Mat &frame, const vector<cv::Mat> &outs, vector<cv::Rect> &faces)
    {
        // define how sure it has to be to call it a face
        float confThreshold = 0.5;
        float nmsThreshold = 0.4;

        vector<int> classIds;
        vector<float> confidences;
        vector<cv::Rect> boxes;

        string saveFolder = OUTPUTIMAGESLOCATION;

        for (const auto &out : outs)
        {
            for (int i = 0; i < out.rows; ++i)
            { // for each detected bolb see if its confident enough for it to be a face
                auto data = out.ptr<float>(i);
                float confidence = data[4];
                if (confidence > confThreshold)
                {
                    // if its confident enough calulate where said face is and mke a rectangle at that location
                    int centerX = static_cast<int>(data[0] * frame.cols); // Calculate the x-coordinate of the center of the bounding box
                    int centerY = static_cast<int>(data[1] * frame.rows); // Calculate the y-coordinate of the center of the bounding box
                    int width = static_cast<int>(data[2] * frame.cols);   // Calculate the width of the bounding box
                    int height = static_cast<int>(data[3] * frame.rows);  // Calculate the height of the bounding box

                    int left = centerX - width / 2; // Calculate the x-coordinate of the left-top corner of the bounding box
                    int top = centerY - height / 2; // Calculate the y-coordinate of the left-top corner of the bounding box

                    // Expand the bounding box by adding/subtracting expansionPixels
                    left = std::max(0, left - EXPANSIONPIXELS);
                    top = std::max(0, top - EXPANSIONPIXELS);
                    width = std::min(frame.cols - left, width + 2 * EXPANSIONPIXELS);
                    height = std::min(frame.rows - top, height + 2 * EXPANSIONPIXELS);

                    // Create a rectangle representing the face
                    cv::Rect box(left, top, width, height);

                    // Store the class ID, confidence, and bounding box
                    boxes.push_back(box);
                    confidences.push_back(confidence);
                    classIds.push_back(i);
                }
            }
        }

        // filter out overlapping bounding boxes
        vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

        // Check if the correct amount of faces have been detected

        CheckAndSafeFaces(indices, boxes, frame);
    }

    // Check if the correct amount of faces have been detected
    void CheckAndSafeFaces(vector<int> indices, vector<cv::Rect> boxes, const cv::Mat &frame)
    {

        if (indices.size() >= numberPlayers && facesCaptured == false)
        {

            std::cout << "Number of faces found: " << indices.size() << std::endl;

            // Crop and process each detected face
            for (size_t i = 0; i < indices.size(); ++i)
            {
                int idx = indices[i];
                cv::Rect face = boxes[idx];

                // Perform cropping
                cv::Mat croppedFace = frame(face);

                // Generate a unique filename
                stringstream filename;
                // UNCOMMENT IF YOU WANT TO PUT IN FOLDER INSTEAD
                // filename << OUTPUTIMAGESLOCATION << "/face_" << i << ".jpg";
                filename << "face_" << i << ".jpg";

                // Save the cropped face to a file
                cv::imwrite(filename.str(), croppedFace);
            }
            facesCaptured = true;
        }

        // add found faces to the 'faces' vector with the final list of detected faces
        for (size_t i = 0; i < indices.size(); ++i)
        {
            int idx = indices[i];
            faces.push_back(boxes[idx]);
        }
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
        {// Another edge case scenario where the file might be empty, if so, return 0
            if (temp1.empty())
            {
                numberPlayers = 0;
            }
            else
            {// The value in the file is a string so make it an int
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
};

int main()
{
    // start face recognising on hardware camera
    FaceRecognitionHandler webcamFaceRecognition(0);
    webcamFaceRecognition.startCapture();
    webcamFaceRecognition.logisch();
    
    // Use additional cameras if needed
    // FaceRecognitionHandler webcamFaceRecognition2(1);
    // webcamFaceRecognition2.startCapture();

    // Use IP camera if needed
    // FaceRecognitionHandler IPcamFaceRecognition(CAMERAIP);
    // IPcamFaceRecognition.startCapture();

    // Wait for the capture threads to finish
    webcamFaceRecognition.stopCapture();
    // webcamFaceRecognition2.stopCapture();
    // IPcamFaceRecognition.stopCapture();

    return 0;
}
