#include <opencv2/opencv.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>

namespace fs = std::filesystem;

struct NoteTemplate {
    std::string denomination;
    cv::Mat image;
};

static std::vector<NoteTemplate> templates;

extern "C" {

// Load templates
void load_templates(const char* datasetFolder) {
    templates.clear();
    std::vector<std::string> denominations = {"100","200","500"};
    for(auto &noteValue : denominations){
        std::string folderPath = std::string(datasetFolder) + "/" + noteValue;
        if(!fs::exists(folderPath)) continue;

        int count = 0;
        for(const auto &file : fs::directory_iterator(folderPath)){
            if(count++ >= 10) break;
            cv::Mat img = cv::imread(file.path().string(), cv::IMREAD_GRAYSCALE);
            if(img.empty()) continue;
            cv::resize(img, img, cv::Size(200,100));
            templates.push_back({noteValue, img});
        }
    }
    std::cout << "[INFO] Loaded " << templates.size() << " templates." << std::endl;
}

// Match note
const char* match_note(unsigned char* image_data, int rows, int cols) {
    cv::Mat note(rows, cols, CV_8UC1, image_data);
    double maxScore = 0.0;
    const char* bestNote = "None";
    for(const auto &tpl : templates){
        cv::Mat result;
        cv::matchTemplate(note, tpl.image, result, cv::TM_CCOEFF_NORMED);
        double minVal, maxVal;
        cv::minMaxLoc(result, &minVal, &maxVal);
        if(maxVal > maxScore){
            maxScore = maxVal;
            bestNote = tpl.denomination.c_str();
        }
    }
    if(maxScore < 0.7) return "None"; // threshold
    return bestNote;
}

// Simple webcam detection
void run_webcam_detection() {
    if(templates.empty()){
        std::cout << "[ERROR] Templates not loaded!" << std::endl;
        return;
    }

    cv::VideoCapture cap(0);
    if(!cap.isOpened()){
        std::cout << "[ERROR] Cannot open webcam!" << std::endl;
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    std::string lastAnnounced = "";
    cv::Mat frame;

    std::cout << "[INFO] Webcam Currency Detection Started. Press ESC to exit." << std::endl;

    while(true){
        cap >> frame;
        if(frame.empty()) continue;

        cv::Mat note;
        cv::resize(frame, note, cv::Size(200,100));
        cv::cvtColor(note, note, cv::COLOR_BGR2GRAY);

        const char* detected = match_note(note.data, note.rows, note.cols);

        cv::putText(frame, detected, cv::Point(50,50),
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0,255,0), 2);
        cv::imshow("Currency Detection", frame);
        cv::imshow("Warped Note", note);

        if(std::string(detected) != "None" && std::string(detected) != lastAnnounced){
            std::string cmd = "say " + std::string(detected) + " rupee note detected";
            system(cmd.c_str());
            lastAnnounced = detected;
        }

        if(cv::waitKey(1) == 27) break; // ESC
    }

    cap.release();
    cv::destroyAllWindows();
}

} // extern "C"
