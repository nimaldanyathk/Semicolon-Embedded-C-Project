#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void load_templates(const char* datasetFolder);
void run_webcam_detection();

#ifdef __cplusplus
}
#endif

int main() {
    load_templates("Dataset_preprocessed"); // make sure folder exists
    run_webcam_detection();                  // webcam demo
    return 0;
}
