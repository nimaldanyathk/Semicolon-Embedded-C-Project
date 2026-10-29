# Currency recognition

There are two independent experiments. Neither includes a trained dataset in
this repository, and neither should be described as validated banknote recognition.

| Target | Input | Algorithm | Output | Missing dependency |
| --- | --- | --- | --- | --- |
| `currency-esp32cam` | OV2640 camera | Edge Impulse exported model | Serial predictions | Original `c_currency` Arduino model export |
| `currency_demo` | Desktop webcam | OpenCV normalized template correlation | Preview, console, optional macOS speech | Reference images in `100/`, `200/`, `500/` |

## ESP32 model adapter

The original `EdgeImpulseModel.c` was an Arduino/C++ inference adapter, not the
trained model itself. Its `c_currency_inferencing.h`, model parameters, SDK, and
training/evaluation data were not committed. The adapter now lives at
[`firmware/currency/main.cpp`](../firmware/currency/main.cpp).

The model is intentionally **not** part of default builds or CI. Selecting this
target without its library produces a clear missing-model error. Synthetic host
tests do not validate banknote recognition accuracy.

To restore it in the future:

1. Recover or train an appropriately licensed Edge Impulse camera model using
   representative Indian banknote images, negative/background examples, and a
   genuinely separate evaluation set.
2. Export the **Arduino library**, including the generated header, SDK, and
   model files. Keep the export's license and version information.
3. Extract the library beneath `lib/`, for example
   `lib/c_currency/src/c_currency_inferencing.h`. Local model libraries are
   ignored by Git. If your export uses a different header name, update the
   include and the `__has_include` check in the adapter.
4. Use camera input dimensions no larger than **320 × 240**. The adapter allocates
   a raw RGB buffer plus a separate input buffer in PSRAM and rejects dimensions
   outside that bound at compile time.
5. Build with `pio run -e currency-esp32cam`, upload using the same environment,
   and inspect predictions at 115200 baud. If the export requires a different
   Arduino/SDK version, validate it separately before changing the pinned stack.
6. Verify preprocessing and channel order against the training pipeline using
   known images. Test the model on the actual camera, lighting, and note variants.

The adapter checks initialization/allocation failures, returns framebuffers on
error, reuses allocations, checks resize results, and bounds the classifier's
pixel callback. It supports serial classification or object-detection output
according to the exported model. It does not add denomination labels, confidence
guarantees, counterfeit detection, speech, or navigation integration.

### Public references

A search during the repository refresh did not locate the original `c_currency`
export. Circuit Digest publishes an
[Indian currency recognition example](https://github.com/Circuit-Digest/ESP32-CAM-Based-Indian-Currency-Recognition-System-using-Edge-Impulse.)
with a training workflow. This is a reference for recreating a model, not a
verified replacement for this project's missing export. No third-party weights
or dataset have been silently substituted.

## Desktop OpenCV demo

Requirements: CMake 3.16+, a C11/C++17 compiler, and OpenCV 4 development packages
with `core`, `imgproc`, `imgcodecs`, `videoio`, and `highgui`.

On Debian/Ubuntu install `build-essential cmake libopencv-dev`. On macOS install
`cmake opencv` with Homebrew. On Windows configure CMake to find your OpenCV 4
installation using `OpenCV_DIR`.

```sh
cmake -S . -B build -DSEMICOLON_DESKTOP=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Provide local images you are authorized to use:

```text
data/currency/
├── 100/
│   └── reference-front.png
├── 200/
│   └── reference-front.png
└── 500/
    └── reference-front.png
```

At least one denomination directory must contain a usable image. The loader
sorts filenames, skips unreadable and constant images, and loads up to ten valid
templates per denomination. Images and camera frames are resized to 200 × 100
grayscale. Template correlation uses a **0.7 heuristic threshold**; this is not
a calibrated confidence probability.

```sh
./build/currency_demo data/currency 0
# macOS only, explicitly enable optional speech:
./build/currency_demo data/currency 0 --speak
```

Use the camera index appropriate for your system. Press Escape in the preview
window to exit. Fill the frame with a similarly aligned note: the demo does not
locate, rectify, or compensate for rotated/perspective-distorted banknotes.
The API returns errors rather than throwing C++ exceptions through the C entry
point. Speech uses a fixed executable with argument passing, never a shell.

## Evaluation needed

Before making accuracy or usability claims, record per-denomination confusion
matrices, unknown/background rejection, lighting/pose variation, worn/folded-note
performance, latency and peak memory on the device, and feedback usability with
the intended users. None of these measurements can be inferred from a successful
compilation or synthetic regression tests.
