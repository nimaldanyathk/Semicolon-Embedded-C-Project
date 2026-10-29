#include "currency_detector.h"
#include <opencv2/opencv.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (0)

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("semicolon-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        CHECK(currency_load_templates(nullptr) == -1);
        CHECK(currency_load_templates(root.string().c_str()) == -1);
        std::filesystem::create_directories(root / "100");
        CHECK(currency_load_templates(root.string().c_str()) == 0);
        cv::Mat reference(100, 200, CV_8UC1);
        cv::RNG rng(9128);
        rng.fill(reference, cv::RNG::UNIFORM, 0, 256);
        // Invalid entries must not exhaust the ten VALID-image limit.
        for (int i = 0; i < 12; ++i) std::ofstream(root / "100" / ("0-" + std::to_string(i) + ".txt")) << "invalid";
        cv::imwrite((root / "100" / "1-flat.png").string(), cv::Mat(100, 200, CV_8UC1, cv::Scalar(127)));
        cv::imwrite((root / "100" / "2-reference.png").string(), reference);
        CHECK(currency_load_templates(root.string().c_str()) == 1);
        CHECK(currency_match(reference.data, 100, 200) == 100);
        CHECK(currency_match(nullptr, 100, 200) == -1);
        CHECK(currency_match(reference.data, -1, 200) == -1);
        CHECK(currency_match(reference.data, 50, 50) == 0);
        cv::Mat flat(100, 200, CV_8UC1, cv::Scalar(127));
        CHECK(currency_match(flat.data, 100, 200) == 0);
        cv::Mat unrelated(100, 200, CV_8UC1);
        rng.fill(unrelated, cv::RNG::UNIFORM, 0, 256);
        CHECK(currency_match(unrelated.data, 100, 200) == 0);
        CHECK(currency_load_templates("") == -1);
        CHECK(currency_match(reference.data, 100, 200) == -1); // Old templates were cleared.
        std::filesystem::remove_all(root);
        std::cout << "Currency regression tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(root);
        return 1;
    }
}
