"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {assessReading, normalizeThreshold} = require("../firmware/navigation/web/app.js");

test("threshold validation never disables proximity alerts with NaN or an empty input", () => {
  for (const value of ["", "abc", "Infinity", -1, 0, 301]) assert.equal(normalizeThreshold(value), 80);
  assert.equal(normalizeThreshold("20"), 20);
  assert.equal(normalizeThreshold("300"), 300);
});
test("unknown and invalid readings cannot appear as a clear path", () => {
  assert.equal(assessReading({distance_cm: -1, source: "none"}, 80).state, "unavailable");
  assert.equal(assessReading({distance_cm: 0, source: "lidar"}, 80).state, "unavailable");
  for (const data of [null, {}, {distance_cm: "30", source: "lidar"}, {distance_cm: 30, source: "bad"},
    {distance_cm: 9999, source: "lidar"}, {distance_cm: 10, source: "lidar"}]) {
    assert.throws(() => assessReading(data, 80));
  }
});
test("LiDAR and ultrasonic alerts respect units and threshold boundaries", () => {
  assert.equal(assessReading({distance_cm: 79, source: "lidar"}, 80).state, "near");
  assert.equal(assessReading({distance_cm: 80, source: "lidar"}, 80).state, "measuring");
  assert.equal(assessReading({distance_cm: 2, source: "ultrasonic"}, 80).state, "near");
  assert.throws(() => assessReading({distance_cm: 301, source: "ultrasonic"}, 80));
});
