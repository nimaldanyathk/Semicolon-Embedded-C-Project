#include "semicolon_core.h"
#include <string.h>

bool sc_lidar_feed(sc_lidar_parser *parser, uint8_t byte, sc_lidar_frame *out) {
    if (!parser || !out) return false;
    if (parser->used < 2 && byte != 0x59) {
        parser->used = 0;
        return false;
    }
    parser->bytes[parser->used++] = byte;
    if (parser->used < sizeof(parser->bytes)) return false;

    uint8_t checksum = 0;
    for (size_t i = 0; i < 8; ++i) checksum = (uint8_t)(checksum + parser->bytes[i]);
    if (checksum == parser->bytes[8]) {
        out->distance_cm = (uint16_t)(parser->bytes[2] | ((uint16_t)parser->bytes[3] << 8));
        out->strength = (uint16_t)(parser->bytes[4] | ((uint16_t)parser->bytes[5] << 8));
        out->valid = out->distance_cm >= 20 && out->distance_cm <= 800 &&
                     out->strength >= 100 && out->strength != UINT16_MAX;
        parser->used = 0;
        return true;
    }

    // Preserve a possible header inside a corrupt frame (dropped/noisy bytes).
    for (size_t i = 1; i < 8; ++i) {
        if (parser->bytes[i] == 0x59 && parser->bytes[i + 1] == 0x59) {
            parser->used = 9 - i;
            memmove(parser->bytes, parser->bytes + i, parser->used);
            return false;
        }
    }
    parser->used = byte == 0x59 ? 1 : 0;
    if (parser->used) parser->bytes[0] = byte;
    return false;
}

bool sc_reading_fresh(const sc_reading *reading, uint32_t now_ms, uint32_t timeout_ms) {
    // Unsigned subtraction remains correct across millis() rollover.
    return reading && reading->valid && reading->distance_cm > 0 &&
           (uint32_t)(now_ms - reading->received_ms) < timeout_ms;
}

sc_distance sc_select_distance(const sc_reading *lidar, const sc_reading *ultrasonic,
                               uint32_t now_ms, uint32_t timeout_ms) {
    if (sc_reading_fresh(lidar, now_ms, timeout_ms)) {
        return (sc_distance){lidar->distance_cm, SC_SOURCE_LIDAR};
    }
    if (sc_reading_fresh(ultrasonic, now_ms, timeout_ms)) {
        return (sc_distance){ultrasonic->distance_cm, SC_SOURCE_ULTRASONIC};
    }
    return (sc_distance){-1, SC_SOURCE_NONE};
}

const char *sc_source_name(sc_source source) {
    switch (source) {
        case SC_SOURCE_LIDAR: return "lidar";
        case SC_SOURCE_ULTRASONIC: return "ultrasonic";
        default: return "none";
    }
}
