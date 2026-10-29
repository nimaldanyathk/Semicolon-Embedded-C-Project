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

const fs = require("node:fs");
const vm = require("node:vm");
function browserHarness() {
  const elements = new Map();
  const element = id => {
    if (!elements.has(id)) elements.set(id, {textContent: "", dataset: {}, checked: true,
      value: "80", hidden: false, setAttribute() {}, removeAttribute() {}});
    return elements.get(id);
  };
  let now = 0, response = {distance_cm: 60, source: "lidar", camera: false}, fail = false;
  let started = 0, stopped = 0, pending = 0, maximumPending = 0;
  const intervals = [], timers = [], events = {};
  const document = {hidden: false, getElementById: element, addEventListener: (type, handler) => { events[type] = handler; }};
  class Audio {
    state = "running";
    async resume() {}
    createOscillator() { return {frequency: {}, connect() { return this; }, start() { ++started; }, stop() { ++stopped; }, disconnect() {}}; }
    createGain() { return {gain: {}, connect() {}, disconnect() {}}; }
  }
  const context = {document, navigator: {vibrate() {}}, performance: {now: () => now},
    window: {AudioContext: Audio, addEventListener() {}, location: {href: "http://device/"}},
    AbortController, URL,
    setTimeout: (handler, delay) => { timers.push({handler, delay}); return timers.length; },
    clearTimeout() {}, setInterval: handler => intervals.push(handler),
    fetch: async () => {
      maximumPending = Math.max(maximumPending, ++pending);
      await Promise.resolve();
      --pending;
      if (fail) throw new Error("Disconnected");
      return {ok: true, json: async () => response};
    }};
  vm.runInNewContext(fs.readFileSync(require.resolve("../firmware/navigation/web/app.js"), "utf8"), context);
  return {element, document, events, intervals, timers,
    advance: value => { now += value; }, response: value => { response = value; }, fail: () => { fail = true; },
    audio: () => ({started, stopped}), maximumPending: () => maximumPending};
}
const flush = () => new Promise(resolve => setImmediate(resolve));

test("audio requires a gesture and stops on stale data, checkbox changes, and hiding", async () => {
  const browser = browserHarness();
  await flush();
  assert.equal(browser.element("status").textContent, "Obstacle nearby");
  assert.equal(browser.audio().started, 0);
  await browser.element("arm").onclick();
  assert.equal(browser.audio().started, 1);
  browser.element("beep").checked = false;
  browser.element("beep").onchange();
  assert.equal(browser.audio().stopped, 1);
  browser.element("beep").checked = true;
  browser.element("beep").onchange();
  assert.equal(browser.audio().started, 2);
  browser.advance(2000);
  browser.intervals[0]();
  assert.equal(browser.element("status").dataset.state, "offline");
  assert.equal(browser.element("distance").textContent, "—");
  assert.equal(browser.audio().stopped, 2);
  await browser.timers.find(timer => timer.delay === 200).handler();
  assert.equal(browser.audio().started, 3);
  browser.document.hidden = true;
  browser.events.visibilitychange();
  assert.equal(browser.audio().stopped, 3);
  assert.equal(browser.element("status").dataset.state, "offline");
  browser.document.hidden = false;
  browser.intervals[0]();
  assert.equal(browser.element("distance").textContent, "—");
  assert.equal(browser.maximumPending(), 1);
});

test("connection errors and invalid sensor data clear a previous proximity warning", async () => {
  const browser = browserHarness();
  await flush();
  await browser.element("arm").onclick();
  browser.response({distance_cm: -1, source: "none"});
  await browser.timers.find(timer => timer.delay === 200).handler();
  assert.equal(browser.element("status").dataset.state, "unavailable");
  assert.equal(browser.audio().stopped, 1);
  browser.fail();
  await browser.timers.find(timer => timer.delay === 200).handler();
  assert.equal(browser.element("status").dataset.state, "offline");
  assert.equal(browser.element("distance").textContent, "—");
});
