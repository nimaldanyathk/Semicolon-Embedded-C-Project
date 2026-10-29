#ifndef CURRENCY_DETECTOR_H
#define CURRENCY_DETECTOR_H
#ifdef __cplusplus
extern "C" {
#endif
// Single-threaded API. Errors return -1; diagnostic text goes to stderr.
int currency_load_templates(const char *dataset_folder);
// Tightly packed 8-bit grayscale image. Returns denomination, 0 (unknown), or -1.
int currency_match(const unsigned char *data, int rows, int cols);
int currency_run_webcam(int camera_index, int enable_speech);
#ifdef __cplusplus
}
#endif
#endif
