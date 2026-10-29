"use strict";

function normalizeThreshold(value) {
  const number = Number(value);
  return Number.isFinite(number) && number >= 20 && number <= 300 ? number : 80;
}

function assessReading(data, threshold) {
  if (!data || !Number.isInteger(data.distance_cm) ||
      !["lidar", "ultrasonic", "none"].includes(data.source)) {
    throw new Error("Invalid sensor response");
  }
  const distance = data.distance_cm;
  if (data.source === "none" || distance <= 0) {
    return {state: "unavailable", distance: null, source: "none"};
  }
  const maximum = data.source === "lidar" ? 800 : 300;
  const minimum = data.source === "lidar" ? 20 : 2;
  if (distance < minimum || distance > maximum) throw new Error("Out-of-range distance");
  return {state: distance < normalizeThreshold(threshold) ? "near" : "measuring", distance, source: data.source};
}

// Export pure logic for host tests; the embedded browser has no module loader.
if (typeof module !== "undefined") module.exports = {normalizeThreshold, assessReading};

if (typeof document !== "undefined") {
  const $ = id => document.getElementById(id);
  let armed = false, audio = null, oscillator = null, gain = null;
  let latest = null, lastResponse = 0, state = "offline", lastSpeech = -Infinity;
  let previewing = false;
  const speakSupported = "speechSynthesis" in window && "SpeechSynthesisUtterance" in window;
  $("support").textContent = [
    "Sound requires a button press.",
    speakSupported ? "Speech is available." : "Speech is unavailable in this browser.",
    "vibrate" in navigator ? "Vibration support depends on your phone and browser permissions." : "Phone vibration is unavailable in this browser."
  ].join(" ");

  function stopTone() {
    if (oscillator) { oscillator.stop(); oscillator.disconnect(); oscillator = null; }
    if (gain) { gain.disconnect(); gain = null; }
  }
  function stopAlerts() {
    stopTone();
    if (speakSupported) window.speechSynthesis.cancel();
    if ("vibrate" in navigator) navigator.vibrate(0);
  }
  function speak(text) {
    if (!armed || !$("voice").checked || !speakSupported) return;
    const now = performance.now();
    if (now - lastSpeech < 5000) return;
    lastSpeech = now;
    window.speechSynthesis.cancel();
    const message = new window.SpeechSynthesisUtterance(text);
    message.lang = "en-IN";
    window.speechSynthesis.speak(message);
  }
  function setState(next, distance = null, source = "none") {
    const changed = next !== state;
    state = next;
    $("distance").textContent = distance === null ? "—" : String(distance);
    const statusText = {near: "Obstacle nearby", measuring: "Receiving distance", unavailable: "Sensor unavailable", offline: "Connection lost — readings unavailable"}[next];
    if ($("status").textContent !== statusText) $("status").textContent = statusText;
    $("status").dataset.state = next;
    $("source").textContent = `Sensor: ${source === "none" ? "unavailable" : source}`;
    if (next !== "near" || !armed || !$("beep").checked) stopTone();
    else if (audio && audio.state === "running" && !oscillator) {
      oscillator = audio.createOscillator();
      gain = audio.createGain();
      oscillator.frequency.value = 800;
      gain.gain.value = 0.08;
      oscillator.connect(gain).connect(audio.destination);
      oscillator.start();
    }
    if (changed && next !== "near") {
      if ("vibrate" in navigator) navigator.vibrate(0);
      if (speakSupported) window.speechSynthesis.cancel();
    }
    if (armed && changed) {
      if (next === "near") {
        speak("Warning. Obstacle nearby.");
        if ($("vibrate").checked && "vibrate" in navigator) navigator.vibrate([200, 100, 200]);
      } else if (next === "unavailable" || next === "offline") speak("Distance readings unavailable.");
    }
  }
  function render() {
    if (document.hidden || !latest || performance.now() - lastResponse >= 2000) {
      setState("offline");
      return;
    }
    const value = assessReading(latest, $("threshold").value);
    setState(value.state, value.distance, value.source);
  }
  $("arm").onclick = async () => {
    armed = !armed;
    $("arm").textContent = armed ? "Disable alerts" : "Enable alerts";
    $("arm").setAttribute("aria-pressed", String(armed));
    if (armed) {
      const Audio = window.AudioContext || window.webkitAudioContext;
      try {
        if (Audio && !audio) audio = new Audio();
        if (audio) await audio.resume();
      } catch (_) { $("support").textContent = "Audio could not start. Check browser permissions."; }
      state = "armed"; // Re-evaluate the current reading after the user gesture.
      render();
    } else stopAlerts();
  };
  for (const id of ["beep", "voice", "vibrate", "threshold"]) $(id).onchange = () => {
    if (id === "threshold") $(id).value = normalizeThreshold($(id).value);
    if (!$("voice").checked && speakSupported) window.speechSynthesis.cancel();
    if (!$("vibrate").checked && "vibrate" in navigator) navigator.vibrate(0);
    render();
  };
  function stopPreview() {
    previewing = false;
    $("preview").removeAttribute("src");
    $("preview").hidden = true;
    $("camera-toggle").textContent = "Start preview";
    $("camera-toggle").setAttribute("aria-pressed", "false");
  }
  $("camera-toggle").onclick = () => {
    if (previewing) return stopPreview();
    previewing = true;
    const url = new URL("/stream", window.location.href);
    url.port = "81";
    $("preview").src = url.href;
    $("preview").hidden = false;
    $("camera-toggle").textContent = "Stop preview";
    $("camera-toggle").setAttribute("aria-pressed", "true");
    $("camera-status").textContent = "";
  };
  $("preview").onerror = () => { stopPreview(); $("camera-status").textContent = "Camera stream unavailable. Close other previews and try again."; };
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) { latest = null; stopAlerts(); stopPreview(); setState("offline"); }
  });
  window.addEventListener("pagehide", () => { stopAlerts(); stopPreview(); });
  // No overlapping requests. Timeout plus independent watchdog clears stale alerts.
  async function poll() {
    if (!document.hidden) {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 1500);
      try {
        const response = await fetch("/sensors", {cache: "no-store", signal: controller.signal});
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const data = await response.json();
        assessReading(data, $("threshold").value);
        if (!document.hidden) {
          latest = data;
          lastResponse = performance.now();
          $("camera-panel").hidden = data.camera !== true;
          if (!data.camera) stopPreview();
          render();
        }
      } catch (_) { latest = null; setState("offline"); }
      finally { clearTimeout(timeout); }
    }
    setTimeout(poll, 200);
  }
  setInterval(render, 250);
  poll();
}
