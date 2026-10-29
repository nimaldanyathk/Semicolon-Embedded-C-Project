#include "semicolon_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "Line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

static void packet(uint8_t frame[9], unsigned distance, unsigned strength) {
    const uint8_t bytes[] = {0x59, 0x59, distance & 255, distance >> 8,
                             strength & 255, strength >> 8, 0, 8, 0};
    memcpy(frame, bytes, 9);
    for (size_t i = 0; i < 8; ++i) frame[8] = (uint8_t)(frame[8] + frame[i]);
}

static sc_lidar_frame decode(unsigned distance, unsigned strength) {
    sc_lidar_parser parser = {0};
    sc_lidar_frame result = {0};
    uint8_t frame[9];
    packet(frame, distance, strength);
    for (size_t i = 0; i < 9; ++i) CHECK(sc_lidar_feed(&parser, frame[i], &result) == (i == 8));
    CHECK(result.distance_cm == distance);
    CHECK(result.strength == strength);
    return result;
}

int main(void) {
    CHECK(decode(123, 400).valid);
    CHECK(decode(20, 100).valid);
    CHECK(decode(800, 65534).valid);
    CHECK(!decode(19, 100).valid);
    CHECK(!decode(801, 100).valid);
    CHECK(!decode(80, 99).valid);
    CHECK(!decode(80, 65535).valid);
    CHECK(!decode(0, 400).valid);
    CHECK(!decode(65535, 400).valid);

    sc_lidar_parser parser = {0};
    sc_lidar_frame result = {0};
    uint8_t frame[9];
    packet(frame, 300, 250);
    const uint8_t noise[] = {0, 0x59, 0, 42, 17};
    for (size_t i = 0; i < sizeof(noise); ++i) CHECK(!sc_lidar_feed(&parser, noise[i], &result));
    for (unsigned repetition = 0; repetition < 3; ++repetition) {
        for (size_t i = 0; i < 9; ++i) CHECK(sc_lidar_feed(&parser, frame[i], &result) == (i == 8));
    }
    frame[8] ^= 1;
    for (size_t i = 0; i < 9; ++i) CHECK(!sc_lidar_feed(&parser, frame[i], &result));

    // Every possible truncated prefix, followed immediately by a complete frame.
    packet(frame, 300, 250);
    for (size_t prefix = 1; prefix < 9; ++prefix) {
        memset(&parser, 0, sizeof(parser));
        for (size_t i = 0; i < prefix; ++i) sc_lidar_feed(&parser, frame[i], &result);
        bool received = false;
        for (size_t i = 0; i < 9; ++i) received = sc_lidar_feed(&parser, frame[i], &result) || received;
        CHECK(received && result.distance_cm == 300);
    }
    // Exercise arbitrary UART noise under sanitizers and verify eventual recovery.
    uint32_t random = 12345;
    for (size_t i = 0; i < 100000; ++i) {
        random = random * 1664525U + 1013904223U;
        sc_lidar_feed(&parser, (uint8_t)(random >> 24), &result);
        CHECK(parser.used < 9);
    }
    for (size_t i = 0; i < 18; ++i) sc_lidar_feed(&parser, frame[i % 9], &result);
    CHECK(result.distance_cm == 300);
    CHECK(!sc_lidar_feed(NULL, 0, &result));
    CHECK(!sc_lidar_feed(&parser, 0, NULL));

    sc_reading lidar = {90, 100, true}, ultra = {50, 1400, true};
    CHECK(sc_select_distance(&lidar, &ultra, 1500, 1500).source == SC_SOURCE_LIDAR);
    CHECK(sc_select_distance(&lidar, &ultra, 1600, 1500).source == SC_SOURCE_ULTRASONIC);
    CHECK(sc_select_distance(&lidar, &ultra, 2900, 1500).source == SC_SOURCE_NONE);
    lidar.valid = false;
    CHECK(sc_select_distance(&lidar, &ultra, 1500, 1500).distance_cm == 50);
    ultra.valid = false;
    CHECK(sc_select_distance(&lidar, &ultra, 1500, 1500).distance_cm == -1);
    sc_reading rollover = {42, UINT32_MAX - 99, true};
    CHECK(sc_reading_fresh(&rollover, 100, 1500));
    CHECK(!sc_reading_fresh(&rollover, 1400, 1500));
    CHECK(!sc_reading_fresh(NULL, 100, 1500));
    CHECK(strcmp(sc_source_name(SC_SOURCE_NONE), "none") == 0);
    puts("Core regression tests passed.");
    return 0;
}
