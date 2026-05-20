const state = {
  device: "detached",
  sessionId: null,
  frames: [],
  sequence: 0,
  queued: 0,
  rxOverruns: 0,
  txUnderruns: 0,
  timer: null,
  mode: "idle",
};

const els = {
  protocol: document.querySelector("#protocol"),
  port: document.querySelector("#port"),
  i2cSpeed: document.querySelector("#i2cSpeed"),
  i2cAddress: document.querySelector("#i2cAddress"),
  baudrate: document.querySelector("#baudrate"),
  parity: document.querySelector("#parity"),
  pinA: document.querySelector("#pinA"),
  pinB: document.querySelector("#pinB"),
  pullup: document.querySelector("#pullup"),
  vtarget: document.querySelector("#vtarget"),
  attackType: document.querySelector("#attackType"),
  selectionMode: document.querySelector("#selectionMode"),
  stimulusBytes: document.querySelector("#stimulusBytes"),
  repeats: document.querySelector("#repeats"),
  budget: document.querySelector("#budget"),
  frequency: document.querySelector("#frequency"),
  frequencyLabel: document.querySelector("#frequencyLabel"),
  connectBtn: document.querySelector("#connectBtn"),
  capsBtn: document.querySelector("#capsBtn"),
  armBtn: document.querySelector("#armBtn"),
  captureBtn: document.querySelector("#captureBtn"),
  stopBtn: document.querySelector("#stopBtn"),
  disarmBtn: document.querySelector("#disarmBtn"),
  queueBtn: document.querySelector("#queueBtn"),
  fuzzBtn: document.querySelector("#fuzzBtn"),
  clearBtn: document.querySelector("#clearBtn"),
  demoFrameBtn: document.querySelector("#demoFrameBtn"),
  traceFilter: document.querySelector("#traceFilter"),
  traceBody: document.querySelector("#traceBody"),
  sessionState: document.querySelector("#sessionState"),
  sessionId: document.querySelector("#sessionId"),
  frameCount: document.querySelector("#frameCount"),
  queueState: document.querySelector("#queueState"),
  rxOverruns: document.querySelector("#rxOverruns"),
  txUnderruns: document.querySelector("#txUnderruns"),
  timelineBar: document.querySelector("#timelineBar"),
};

const stateLabels = {
  detached: "Detached",
  connected: "Connected",
  caps: "Capabilities read",
  armed: "Armed",
  capturing: "Capturing",
  fuzzing: "Running fuzz",
  disarmed: "Disarmed",
};

function nowLabel() {
  const totalMs = state.sequence * 137;
  const seconds = Math.floor(totalMs / 1000) % 60;
  const ms = totalMs % 1000;
  return `00:${String(seconds).padStart(2, "0")}.${String(ms).padStart(3, "0")}`;
}

function addFrame(bus, event, data, decoded) {
  state.sequence += 1;
  state.frames.push({
    time: nowLabel(),
    bus,
    event,
    data,
    decoded,
  });
  render();
}

