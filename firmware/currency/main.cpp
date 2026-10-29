/*
 * Based on the project's Edge Impulse Arduino example.
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <Arduino.h>
#if !__has_include(<c_currency_inferencing.h>)
#error "Missing trained model: export c_currency as an Edge Impulse Arduino library into lib/. See docs/currency.md."
#else
#include <c_currency_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "img_converters.h"
#include "esp_heap_caps.h"
#include "camera_board.h"

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "The exported model must use a camera input."
#endif
static_assert(EI_CLASSIFIER_INPUT_WIDTH > 0 && EI_CLASSIFIER_INPUT_WIDTH <= 320, "Model width must be 1–320");
static_assert(EI_CLASSIFIER_INPUT_HEIGHT > 0 && EI_CLASSIFIER_INPUT_HEIGHT <= 240, "Model height must be 1–240");

constexpr size_t rawBytes = 320 * 240 * 3;
constexpr size_t inputPixels = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
static uint8_t *rawImage = nullptr, *modelImage = nullptr;
static bool ready = false;

static int getImageData(size_t offset, size_t length, float *out) {
    if (!ready || !out || offset > inputPixels || length > inputPixels - offset) return -1;
    for (size_t i = 0; i < length; ++i) {
        const size_t pixel = (offset + i) * 3;
        // Pinned esp32-camera JPEG converter yields BGR byte order.
        out[i] = static_cast<float>((static_cast<uint32_t>(modelImage[pixel + 2]) << 16) |
                                  (static_cast<uint32_t>(modelImage[pixel + 1]) << 8) | modelImage[pixel]);
    }
    return 0;
}

void setup() {
    Serial.begin(115200);
    if (!psramFound()) { Serial.println("Currency inference requires working PSRAM."); return; }
    auto config = semicolonCameraConfig();
    if (esp_camera_init(&config) != ESP_OK) { Serial.println("Camera initialization failed."); return; }
    rawImage = static_cast<uint8_t *>(heap_caps_malloc(rawBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    modelImage = static_cast<uint8_t *>(heap_caps_malloc(inputPixels * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!rawImage || !modelImage) {
        free(rawImage); free(modelImage);
        rawImage = modelImage = nullptr;
        esp_camera_deinit();
        Serial.println("Not enough PSRAM for image buffers.");
        return;
    }
    ready = true;
    Serial.println("Currency inference ready; predictions are printed over serial.");
}

void loop() {
    delay(100);
    if (!ready) return;
    camera_fb_t *frame = esp_camera_fb_get();
    if (!frame) { Serial.println("Capture failed."); return; }
    const bool dimensionsOk = frame->width == 320 && frame->height == 240 && frame->format == PIXFORMAT_JPEG;
    const bool converted = dimensionsOk && fmt2rgb888(frame->buf, frame->len, frame->format, rawImage);
    esp_camera_fb_return(frame);
    if (!converted) { Serial.println("Unexpected frame or RGB conversion failed."); return; }
    const int resized = ei::image::processing::crop_and_interpolate_rgb888(
        rawImage, 320, 240, modelImage, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
    if (resized != 0) { Serial.println("Image resize failed."); return; }
    ei::signal_t signal;
    signal.total_length = inputPixels;
    signal.get_data = getImageData;
    ei_impulse_result_t result = {};
    const auto error = run_classifier(&signal, &result, false);
    if (error != EI_IMPULSE_OK) { Serial.printf("Inference failed: %d\n", error); return; }
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    for (size_t i = 0; i < result.bounding_boxes_count; ++i) {
        const auto &box = result.bounding_boxes[i];
        if (box.value > 0) Serial.printf("%s: %.3f (x=%u, y=%u, w=%u, h=%u)\n",
            box.label, box.value, box.x, box.y, box.width, box.height);
    }
#else
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
        Serial.printf("%s: %.3f\n", result.classification[i].label, result.classification[i].value);
    }
#endif
}
#endif
