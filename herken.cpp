// g++ -o herken herken.cpp `pkg-config --cflags --libs opencv4` -std=c++11
#include <opencv2/opencv.hpp>

std::vector<cv::String> getOutputNames(const cv::dnn::Net &net)
{
    static std::vector<cv::String> names;
    if (names.empty())
    {
        std::vector<int> outLayers = net.getUnconnectedOutLayers();
        std::vector<cv::String> layersNames = net.getLayerNames();
        names.resize(outLayers.size());
        for (size_t i = 0; i < outLayers.size(); ++i)
        {
            names[i] = layersNames[outLayers[i] - 1];
        }
    }
    return names;
}

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
        {
            auto data = out.ptr<float>(i);
            float confidence = data[4];
            if (confidence > confThreshold)
            {
                int centerX = static_cast<int>(data[0] * frame.cols);
                int centerY = static_cast<int>(data[1] * frame.rows);
                int width = static_cast<int>(data[2] * frame.cols);
                int height = static_cast<int>(data[3] * frame.rows);

                int left = centerX - width / 2;
                int top = centerY - height / 2;

                cv::Rect box(left, top, width, height);
                boxes.push_back(box);
                confidences.push_back(confidence);
                classIds.push_back(i);
            }
        }
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

    for (size_t i = 0; i < indices.size(); ++i)
    {
        int idx = indices[i];
        faces.push_back(boxes[idx]);
    }
}

int main()
{
    cv::dnn::Net yolo_net = cv::dnn::readNetFromDarknet("cfg/yolov3-face.cfg", "model-weights/yolov3-wider_16000.weights");
    yolo_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    yolo_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    
    if (yolo_net.empty())
    {
        std::cerr << "Error: Unable to load YOLO model." << std::endl;
        return -1;
    }

    cv::VideoCapture cap(0, cv::CAP_V4L);
    if (!cap.isOpened())
    {
        std::cerr << "Error: Unable to open webcam." << std::endl;
        return -1;
    }

    while (true)
    {
        cv::Mat frame;
        cap.read(frame);
        if (frame.empty())
        {
            std::cerr << "Error: Couldn't capture a frame." << std::endl;
            break;
        }

        cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, cv::Size(320, 320), cv::Scalar(0, 0, 0), true, false);
        yolo_net.setInput(blob);

        std::vector<cv::Mat> outs;
        yolo_net.forward(outs, getOutputNames(yolo_net));

        std::vector<cv::Rect> faces;
        postprocess(frame, outs, faces);

        for (const auto &face : faces)
        {
            cv::rectangle(frame, face, cv::Scalar(255, 0, 0), 2);
        }

        cv::imshow("YOLO Face Recognition", frame);
        int key = cv::waitKey(30);
        if (key == 27)
        {
            break; // Press Esc to stop the video
        }
    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}
