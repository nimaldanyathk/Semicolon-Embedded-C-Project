#ifndef SEMICOLON_CORE_H
#define SEMICOLON_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t bytes[9];
    size_t used;
} sc_lidar_parser;

typedef struct {
    uint16_t distance_cm;
    uint16_t strength;
    bool valid;
} sc_lidar_frame;

typedef struct {
    int distance_cm;
    uint32_t received_ms;
    bool valid;
} sc_reading;

typedef enum { SC_SOURCE_NONE, SC_SOURCE_LIDAR, SC_SOURCE_ULTRASONIC } sc_source;
typedef struct {
    int distance_cm;
    sc_source source;
} sc_distance;

// Zero-initialize parser. Returns true for each checksum-valid frame,
// including invalid measurements, so bad signal invalidates old readings.
bool sc_lidar_feed(sc_lidar_parser *parser, uint8_t byte, sc_lidar_frame *out);
bool sc_reading_fresh(const sc_reading *reading, uint32_t now_ms, uint32_t timeout_ms);
sc_distance sc_select_distance(const sc_reading *lidar, const sc_reading *ultrasonic,
                               uint32_t now_ms, uint32_t timeout_ms);
const char *sc_source_name(sc_source source);
#ifdef __cplusplus
}
#endif
#endif
