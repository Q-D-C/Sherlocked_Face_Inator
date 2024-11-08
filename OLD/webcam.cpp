// g++ -o webcam webcam.cpp `pkg-config --cflags --libs opencv4` -std=c++11

#include "opencv2/opencv.hpp"
#include <iostream>
#include <thread>
#include<unistd.h> 

using namespace std;
using namespace cv;

#define USEWEBCAM 1

#define CAMERAIP "http:cool_ip_ofz"

class WebcamHandler
{
public:
    // Constructor for hardware webcam
    WebcamHandler(int cameraIndex = 0) : isIPCamera(false), cameraIndex(cameraIndex), cap(cameraIndex, CAP_V4L) {}

    // Constructor for IP camera
    WebcamHandler(const string &cameraIP) : isIPCamera(true), cameraIP(cameraIP), cap(cameraIP, CAP_V4L) {}

    // Start capturing frames
    void startCapture()
    {
        captureThread = std::thread(&WebcamHandler::captureFrames, this);
    }

    // Stop capturing frames
    void stopCapture()
    {
        if (captureThread.joinable())
        {
            captureThread.join();
        }
    }

    // Set camera index for hardware webcam
    void setCameraIndex(int index)
    {
        if (!isIPCamera)
        {
            cameraIndex = index;
            cap.open(cameraIndex);
        }
    }

    // Set camera IP for IP camera
    void setCameraIP(const string &ip)
    {
        if (isIPCamera)
        {
            cameraIP = ip;
            cap.open(cameraIP);
        }
    }

private:
    bool isIPCamera; // Flag to identify camera type
    int cameraIndex; // Camera index for hardware webcam
    string cameraIP; //  Camera IP address
    VideoCapture cap;
    std::thread captureThread;

    void captureFrames()
    {
        for (;;)
        {
            Mat frame;
            cap >> frame;
            if (frame.empty())
                break; // end of video stream
            imshow("Camera Feed", frame);
            if (waitKey(10) == 27)
                break; // stop capturing by pressing ESC
        }
    }
};

int main(int argc, char **argv)
{

    // Use hardware webcam
    WebcamHandler webcamHardware(0);
    webcamHardware.startCapture();

    // Use IP camera
    // WebcamHandler webcamIP(CAMERAIP);
    // webcamIP.startCapture();

    // Main thread can perform other tasks if needed

    // Wait for the capture threads to finish
    webcamHardware.stopCapture();
    // webcamIP.stopCapture();

    // The cameras will be closed automatically upon exit
    return 0;
}