function render() {
  const filter = els.traceFilter.value.trim().toLowerCase();
  const visible = state.frames.filter((frame) => {
    if (!filter) return true;
    return [frame.time, frame.bus, frame.event, frame.data, frame.decoded]
      .join(" ")
      .toLowerCase()
      .includes(filter);
  });

  els.traceBody.replaceChildren(
    ...visible.map((frame) => {
      const row = document.createElement("tr");
      row.innerHTML = `
        <td>${escapeHtml(frame.time)}</td>
        <td>${escapeHtml(frame.bus)}</td>
        <td><span class="event ${escapeHtml(frame.event)}">${escapeHtml(frame.event)}</span></td>
        <td>${escapeHtml(frame.data)}</td>
        <td>${escapeHtml(frame.decoded)}</td>
      `;
      return row;
    }),
  );

  els.sessionState.textContent = stateLabels[state.device];
  els.sessionState.className = `state-pill state-${state.device}`;
  els.sessionId.textContent = state.sessionId
    ? `Session: 0x${state.sessionId.toString(16).toUpperCase().padStart(4, "0")}`
    : "Session: -";
  els.frameCount.textContent = `Frames: ${state.frames.length}`;
  els.queueState.textContent = `Queued: ${state.queued}`;
  els.rxOverruns.textContent = state.rxOverruns;
  els.txUnderruns.textContent = state.txUnderruns;
  els.frequencyLabel.textContent = `${els.frequency.value} Hz`;
  els.timelineBar.style.width = `${Math.min(100, state.frames.length * 4)}%`;

  els.traceBody.parentElement.parentElement.scrollTop =
    els.traceBody.parentElement.parentElement.scrollHeight;
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function stopTimer() {
  if (state.timer) {
    clearInterval(state.timer);
    state.timer = null;
  }
}

function startTimer(mode) {
  stopTimer();
  state.mode = mode;
  state.timer = setInterval(() => {
    if (mode === "capture") {
      addGeneratedCaptureFrame();
    } else {
      addGeneratedFuzzFrame();
    }
  }, 700);
}

function addGeneratedCaptureFrame() {
  const bus = els.protocol.value;
  const events = bus === "I2C"
    ? ["START", "BYTE", "ACK", "BYTE", "STOP"]
    : ["BYTE", "BYTE", "BYTE", "BREAK", "BYTE"];
  const event = events[state.sequence % events.length];
  const firstByte = (0x40 + state.sequence) & 0xff;
  const secondByte = (0xa0 + state.sequence * 3) & 0xff;
  const data = event === "BYTE" ? `0x${hex(firstByte)} 0x${hex(secondByte)}` : "";
  const decoded = bus === "I2C"
    ? decodeI2CEvent(event, data)
    : decodeUARTEvent(event, data);
  addFrame(bus, event, data, decoded);
}

function addGeneratedFuzzFrame() {
  const bus = els.protocol.value;
  const stimulusId = state.sequence + 1;
  addFrame(
    bus,
    "FUZZ_TX",
    `id=${stimulusId} ${els.stimulusBytes.value}`,
    `${els.attackType.value}, ${els.selectionMode.value}`,
  );

  if (stimulusId % 4 === 0) {
    addFrame(bus, "NACK", "", "Target rejected mutated frame");
  }

  if (stimulusId % 13 === 0) {
    state.rxOverruns += 1;
    addFrame(bus, "OVERFLOW", "", "Backpressure marker");
  }
}

function decodeI2CEvent(event, data) {
  if (event === "START") return "Bus idle -> transaction";
  if (event === "STOP") return "Transaction complete";
  if (event === "ACK") return "Target acknowledged";
  return data.startsWith("0x9") ? `Address ${els.i2cAddress.value}` : "Decoded payload chunk";
}

function decodeUARTEvent(event, data) {
  if (event === "BREAK") return "Line break detected";
  return `${els.baudrate.value} baud, ${els.parity.value.toLowerCase()} parity`;
}

function hex(value) {
  return value.toString(16).toUpperCase().padStart(2, "0");
}

function connect() {
  stopTimer();
  state.device = "connected";
  state.sessionId = 0x42;
  addFrame("USB", "HELLO_ACK", "protocol=1 session=0x0042", "Mock Pico connected");
}

function getCaps() {
  state.device = "caps";
  addFrame("USB", "CAPS", "I2C UART PIO=8 BUF=128KiB", "Capabilities received");
}

function arm() {
  stopTimer();
  state.device = "armed";
  addFrame(
    "USB",
    "ARM_OK",
    `pins=${els.pinA.value}/${els.pinB.value} vtarget=${els.vtarget.value}`,
    "Configuration validated",
  );
}

function startCapture() {
  state.device = "capturing";
  addFrame(els.protocol.value, "START", "START_CAPTURE", "Live capture started");
  startTimer("capture");
}

function stop() {
  stopTimer();
  state.device = "armed";
  addFrame("USB", "STOP_OK", "drained=512", "Buffers drained, back to armed");
}

function disarm() {
  stopTimer();
  state.device = "disarmed";
  state.queued = 0;
  addFrame("USB", "DISARM", "pins=HIGH-Z", "Safe state");
}

function queueStimulus() {
  state.queued = Math.min(32, state.queued + 1);
  if (state.device !== "capturing" && state.device !== "fuzzing") {
    state.device = "armed";
  }
  addFrame("USB", "QUEUE", els.stimulusBytes.value, `${els.attackType.value} stimulus queued`);
}

function startFuzz() {
  if (state.queued === 0) {
    queueStimulus();
  }
  state.device = "fuzzing";
  addFrame(els.protocol.value, "START", "START_FUZZ", "Fuzzer scheduler started");
  startTimer("fuzz");
}

function clearTrace() {
  state.frames = [];
  state.sequence = 0;
  state.rxOverruns = 0;
  state.txUnderruns = 0;
  render();
}

function seedTrace() {
  addFrame("I2C", "START", "", "Bus idle -> transaction");
  addFrame("I2C", "BYTE", "0x90", "Address 0x48 write");
  addFrame("I2C", "ACK", "", "Target acknowledged");
  addFrame("I2C", "BYTE", "0x00 0x7F", "Register payload");
  addFrame("I2C", "STOP", "", "Transaction complete");
}

els.connectBtn.addEventListener("click", connect);
els.capsBtn.addEventListener("click", getCaps);
els.armBtn.addEventListener("click", arm);
els.captureBtn.addEventListener("click", startCapture);
els.stopBtn.addEventListener("click", stop);
els.disarmBtn.addEventListener("click", disarm);
els.queueBtn.addEventListener("click", queueStimulus);
els.fuzzBtn.addEventListener("click", startFuzz);
els.clearBtn.addEventListener("click", clearTrace);
els.demoFrameBtn.addEventListener("click", addGeneratedCaptureFrame);
els.traceFilter.addEventListener("input", render);
els.frequency.addEventListener("input", render);

seedTrace();
render();
