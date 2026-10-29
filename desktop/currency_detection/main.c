#include "currency_detector.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4 || strcmp(argv[1], "--help") == 0) {
        fprintf(argc >= 2 ? stdout : stderr, "Usage: currency_demo DATASET [CAMERA_INDEX] [--speak]\n"
                "DATASET must contain 100/, 200/, and/or 500/ reference images.\n"
                "Speech is optional and supported on macOS only. ESC closes the preview.\n");
        return argc == 2 && strcmp(argv[1], "--help") == 0 ? 0 : 2;
    }
    int camera = 0, speak = 0;
    if (argc >= 3) {
        char *end = NULL;
        errno = 0;
        long value = strtol(argv[2], &end, 10);
        if (errno || end == argv[2] || *end || value < 0 || value > 255) {
            fprintf(stderr, "CAMERA_INDEX must be an integer from 0 to 255.\n");
            return 2;
        }
        camera = (int)value;
    }
    if (argc == 4) {
        if (strcmp(argv[3], "--speak") != 0) { fprintf(stderr, "Unknown option: %s\n", argv[3]); return 2; }
        speak = 1;
    }
    if (currency_load_templates(argv[1]) <= 0) return 1;
    return currency_run_webcam(camera, speak) == 0 ? 0 : 1;
}
