// g++ -o herken herken.cpp `pkg-config --cflags --libs opencv4` -std=c++11
#include <opencv2/opencv.hpp>

int main() {
cv::CascadeClassifier face_cascade("/home/nino/req xml/haarcascade_frontalface_default.xml");
if (face_cascade.empty()) {
    std::cerr << "Error: Unable to load face cascade classifier." << std::endl;
    return -1;
}


    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error: Unable to open webcam." << std::endl;
        return -1;
    }

    while (true) {
        cv::Mat frame;
        cap.read(frame);
        if (frame.empty()) {
            std::cerr << "Error: Couldn't capture a frame." << std::endl;
            break;
        }

        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        std::vector<cv::Rect> faces;
        face_cascade.detectMultiScale(gray, faces, 1.3, 5);

        for (const auto& face : faces) {
            cv::rectangle(frame, face, cv::Scalar(255, 0, 0), 2);
            cv::Mat roi_gray = gray(face);
            cv::Mat roi_color = frame(face);
        }

        cv::imshow("img", frame);
        int key = cv::waitKey(30);
        if (key == 27) {
            break;  // Press Esc to stop the video
        }
    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}
