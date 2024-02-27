// g++ -o herken herken.cpp `pkg-config --cflags --libs opencv4` -std=c++11

#include "opencv2/opencv.hpp"
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace std;
using namespace cv;

class WebcamHandler
{
public:
    // making sure that you can init the webcam handler with a hardware cam and also a IP cam

    WebcamHandler() : isIPCamera(false), cameraIndex(0), cap(0, CAP_V4L) {}

    WebcamHandler(int cameraIndex) : isIPCamera(false), cameraIndex(cameraIndex), cap(cameraIndex, CAP_V4L) {}

    WebcamHandler(const string &cameraIP) : isIPCamera(true), cameraIP(cameraIP), cap(cameraIP, CAP_V4L) {}

    void startCapture()
    {
        // Start capturing frames in a separate thread
        captureThread = std::thread(&WebcamHandler::captureFrames, this);
    }

    void stopCapture()
    {
        // Stop capturing said frames
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
    bool isIPCamera;           // Flag to identify camera type
    int cameraIndex;           // Camera index for hardware webcam
    string cameraIP;           // Camera IP address
    VideoCapture cap;          // Video capture object
    std::thread captureThread; // Thread for capturing frames
    vector<Rect> faces;        // Vector to store the face locations

    // making a polymorphic solution to process framez, this way it can be overwritten by children classes
    virtual void processFrame(Mat &frame) = 0;

private:
    void captureFrames()
    {
        for (;;)
        {
            Mat frame;    // creating a mat
            cap >> frame; // pushing the current frame to said may
            if (frame.empty())
                break;           // stop if fails
            processFrame(frame); // process the frame
            if (waitKey(10) == 27)
                break; // stop capturing by pressing ESC
                       // print results
            for (int i = 0; i < faces.size(); i++)
            {
                cout << faces[i] << " ";
            }
            cout << endl;
        }
    }
};

class FaceRecognitionHandler : public WebcamHandler
{

public:
    // Constructor initializes YOLO model
    FaceRecognitionHandler(int cameraIndex) : WebcamHandler(cameraIndex)
    {
        // yolo_net = cv::dnn::readNetFromDarknet("yolov3-face.cfg", "yolov3-wider_16000.weights");
        yolo_net = cv::dnn::readNetFromDarknet("/home/nino/facemodels/cfg/yolov3-face.cfg", "/home/nino/facemodels/model-weights/yolov3-wider_16000.weights");
        yolo_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        yolo_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        if (yolo_net.empty())
        {
            cerr << "Error: Unable to load YOLO model." << endl;
            exit(-1);
        }
    }

protected:
    // init the YOLO neural network
    cv::dnn::Net yolo_net;

    // Process each frame for face recognition
    void processFrame(Mat &frame) override
    {
        // remove past found faces from memoomoty
        faces.clear();

        Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, Size(320, 320), Scalar(0, 0, 0), true, false);
        yolo_net.setInput(blob);

        // Convert frame to YOLO input blob
        vector<Mat> outs;
        yolo_net.forward(outs, getOutputNames(yolo_net));

        // post process just the frames that have faces
        postprocess(frame, outs, faces);

        cout << "Number of faces found: " << faces.size() << endl;

        for (const auto &face : faces)
        {
            // Draw rectangle around each detected face
            rectangle(frame, face, Scalar(255, 0, 0), 2);
        }

        // Display the frame with face recognition
        imshow("YOLO Face Recognition", frame);
    }

    // Get names of YOLO output layers
    std::vector<cv::String> getOutputNames(const cv::dnn::Net &net)
    {
        static std::vector<cv::String> names;
        if (names.empty())
        {
            // retrieve all layer names from the yolo network and resize the name vector accordingly
            std::vector<int> outLayers = net.getUnconnectedOutLayers();
            std::vector<cv::String> layersNames = net.getLayerNames();
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
    void postprocess(const cv::Mat &frame, const std::vector<cv::Mat> &outs, std::vector<cv::Rect> &faces)
    {
        float confThreshold = 0.5;
        float nmsThreshold = 0.4;

        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

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
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

        // add found faces to the 'faces' vector with the final list of detected faces
        for (size_t i = 0; i < indices.size(); ++i)
        {
            int idx = indices[i];
            faces.push_back(boxes[idx]);
        }
        // print results
        //  for (int i = 0; i < faces.size(); i++)
        //  {
        //      cout << faces[i] << " ";
        //  }
        //  cout << endl;
    }
};

int main()
{
    // start face recognising on hardware camera
    FaceRecognitionHandler webcamFaceRecognition(0);
    webcamFaceRecognition.startCapture();

    // Use additional cameras if needed
    // FaceRecognitionHandler webcamFaceRecognition2(1);
    // webcamFaceRecognition2.startCapture();

    // Use IP camera if needed
    // FaceRecognitionHandler IPcamFaceRecognition(CAMERAIP);
    // IPcamFaceRecognition.startCapture();

    // Main thread can perform other tasks if needed

    // Wait for the capture threads to finish
    webcamFaceRecognition.stopCapture();
    // webcamFaceRecognition2.stopCapture();
    // IPcamFaceRecognition.stopCapture();

    return 0;
}
