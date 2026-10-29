#include "currency_detector.h"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
extern char **environ;
#endif

namespace {
struct NoteTemplate { int denomination; cv::Mat image; };
std::vector<NoteTemplate> templates;
constexpr int templateWidth = 200, templateHeight = 100;
constexpr double threshold = 0.7;

bool hasDetail(const cv::Mat &image) {
    cv::Scalar mean, deviation;
    cv::meanStdDev(image, mean, deviation);
    return deviation[0] >= 1.0; // Constant templates give misleading normalized correlations.
}

class Speech {
#ifdef __APPLE__
    pid_t child = -1;
#endif
public:
    void say(int denomination) {
#ifdef __APPLE__
        if (child > 0 && waitpid(child, nullptr, WNOHANG) == 0) return;
        child = -1;
        std::string text = std::to_string(denomination) + " rupee note detected";
        char executable[] = "/usr/bin/say";
        char *args[] = {executable, text.data(), nullptr};
        if (posix_spawn(&child, executable, nullptr, nullptr, args, environ) != 0) child = -1;
#else
        (void)denomination;
#endif
    }
    ~Speech() {
#ifdef __APPLE__
        if (child > 0 && waitpid(child, nullptr, WNOHANG) == 0) {
            kill(child, SIGTERM);
            waitpid(child, nullptr, 0);
        }
#endif
    }
};
}

extern "C" int currency_load_templates(const char *datasetFolder) {
    templates.clear();
    try {
        if (!datasetFolder || !*datasetFolder || !std::filesystem::is_directory(datasetFolder)) {
            std::cerr << "Dataset directory is missing.\n";
            return -1;
        }
        std::vector<NoteTemplate> loaded;
        for (int denomination : {100, 200, 500}) {
            const auto folder = std::filesystem::path(datasetFolder) / std::to_string(denomination);
            if (!std::filesystem::is_directory(folder)) continue;
            std::vector<std::filesystem::path> files;
            for (const auto &file : std::filesystem::directory_iterator(folder)) {
                if (file.is_regular_file()) files.push_back(file.path());
            }
            std::sort(files.begin(), files.end());
            int count = 0;
            for (const auto &file : files) {
                if (count == 10) break;
                try {
                    cv::Mat image = cv::imread(file.string(), cv::IMREAD_GRAYSCALE);
                    if (image.empty()) continue;
                    cv::resize(image, image, cv::Size(templateWidth, templateHeight));
                    if (!hasDetail(image)) continue;
                    loaded.push_back({denomination, image});
                    ++count;
                } catch (const cv::Exception &) {
                    std::cerr << "Skipping unreadable image: " << file.filename() << '\n';
                }
            }
        }
        templates = std::move(loaded);
        if (templates.empty()) std::cerr << "No usable reference images in 100/, 200/, or 500/.\n";
        return static_cast<int>(templates.size());
    } catch (const std::exception &error) {
        std::cerr << "Cannot load dataset: " << error.what() << '\n';
        return -1;
    } catch (...) { return -1; }
}

extern "C" int currency_match(const unsigned char *data, int rows, int cols) {
    if (!data || rows <= 0 || cols <= 0 || templates.empty()) return -1;
    if (rows < templateHeight || cols < templateWidth) return 0;
    try {
        const cv::Mat image(rows, cols, CV_8UC1, const_cast<unsigned char *>(data));
        if (!hasDetail(image)) return 0;
        double best = threshold;
        int denomination = 0;
        for (const auto &note : templates) {
            cv::Mat result;
            cv::matchTemplate(image, note.image, result, cv::TM_CCOEFF_NORMED);
            double score;
            cv::minMaxLoc(result, nullptr, &score);
            if (std::isfinite(score) && score >= best) {
                best = score;
                denomination = note.denomination;
            }
        }
        return denomination;
    } catch (const std::exception &error) {
        std::cerr << "Matching failed: " << error.what() << '\n';
        return -1;
    } catch (...) { return -1; }
}

extern "C" int currency_run_webcam(int cameraIndex, int enableSpeech) {
    if (cameraIndex < 0 || templates.empty()) return -1;
#ifndef __APPLE__
    if (enableSpeech) { std::cerr << "--speak is supported on macOS only.\n"; return -1; }
#endif
    try {
        cv::VideoCapture camera(cameraIndex);
        if (!camera.isOpened()) { std::cerr << "Cannot open webcam. Check device index and permissions.\n"; return -1; }
        Speech speech;
        int lastAnnounced = 0, unknownFrames = 0, failures = 0;
        for (;;) {
            cv::Mat frame, gray;
            if (!camera.read(frame) || frame.empty()) {
                if (++failures >= 5) { cv::destroyAllWindows(); std::cerr << "Camera stopped returning frames.\n"; return -1; }
                if (cv::waitKey(50) == 27) break;
                continue;
            }
            failures = 0;
            cv::resize(frame, gray, cv::Size(templateWidth, templateHeight));
            cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
            const int note = currency_match(gray.data, gray.rows, gray.cols);
            if (note < 0) { cv::destroyAllWindows(); return -1; }
            if (note == 0) {
                unknownFrames = std::min(unknownFrames + 1, 10);
                if (unknownFrames == 10) lastAnnounced = 0;
            }
            else {
                unknownFrames = 0;
                if (note != lastAnnounced) {
                    std::cout << note << " rupee note (template match)\n";
                    if (enableSpeech) speech.say(note);
                    lastAnnounced = note;
                }
            }
            cv::putText(frame, note ? "INR " + std::to_string(note) : "Unknown", cv::Point(20, 40),
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            cv::imshow("Semicolon - currency template demo (ESC to close)", frame);
            if (cv::waitKey(1) == 27) break;
        }
        cv::destroyAllWindows();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Webcam demo failed: " << error.what() << '\n';
        try { cv::destroyAllWindows(); } catch (...) {}
        return -1;
    } catch (...) { return -1; }
}
