#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <RadioLib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "protocol.h"

bool gatewayLedBlinking = false;
uint32_t gatewayLedOffAt = 0;

const uint32_t GATEWAY_LED_BLINK_MS = 100;

Module radioModule(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
SX1262 radio(&radioModule);
WebServer server(80);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

struct NodeStatus {
    uint8_t nodeId;
    bool online;
    bool seen;
    uint16_t lastSequence;
    uint32_t lastFrame;
    uint32_t lastSeen;
    uint32_t packetsReceived;
    uint32_t packetsMissed;
    SensorPayload sensor;
    int16_t rssi;
    float snr;
};

NodeStatus nodes[NODE_COUNT]{};
uint32_t frameNumber = 0;
uint32_t lastSyncAt = 0;
uint32_t syncMessagesSent = 0;
uint32_t syncMessagesFailed = 0;
uint32_t lastDisplayUpdate = 0;
uint32_t buzzerOffAt = 0;
bool displayAvailable = false;


void blinkGatewayLed() {
    digitalWrite(GATEWAY_LED, HIGH);
    gatewayLedBlinking = true;
    gatewayLedOffAt = millis() + GATEWAY_LED_BLINK_MS;
}

void updateGatewayLed() {
    if (gatewayLedBlinking && static_cast<int32_t>(millis() - gatewayLedOffAt) >= 0) {
        
        digitalWrite(GATEWAY_LED, LOW);
        gatewayLedBlinking = false;
    }
}

void triggerBuzzer() {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOffAt = millis() + BUZZER_ON_TIME_MS;
}

void updateBuzzer() {
    if (buzzerOffAt != 0 && static_cast<int32_t>(millis() - buzzerOffAt) >= 0) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerOffAt = 0;
    }
}

void initializeDisplay() {
    Wire.begin(OLED_SDA, OLED_SCL);
    displayAvailable = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    if (!displayAvailable) {
        Serial.println("OLED initialization failed; continuing without display.");
        return;
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Mine Gateway");
    display.println("Starting...");
    display.display();
}

void printNodeStatus(uint8_t index) {
    const NodeStatus &node = nodes[index];
    display.println();
    display.print("N");
    display.print(node.nodeId);
    display.print(node.online ? ": ON " : ": OFF ");
    display.print("P:");
    display.print(node.packetsReceived);
    display.print(" M:");
    display.println(node.packetsMissed);

    if (node.seen) {
        display.print(" R:");
        display.print(node.rssi);
        display.print(" | S:");
        display.println(node.snr, 1);
    } else {
        display.println(" R:-- S:--");
    }
}

void updateDisplay() {
    if (!displayAvailable || millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL_MS) {
        return;
    }
    lastDisplayUpdate = millis();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("SYNC:");
    display.print(syncMessagesSent);
    display.print(" | F:");
    display.println(frameNumber);
    printNodeStatus(0);
    if (NODE_COUNT > 1) {
        printNodeStatus(1);
    }
    //display.print("WiFi:");
    //display.println(WiFi.status() == WL_CONNECTED ? "ON" : "OFF");
    // display.print("B:");
    // display.print(syncMessagesFailed);
    // display.print(" ");
    // display.println(buzzerOffAt != 0 ? "ALERT" : "OK");
    display.display();
}

// const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
// <!doctype html>
// <html lang="en">
// <head>
// <meta charset="utf-8">
// <meta name="viewport" content="width=device-width,initial-scale=1">
// <title>Mine Monitoring Gateway</title>
// <style>
// :root{color-scheme:light;--ink:#17252b;--muted:#63747a;--line:#dce5e7;--paper:#f4f7f5;--card:#fff;--green:#17845b;--amber:#b97718;--red:#b33b35}
// *{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:16px/1.45 ui-sans-serif,system-ui,sans-serif}
// main{max-width:1100px;margin:auto;padding:28px 18px 42px}header{display:flex;justify-content:space-between;gap:16px;align-items:end;margin-bottom:24px}h1{margin:0;font-size:clamp(1.7rem,4vw,2.6rem);letter-spacing:0}header p{margin:4px 0 0;color:var(--muted)}#updated{color:var(--muted);font-size:.85rem;text-align:right}
// .nodes{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.card{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:20px;box-shadow:0 8px 24px #18343b0b}.card h2{margin:0}.head{display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid var(--line);padding-bottom:14px;margin-bottom:16px}.status{font-weight:700;font-size:.82rem}.online{color:var(--green)}.unstable{color:var(--amber)}.offline{color:var(--red)}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.metric{border-top:2px solid var(--line);padding-top:9px}.label{color:var(--muted);font-size:.82rem}.value{font-size:1.12rem;font-weight:700;margin-top:2px}.meta{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:18px;color:var(--muted);font-size:.86rem}.wide{grid-column:1/-1}.alert{color:var(--red);font-weight:700}.empty{padding:26px;text-align:center;color:var(--muted);border:1px dashed var(--line);border-radius:8px}@media(max-width:680px){header{display:block}#updated{text-align:left;margin-top:12px}.nodes{grid-template-columns:1fr}}
// </style>
// </head>
// <body><main><header><div><h1>Mine Monitoring Gateway</h1><p>Gateway-coordinated LoRa sensor network</p></div><div id="updated">Loading...</div></header><section id="nodes" class="nodes"></section></main>
// <script>
// const esc=value=>String(value).replace(/[&<>"']/g,char=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[char]));
// const temp=n=>n===-32768?'Unavailable':(n/10).toFixed(1)+' °C';
// const pressure=n=>n?((n/100).toFixed(1)+' hPa'):'Unavailable';
// const state=node=>node.online?(node.missed?['UNSTABLE','unstable']:['ONLINE','online']):['OFFLINE','offline'];
// function render(nodes){document.querySelector('#nodes').innerHTML=nodes.map(node=>{const [label,klass]=state(node);const alerts=[];if(node.flame)alerts.push('Flame detected');if(node.vibration)alerts.push('Vibration detected');return `<article class="card"><div class="head"><h2>Node ${esc(node.id)}</h2><span class="status ${klass}">${label}</span></div><div class="grid"><div class="metric"><div class="label">Temperature</div><div class="value">${temp(node.temperature)}</div></div><div class="metric"><div class="label">Pressure</div><div class="value">${pressure(node.pressure)}</div></div><div class="metric"><div class="label">Gas ADC</div><div class="value">${esc(node.gas)}</div></div><div class="metric"><div class="label">Flame</div><div class="value ${node.flame?'alert':''}">${node.flame?'ALERT':'SAFE'}</div></div><div class="metric"><div class="label">Vibration</div><div class="value ${node.vibration?'alert':''}">${node.vibration?'ALERT':'NORMAL'}</div></div><div class="metric"><div class="label">Acceleration</div><div class="value">${esc(node.acceleration)}</div></div></div><div class="meta"><span>Packets: ${esc(node.packets)}</span><span>Missed: ${esc(node.missed)}</span><span>RSSI: ${esc(node.rssi)} dBm</span><span>SNR: ${esc(node.snr)} dB</span><span>Frame: ${esc(node.frame)}</span><span>Last seen: ${esc(node.lastSeen)}</span>${alerts.length?`<span class="wide alert">${esc(alerts.join(' | '))}</span>`:''}</div></article>`}).join('')||'<div class="empty">Waiting for node packets</div>';document.querySelector('#updated').textContent='Updated '+new Date().toLocaleTimeString()}
// async function refresh(){try{const response=await fetch('/api/nodes',{cache:'no-store'});render(await response.json())}catch(error){document.querySelector('#updated').textContent='Gateway unavailable'}}
// refresh();setInterval(refresh,2000);
// </script></body></html>
// )rawliteral";


// graph discontinuity fix
// old graph look restore
// mouse hover effect

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mine Monitoring Gateway</title>

<style>
:root{
  --bg:#f4f7f6;
  --panel:#ffffff;
  --panel2:#f7faf9;
  --line:#dce6e2;
  --text:#17252b;
  --muted:#6b7b80;

  --green:#17845b;
  --yellow:#b97718;
  --red:#c33f3f;
  --blue:#3189c9;
  --cyan:#168f82;

  --shadow:0 10px 28px rgba(30,55,60,.08);
}
*{
  box-sizing:border-box;
}
body{
  margin:0;
  background:
    radial-gradient(circle at 10% 0%,#e7f3ee 0,transparent 30%),
    radial-gradient(circle at 90% 10%,#eaf3f8 0,transparent 28%),
    var(--bg);
  color:var(--text);
  font-family:
    Inter,
    ui-sans-serif,
    system-ui,
    -apple-system,
    BlinkMacSystemFont,
    "Segoe UI",
    sans-serif;
}
main{
  max-width:1250px;
  margin:auto;
  padding:28px 20px 50px;
}
/* ---------- HEADER ---------- */
header{
  display:flex;
  justify-content:space-between;
  align-items:flex-end;
  gap:20px;
  margin-bottom:25px;
}
.brand{
  display:flex;
  align-items:center;
  gap:14px;
}
.logo{
  width:48px;
  height:48px;
  border-radius:14px;
  display:grid;
  place-items:center;
  background:linear-gradient(145deg,#e2f3ec,#d1ebe2);
  border:1px solid #b9dace;
  color:#17845b;
  font-size:24px;
}
h1{
  margin:0;
  font-size:clamp(1.6rem,4vw,2.3rem);
  letter-spacing:-.03em;
}
.subtitle{
  margin:4px 0 0;
  color:var(--muted);
  font-size:.9rem;
}
.gateway-status{
  text-align:right;
  color:var(--muted);
  font-size:.85rem;
}
.gateway-dot{
  display:inline-block;
  width:8px;
  height:8px;
  border-radius:50%;
  background:var(--green);
  box-shadow:0 0 12px var(--green);
  margin-right:6px;
}
/* ---------- OVERVIEW ---------- */
.overview{
  display:grid;
  grid-template-columns:repeat(4,1fr);
  gap:12px;
  margin-bottom:20px;
}
.overview-card{
  background:#d2dfe6;
  border:1px solid var(--line);
  border-radius:16px;
  padding:16px 18px;
  box-shadow:var(--shadow);
}
.overview-label{
  color:var(--muted);
  font-size:.76rem;
  text-transform:uppercase;
  letter-spacing:.08em;
}
.overview-value{
  margin-top:5px;
  font-size:1.55rem;
  font-weight:750;
}
/* ---------- NODE GRID ---------- */
.nodes{
  display:grid;
  grid-template-columns:repeat(2,minmax(0,1fr));
  gap:18px;
}
.node{
  background:rgba(255,255,255,.96);
  border:1px solid var(--line);
  border-radius:18px;
  overflow:hidden;
  box-shadow:var(--shadow);
}
.node-head{
  display:flex;
  justify-content:space-between;
  align-items:center;
  padding:18px 20px;
  border-bottom:1px solid var(--line);
}
.node-title{
  display:flex;
  align-items:center;
  gap:10px;
}
.node-title h2{
  margin:0;
  font-size:1.15rem;
}
.status{
  padding:5px 9px;
  border-radius:999px;
  font-size:.7rem;
  font-weight:800;
  letter-spacing:.06em;
}
.online{
  color:#147a52;
  background:#e5f6ee;
}
.unstable{
  color:#9a650f;
  background:#fff3d9;
}
.offline{
  color:#b53535;
  background:#fde8e8;
}

/* ---------- MAIN METRICS ---------- */

.metrics{
  display:grid;
  grid-template-columns:1fr 1fr;
  gap:12px;
  padding:18px 20px 10px;
}
.metric-card{
  background:var(--panel2);
  border:1px solid var(--line);
  border-radius:13px;
  padding:14px;
}
.metric-label{
  color:var(--muted);
  font-size:.75rem;
  text-transform:uppercase;
  letter-spacing:.07em;
}
.metric-value{
  margin-top:5px;
  font-size:1.35rem;
  font-weight:750;
}
.metric-sub{
  color:var(--muted);
  font-size:.75rem;
  margin-top:2px;
}
/* ---------- GAS GAUGE ---------- */
.gas-section{
  padding:10px 20px 5px;
}
.section-title{
  color:var(--muted);
  font-size:.74rem;
  text-transform:uppercase;
  letter-spacing:.08em;
  margin-bottom:8px;
}
.gas-wrap{
  display:flex;
  align-items:center;
  gap:18px;
  background:var(--panel2);
  border:1px solid var(--line);
  border-radius:13px;
  padding:13px 15px;
}
.gauge{
  position:relative;
  width:100%;
  height:13px;
  border-radius:20px;
  background:#253137;
  overflow:hidden;
}
.gauge-bar{
  height:100%;
  width:0%;
  border-radius:20px;
  transition:width .5s ease;
  background:var(--green);
}
.gas-number{
  min-width:75px;
  text-align:right;
  font-size:1.05rem;
  font-weight:750;
}
.gas-state{
  min-width:58px;
  text-align:center;
  font-size:.7rem;
  font-weight:800;
}
/* ---------- CHART ---------- */
.chart-section{
  padding:14px 20px 5px;
}
.chart{
  height:180px;
  width:100%;
  background:var(--panel2);
  border:1px solid var(--line);
  border-radius:13px;
  overflow:hidden;
}
canvas{
  width:100%;
  height:100%;
  display:block;
}
/* ---------- ALERTS ---------- */
.alerts{
  display:grid;
  grid-template-columns:1fr 1fr;
  gap:10px;
  padding:12px 20px 5px;
}
.alert-box{
  border-radius:12px;
  padding:11px 13px;
  background:#f7faf9;
  border:1px solid var(--line);
}
.alert-box.active{
  border-color:#e3a3a3;
  background:#fff1f1;
}
.alert-row{
  display:flex;
  justify-content:space-between;
  align-items:center;
}
.alert-name{
  font-size:.82rem;
  font-weight:650;
}
.safe{
  color:var(--green);
  font-size:.73rem;
  font-weight:800;
}
.danger{
  color:var(--red);
  font-size:.73rem;
  font-weight:800;
}
/* ---------- META ---------- */
.meta{
  display:grid;
  grid-template-columns:repeat(3,1fr);
  gap:8px;
  padding:15px 20px 20px;
  color:var(--muted);
  font-size:.72rem;
}
.meta span{
  background:#f4f8f6;
  border:1px solid #dce6e2;
  border-radius:8px;
  padding:7px 8px;
}
/* ---------- EMPTY ---------- */
.empty{
  padding:45px;
  text-align:center;
  color:var(--muted);
  border:1px dashed var(--line);
  border-radius:16px;
}
/* ---------- RESPONSIVE ---------- */
@media(max-width:850px){
  .nodes{
    grid-template-columns:1fr;
  }
  .overview{
    grid-template-columns:repeat(2,1fr);
  }
}
@media(max-width:600px){
  main{
    padding:20px 12px 35px;
  }
  header{
    align-items:flex-start;
  }
  .gateway-status{
    display:none;
  }
  .metrics{
    grid-template-columns:1fr 1fr;
  }
  .meta{
    grid-template-columns:1fr 1fr;
  }
}
</style>
</head>

<body>

<main>

<header>
  <div class="brand">
    <div class="logo">⛏</div>
    <div>
      <h1>LoRaForge | Mine Monitoring</h1>
      <p class="subtitle">
        Gateway-coordinated LoRa sensor network
      </p>
    </div>
  </div>

  <div class="gateway-status">
    <div>
      <span class="gateway-dot"></span>
      GATEWAY ONLINE
    </div>
    <div id="updated">Connecting...</div>
  </div>
</header>


<!-- ================= OVERVIEW ================= -->

<section class="overview">
  <div class="overview-card">
    <div class="overview-label">Nodes Online</div>
    <div class="overview-value" id="onlineCount">0 / 2</div>
  </div>

  <div class="overview-card">
    <div class="overview-label">Total Packets</div>
    <div class="overview-value" id="packetCount">0</div>
  </div>

  <div class="overview-card">
    <div class="overview-label">Active Alerts</div>
    <div class="overview-value" id="alertCount">0</div>
  </div>

  <div class="overview-card">
    <div class="overview-label">Last Update</div>
    <div class="overview-value" id="updateAge">--</div>
  </div>
</section>


<!-- ================= NODES ================= -->

<section id="nodes" class="nodes"></section>
</main>


<script>

const history = {};
const MAX_POINTS = 40;
/* ---------- HELPERS ---------- */

const esc = value =>
  String(value ?? '')
    .replace(/[&<>"']/g,char=>({
      '&':'&amp;',
      '<':'&lt;',
      '>':'&gt;',
      '"':'&quot;',
      "'":'&#39;'
    }[char]));

const temp = n =>
  n === -32768
    ? 'Unavailable'
    : (Number(n)/10).toFixed(1) + ' °C';

const pressure = n =>
  n
    ? (Number(n)/100).toFixed(1) + ' hPa'
    : 'Unavailable';

function state(node){
  if(!node.online)
    return ['OFFLINE','offline'];
  if(node.missed)
    return ['UNSTABLE','unstable'];
  return ['ONLINE','online'];
}
/* ---------- ACCELERATION ---------- */
/*
  Supports:

  1. Object:
     acceleration: {
       x: 0.12,
       y: 0.03,
       z: 0.98
     }
  2. Array:
     acceleration: [0.12,0.03,0.98]
  3. Numeric magnitude:
     acceleration: 1.02
  4. Existing string:
     acceleration: "1.02"
*/

function getAcceleration(node){
  let a = node.acceleration;
  if(a && typeof a === 'object'){
    if(Array.isArray(a)){
      return {
        x:Number(a[0]) || 0,
        y:Number(a[1]) || 0,
        z:Number(a[2]) || 0
      };
    }

    return {
      x:Number(a.x) || 0,
      y:Number(a.y) || 0,
      z:Number(a.z) || 0
    };
  }

  let value = Number(a);
  if(!Number.isNaN(value)){
    return {
      x:value,
      y:0,
      z:0
    };
  }
  return {
    x:0,
    y:0,
    z:0
  };
}


/* ---------- HISTORY ---------- */

// function updateHistory(node){
//   const id = node.id;
//   if(!history[id]){
//       history[id] = {
//         x:[],
//         y:[],
//         z:[],
//         temperature:[],
//         gas:[]
//     };
// }

//   const a = getAcceleration(node);

//   history[id].x.push(a.x);
//   history[id].y.push(a.y);
//   history[id].z.push(a.z);

//   const t =
//     node.temperature === -32768
//       ? null
//       : Number(node.temperature)/10;

//   history[id].temperature.push(t);
//   history[id].gas.push(Number(node.gas) || 0);

//   if(history[id].x.length > MAX_POINTS){
//     history[id].x.shift();
//     history[id].y.shift();
//     history[id].z.shift();
//     history[id].temperature.shift();
//     history[id].gas.shift();
//   }
// }
function updateHistory(node) {
    if (!history[node.id]) {
        history[node.id] = {
            x: [],
            y: [],
            z: [],
            temperature: [],
            gas: []
        };
    }

    const h = history[node.id];
    if (node.online) {
        h.x.push(node.acceleration.x);
        h.y.push(node.acceleration.y);
        h.z.push(node.acceleration.z);
        h.temperature.push(node.temperature / 10);
        h.gas.push(Number(node.gas) || 0);
    } else {
        h.x.push(null);
        h.y.push(null);
        h.z.push(null);
        h.temperature.push(null);
        h.gas.push(null);
    }

    // Keep only the latest 40 points
    if (h.x.length > MAX_POINTS) h.x.shift();
    if (h.y.length > MAX_POINTS) h.y.shift();
    if (h.z.length > MAX_POINTS) h.z.shift();
    if (h.temperature.length > MAX_POINTS) h.temperature.shift();
    if (h.gas.length > MAX_POINTS) h.gas.shift();
}

/* ---------- DRAW GRAPH ---------- */

function drawGraph(canvas, data){

  const ctx = canvas.getContext('2d');
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;

  canvas.width = rect.width * dpr;
  canvas.height = rect.height * dpr;

  ctx.scale(dpr,dpr);

  const w = rect.width;
  const h = rect.height;

  ctx.clearRect(0,0,w,h);

  ctx.strokeStyle = '#dce6e2';
  ctx.lineWidth = 1;

  for(let i=1;i<5;i++){
    const y = (h/5)*i;
    ctx.beginPath();
    ctx.moveTo(0,y);
    ctx.lineTo(w,y);
    ctx.stroke();
  }

  for(let i=1;i<6;i++){
    const x = (w/6)*i;
    ctx.beginPath();
    ctx.moveTo(x,0);
    ctx.lineTo(x,h);
    ctx.stroke();
  }

  /*
    Ignore null values when calculating
    the graph's range.
  */

  const all = [
    ...data.x.filter(v => v !== null && v !== undefined),
    ...data.y.filter(v => v !== null && v !== undefined),
    ...data.z.filter(v => v !== null && v !== undefined)
  ];

  if(!all.length)
    return;

  let min = Math.min(...all);
  let max = Math.max(...all);

  /*
    Keep graph centered around zero.
  */

  const range = Math.max(
    Math.abs(min),
    Math.abs(max),
    1
  ) * 1.15;

  min = -range;
  max = range;


  function drawLine(values,lineWidth){

    if(values.length < 2)
      return;

    ctx.lineWidth = lineWidth;

    ctx.beginPath();

    let drawing = false;

    values.forEach((value,i)=>{

      /*
        NULL means no measurement.
        Stop the current line here.
      */

      if(value === null || value === undefined){
        drawing = false;
        return;
      }

      const x =
        values.length === 1
          ? w/2
          : (i/(values.length-1))*w;

      const y =
        h -
        ((value-min)/(max-min))*h;

      /*
        Start a new line after a NULL.
      */

      if(!drawing){
        ctx.moveTo(x,y);
        drawing = true;
      }
      else{
        ctx.lineTo(x,y);
      }
    });

    ctx.stroke();
  }


  /*
    X
  */

  ctx.strokeStyle = '#55b8ff';
  drawLine(data.x,1.8);


  /*
    Y
  */

  ctx.strokeStyle = '#54e0d0';
  drawLine(data.y,1.8);


  /*
    Z
  */

  ctx.strokeStyle = '#f2bd5b';
  drawLine(data.z,1.8);


  /*
    Zero line
  */

  const zeroY =
    h - ((0-min)/(max-min))*h;

  ctx.strokeStyle = '#aebdc0';
  ctx.setLineDash([4,5]);

  ctx.beginPath();
  ctx.moveTo(0,zeroY);
  ctx.lineTo(w,zeroY);
  ctx.stroke();

  ctx.setLineDash([]);


  /*
    Legend
  */

  ctx.font = '11px system-ui';

  ctx.fillStyle = '#55b8ff';
  ctx.fillText('X',12,18);

  ctx.fillStyle = '#54e0d0';
  ctx.fillText('Y',32,18);

  ctx.fillStyle = '#f2bd5b';
  ctx.fillText('Z',52,18);


  /*
    Latest acceleration values
  */

  const latestX =
    [...data.x].reverse()
      .find(v => v !== null && v !== undefined);

  const latestY =
    [...data.y].reverse()
      .find(v => v !== null && v !== undefined);

  const latestZ =
    [...data.z].reverse()
      .find(v => v !== null && v !== undefined);

  ctx.font = '12px system-ui';

  ctx.fillStyle = '#55b8ff';
  ctx.fillText(
    `X: ${latestX !== undefined ? latestX.toFixed(2) : '--'}`,
    12,
    h - 12
  );

  ctx.fillStyle = '#54e0d0';
  ctx.fillText(
    `Y: ${latestY !== undefined ? latestY.toFixed(2) : '--'}`,
    70,
    h - 12
  );

  ctx.fillStyle = '#f2bd5b';
  ctx.fillText(
    `Z: ${latestZ !== undefined ? latestZ.toFixed(2) : '--'}`,
    128,
    h - 12
  );


  /* =====================================================
     HOVER
     ===================================================== */

  if(canvas._hoverX !== undefined){

    const mouseX = canvas._hoverX;

    const index = Math.round(
      (mouseX / w) * (data.x.length - 1)
    );

    if(index >= 0 && index < data.x.length){

      const hoverX =
        (index / (data.x.length - 1)) * w;


      /*
        Vertical hover line
      */

      ctx.strokeStyle = '#7d8b8f';
      ctx.lineWidth = 1;
      ctx.setLineDash([4,4]);

      ctx.beginPath();
      ctx.moveTo(hoverX,0);
      ctx.lineTo(hoverX,h);
      ctx.stroke();

      ctx.setLineDash([]);


      /*
        Values at this position
      */

      const xValue = data.x[index];
      const yValue = data.y[index];
      const zValue = data.z[index];

      const xText =
        xValue !== null && xValue !== undefined
          ? `X: ${xValue.toFixed(2)}`
          : 'X: No data';

      const yText =
        yValue !== null && yValue !== undefined
          ? `Y: ${yValue.toFixed(2)}`
          : 'Y: No data';

      const zText =
        zValue !== null && zValue !== undefined
          ? `Z: ${zValue.toFixed(2)}`
          : 'Z: No data';


      /*
        Tooltip
      */

      ctx.font = '12px system-ui';

      const padding = 8;
      const lineHeight = 17;

      const tooltipWidth = 110;
      const tooltipHeight = 3 * lineHeight + padding * 2;

      let tooltipX = hoverX + 10;

      if(tooltipX + tooltipWidth > w){
        tooltipX = hoverX - tooltipWidth - 10;
      }

      let tooltipY = 25;

      ctx.fillStyle = 'rgba(255,255,255,0.94)';
      ctx.strokeStyle = '#dce6e2';
      ctx.lineWidth = 1;

      ctx.beginPath();
      ctx.roundRect(
        tooltipX,
        tooltipY,
        tooltipWidth,
        tooltipHeight,
        7
      );
      ctx.fill();
      ctx.stroke();


      /*
        Tooltip text
      */

      ctx.fillStyle = '#55b8ff';
      ctx.fillText(
        xText,
        tooltipX + padding,
        tooltipY + padding + 12
      );

      ctx.fillStyle = '#168f82';
      ctx.fillText(
        yText,
        tooltipX + padding,
        tooltipY + padding + 12 + lineHeight
      );

      ctx.fillStyle = '#b97718';
      ctx.fillText(
        zText,
        tooltipX + padding,
        tooltipY + padding + 12 + lineHeight * 2
      );
    }
  }


  /*
    Install mouse events only once
  */

  if(!canvas._hoverEventsInstalled){

    canvas._hoverEventsInstalled = true;

    canvas.addEventListener('mousemove', function(event){

      const rect = canvas.getBoundingClientRect();

      canvas._hoverX =
        event.clientX - rect.left;

      drawGraph(canvas, data);
    });


    canvas.addEventListener('mouseleave', function(){

      canvas._hoverX = undefined;

      drawGraph(canvas, data);
    });
  }
}

function drawSingleGraph(canvas, values, unit){

  const ctx = canvas.getContext('2d');

  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;

  canvas.width = rect.width * dpr;
  canvas.height = rect.height * dpr;

  ctx.scale(dpr,dpr);

  const w = rect.width;
  const h = rect.height;

  ctx.clearRect(0,0,w,h);


  /*
    Ignore null values when calculating range.
  */

  const validValues = values.filter(
    value => value !== null && value !== undefined
  );

  if(!validValues.length)
    return;


  /* Grid */

  ctx.strokeStyle = '#dce6e2';
  ctx.lineWidth = 1;

  for(let i=1;i<5;i++){

    const y = (h/5)*i;

    ctx.beginPath();
    ctx.moveTo(0,y);
    ctx.lineTo(w,y);
    ctx.stroke();
  }

  for(let i=1;i<6;i++){

    const x = (w/6)*i;

    ctx.beginPath();
    ctx.moveTo(x,0);
    ctx.lineTo(x,h);
    ctx.stroke();
  }


  /* Range */

  let min = Math.min(...validValues);
  let max = Math.max(...validValues);

  if(min === max){
    min -= 1;
    max += 1;
  }

  const padding = (max-min) * 0.15;

  min -= padding;
  max += padding;


  /* Line */

  ctx.strokeStyle = '#54e0d0';
  ctx.lineWidth = 2;

  ctx.beginPath();

  let drawing = false;

  values.forEach((value,i)=>{

    /*
      NULL = no measurement.
      Break the line.
    */

    if(value === null || value === undefined){
      drawing = false;
      return;
    }

    const x =
      (i/(values.length-1))*w;

    const y =
      h -
      ((value-min)/(max-min))*h;

    if(!drawing){
      ctx.moveTo(x,y);
      drawing = true;
    }
    else{
      ctx.lineTo(x,y);
    }

  });

  ctx.stroke();


  /* Latest value */

  let latest = null;

  for(let i=values.length-1;i>=0;i--){

    if(values[i] !== null && values[i] !== undefined){

      latest = values[i];
      break;
    }
  }

  if(latest !== null){

    ctx.fillStyle = '#17252b';
    ctx.font = '12px system-ui';

    ctx.fillText(
      latest.toFixed(1) + ' ' + unit,
      12,
      h - 12
    );
  }


  /* =====================================================
     HOVER
     ===================================================== */

  if(canvas._hoverX !== undefined){

    const mouseX = canvas._hoverX;

    const index = Math.round(
      (mouseX / w) * (values.length - 1)
    );

    if(index >= 0 && index < values.length){

      const hoverX =
        (index / (values.length - 1)) * w;


      /*
        Vertical hover line
      */

      ctx.strokeStyle = '#7d8b8f';
      ctx.lineWidth = 1;
      ctx.setLineDash([4,4]);

      ctx.beginPath();
      ctx.moveTo(hoverX,0);
      ctx.lineTo(hoverX,h);
      ctx.stroke();

      ctx.setLineDash([]);


      /*
        Value at hovered position
      */

      const value = values[index];

      let valueText;

      if(value === null || value === undefined){
        valueText = 'No data';
      }
      else{
        valueText =
          value.toFixed(1) + ' ' + unit;
      }


      /*
        Tooltip
      */

      ctx.font = '12px system-ui';

      const padding = 8;
      const tooltipWidth = 100;
      const tooltipHeight = 36;

      let tooltipX = hoverX + 10;

      if(tooltipX + tooltipWidth > w){
        tooltipX =
          hoverX - tooltipWidth - 10;
      }

      const tooltipY = 25;

      ctx.fillStyle = 'rgba(255,255,255,0.94)';
      ctx.strokeStyle = '#dce6e2';
      ctx.lineWidth = 1;

      ctx.beginPath();
      ctx.roundRect(
        tooltipX,
        tooltipY,
        tooltipWidth,
        tooltipHeight,
        7
      );
      ctx.fill();
      ctx.stroke();


      /*
        Tooltip text
      */

      ctx.fillStyle =
        value === null || value === undefined
          ? '#6b7b80'
          : '#17252b';

      ctx.fillText(
        valueText,
        tooltipX + padding,
        tooltipY + 22
      );
    }
  }


  /*
    Install mouse events only once
  */

  if(!canvas._hoverEventsInstalled){

    canvas._hoverEventsInstalled = true;

    canvas.addEventListener('mousemove', function(event){

      const rect =
        canvas.getBoundingClientRect();

      canvas._hoverX =
        event.clientX - rect.left;

      drawSingleGraph(canvas, values, unit);
    });


    canvas.addEventListener('mouseleave', function(){

      canvas._hoverX = undefined;

      drawSingleGraph(canvas, values, unit);
    });
  }
}

/* ---------- GAS ---------- */
function gasInfo(value){
  const n = Number(value) || 0;

  /*
    4095 is used here because ESP32 ADC
    commonly uses a 12-bit range.
  */

  const percent =
    Math.max(0,Math.min(100,(n/4095)*100));

  let state = 'NORMAL';
  let cls = '';

  if(percent >= 75){
    state = 'HIGH';
    cls = 'danger';
  }
  else if(percent >= 50){
    state = 'ELEVATED';
    cls = '';
  }

  return {
    percent,
    state,
    cls
  };
}

/* ---------- RENDER NODE ---------- */
function renderNode(node){

  const [label,klass] = state(node);

  const alerts = [];

  if(node.flame)
    alerts.push('Flame detected');

  if(node.vibration)
    alerts.push('Vibration detected');

  const gas = gasInfo(node.gas);
  const acceleration = getAcceleration(node);

  return `
  <article class="node">
    <div class="node-head">
      <div class="node-title">
        <h2>Sensor Node ${esc(node.id)}</h2>
        <span class="status ${klass}">
          ${label}
        </span>
      </div>
    </div>


    <!-- METRICS -->
    <div class="metrics">
      <div class="metric-card">
        <div class="metric-label">
          Temperature
        </div>
        <div class="metric-value">
          ${temp(node.temperature)}
        </div>
        <div class="metric-sub">
          Environmental temperature
        </div>
      </div>


      <div class="metric-card">
        <div class="metric-label">
          Pressure
        </div>
        <div class="metric-value">
          ${pressure(node.pressure)}
        </div>
        <div class="metric-sub">
          Atmospheric pressure
        </div>
      </div>
    </div>


    <!-- GAS -->
    <div class="gas-section">
      <div class="section-title">
        Gas Sensor • ADC Level
      </div>
      <div class="gas-wrap">
        <div style="flex:1">
          <div class="gauge">
            <div
              class="gauge-bar"
              style="width:${gas.percent}%">
            </div>
          </div>
        </div>
        <div class="gas-number">
          ${esc(node.gas)}
        </div>

        <div class="gas-state ${gas.cls}">
          ${gas.state}
        </div>
      </div>
    </div>


    <!-- ACCELERATION -->
    <div class="chart-section">
      <div class="section-title">
        MPU6050 Acceleration
      </div>

      <div class="chart">
        <canvas
          id="accel-${esc(node.id)}">
        </canvas>
      </div>
    </div>

    <!-- temperature graph -->
    <div class="chart-section">
  <div class="section-title">
    Temperature History
  </div>

  <div class="chart">
    <canvas
      id="temp-${esc(node.id)}">
    </canvas>
  </div>
</div>

<!-- gas graph -->
<div class="chart-section">
  <div class="section-title">
    Gas ADC History
  </div>

  <div class="chart">
    <canvas
      id="gas-${esc(node.id)}">
    </canvas>
  </div>
</div>


    <!-- ALERTS -->
    <div class="alerts">
      <div class="alert-box ${node.flame?'active':''}">
        <div class="alert-row">
          <span class="alert-name">
            Flame Sensor
          </span>

          <span class="${node.flame?'danger':'safe'}">
            ${node.flame?'ALERT':'SAFE'}
          </span>
        </div>
      </div>


      <div class="alert-box ${node.vibration?'active':''}">
        <div class="alert-row">
          <span class="alert-name">
            Vibration
          </span>

          <span class="${node.vibration?'danger':'safe'}">
            ${node.vibration?'DETECTED':'NORMAL'}
          </span>
        </div>
      </div>
    </div>

    <!-- META -->

    <div class="meta">
      <span>
        Packets: ${esc(node.packets)}
      </span>

      <span>
        ⚠ Missed: ${esc(node.missed)}
      </span>

      <span>
        RSSI: ${esc(node.rssi)} dBm
      </span>

      <span>
        SNR: ${esc(node.snr)} dB
      </span>

      <span>
        Frame: ${esc(node.frame)}
      </span>

      <span>
        Last: ${esc(node.lastSeen)}
      </span>
    </div>
  </article>
  `;
}

/* ---------- MAIN RENDER ---------- */
function render(nodes){
  const container =
    document.querySelector('#nodes');
  /*
    Update history BEFORE drawing.
  */
  nodes.forEach(updateHistory);
  /*
    Overview statistics
  */
  const online =
    nodes.filter(n=>n.online).length;

  const packets =
    nodes.reduce(
      (sum,n)=>sum+(Number(n.packets)||0),
      0
    );

  const alerts =
    nodes.reduce(
      (sum,n)=>
        sum+
        (n.flame?1:0)+
        (n.vibration?1:0),
      0
    );

  document.querySelector('#onlineCount')
    .textContent =
      `${online} / ${nodes.length || 2}`;

  document.querySelector('#packetCount')
    .textContent =
      packets;

  document.querySelector('#alertCount')
    .textContent =
      alerts;

  container.innerHTML =
    nodes.map(renderNode).join('') ||
    '<div class="empty">Waiting for node packets...</div>';

  /*
    Draw acceleration graphs
  */
  nodes.forEach(node=>{

  /* Acceleration */
  const accelCanvas =
    document.querySelector(
      `#accel-${CSS.escape(String(node.id))}`
    );

  if(accelCanvas && history[node.id]){
    drawGraph(
      accelCanvas,
      history[node.id]
    );
  }

  /* Temperature */
  const tempCanvas =
    document.querySelector(
      `#temp-${CSS.escape(String(node.id))}`
    );

  if(tempCanvas && history[node.id]){
    drawSingleGraph(
      tempCanvas,
      history[node.id].temperature,
      '°C'
    );
  }

  /* Gas */
  const gasCanvas =
    document.querySelector(
      `#gas-${CSS.escape(String(node.id))}`
    );

  if(gasCanvas && history[node.id]){
    drawSingleGraph(
      gasCanvas,
      history[node.id].gas,
      'ADC'
    );
  }
});

  document.querySelector('#updated')
    .textContent =
      'Updated '+new Date().toLocaleTimeString();

  document.querySelector('#updateAge')
    .textContent =
      new Date().toLocaleTimeString();
}


/* ---------- FETCH ---------- */
async function refresh(){
  try{
    const response =
      await fetch(
        '/api/nodes',
        {cache:'no-store'}
      );

    if(!response.ok)
      throw new Error('HTTP '+response.status);

    const nodes =
      await response.json();
    render(nodes);
  }
  catch(error){
    console.error(error);
    document.querySelector('#updated')
      .textContent =
        'Gateway unavailable';
  }
}

refresh();
/*
  Refresh every 2 seconds.
*/
setInterval(refresh,2000);

/*
  Redraw graphs when browser size changes.
*/
window.addEventListener('resize',()=>{
  Object.keys(history).forEach(id=>{
    const canvas =
      document.querySelector(
        `#accel-${CSS.escape(String(id))}`
      );
    if(canvas)
      drawGraph(canvas,history[id]);
  });
});

</script>
</body>
</html>
)rawliteral";


String jsonEscape(const String &value) {
    String escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return escaped;
}

String formatLastSeen(const NodeStatus &node) {
    if (!node.seen) {
        return "never";
    }
    return String((millis() - node.lastSeen) / 1000UL) + "s ago";
}

// String accelerationText(const SensorPayload &sensor) {
//     return String(sensor.accelX / 100.0f, 2) + ", " +
//            String(sensor.accelY / 100.0f, 2) + ", " +
//            String(sensor.accelZ / 100.0f, 2);
// }

void sendJsonResponse() {
    String json = "[";
    for (uint8_t index = 0; index < NODE_COUNT; ++index) {
        const NodeStatus &node = nodes[index];
        if (index > 0) {
            json += ",";
        }
        json += "{\"id\":" + String(node.nodeId);
        json += ",\"online\":" + String(node.online ? "true" : "false");
        json += ",\"temperature\":" + String(node.sensor.temperatureX10);
        json += ",\"pressure\":" + String(node.sensor.pressurePa);
        json += ",\"gas\":" + String(node.sensor.gasAdc);
        json += ",\"flame\":" + String(node.sensor.flame ? "true" : "false");
        json += ",\"vibration\":" + String(node.sensor.vibration ? "true" : "false");
        //json += ",\"acceleration\":\"" + jsonEscape(accelerationText(node.sensor)) + "\"";

        json += ",\"acceleration\":{";
        json += "\"x\":" + String(node.sensor.accelX / 100.0f, 2);
        json += ",\"y\":" + String(node.sensor.accelY / 100.0f, 2);
        json += ",\"z\":" + String(node.sensor.accelZ / 100.0f, 2);
        json += "}";
        
        json += ",\"packets\":" + String(node.packetsReceived);
        json += ",\"missed\":" + String(node.packetsMissed);
        json += ",\"rssi\":" + String(node.rssi);
        json += ",\"snr\":" + String(node.snr, 1);
        json += ",\"frame\":" + String(node.lastFrame);
        json += ",\"lastSeen\":\"" + jsonEscape(formatLastSeen(node)) + "\"}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void initializeNodeStatus() {
    for (uint8_t index = 0; index < NODE_COUNT; ++index) {
        nodes[index].nodeId = index + 1;
        nodes[index].sensor.temperatureX10 = INT16_MIN;
    }
}

void initializeRadio() {
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    pinMode(LORA_ANT_SW, OUTPUT);
    digitalWrite(LORA_ANT_SW, HIGH);

    int16_t state = radio.begin(
        LORA_FREQUENCY_MHZ,
        LORA_BANDWIDTH_KHZ,
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_OUTPUT_POWER_DBM,
        LORA_PREAMBLE_LENGTH
    );
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("Radio initialization failed: %d\n", state);
        while (true) {
            digitalWrite(GATEWAY_LED, !digitalRead(GATEWAY_LED));
            delay(250);
        }
    }
    radio.setCRC(true);
    radio.startReceive();
}

void sendSync() {
    SyncPacket packet{};
    packet.header.version = PROTOCOL_VERSION;
    packet.header.type = PKT_SYNC;
    packet.header.nodeId = 0;
    packet.header.gatewayId = GATEWAY_ID;
    packet.header.sequence = 0;
    packet.header.frameNumber = frameNumber;
    packet.header.payloadLength = sizeof(packet.payload);
    packet.payload.slotDurationMs = SYNC_SLOT_DURATION_MS;
    packet.payload.guardTimeMs = SYNC_GUARD_TIME_MS;
    packet.payload.nodeCount = NODE_COUNT;

    int16_t state = radio.transmit(
        reinterpret_cast<const uint8_t *>(&packet),
        sizeof(packet)
    );
    if (state == RADIOLIB_ERR_NONE) {
        ++syncMessagesSent;
        blinkGatewayLed();
    } else {
        ++syncMessagesFailed;
    }
    Serial.printf("SYNC frame %lu: %s\n",
                  static_cast<unsigned long>(frameNumber),
                  state == RADIOLIB_ERR_NONE ? "sent" : "failed");
    radio.startReceive();
}

void processDataPacket() {
    DataPacket packet{};
    if (radio.getPacketLength() != sizeof(packet)) {
        radio.startReceive();
        return;
    }

    int16_t state = radio.readData(
        reinterpret_cast<uint8_t *>(&packet),
        sizeof(packet)
    );
    int16_t rssi = static_cast<int16_t>(radio.getRSSI());
    float snr = radio.getSNR();
    radio.startReceive();

    if (state != RADIOLIB_ERR_NONE ||
        packet.header.version != PROTOCOL_VERSION ||
        packet.header.type != PKT_DATA ||
        packet.header.gatewayId != GATEWAY_ID ||
        packet.header.nodeId < 1 ||
        packet.header.nodeId > NODE_COUNT ||
        packet.header.payloadLength != sizeof(packet.payload)) {
        return;
    }

    NodeStatus &node = nodes[packet.header.nodeId - 1];
    bool newSensorAlert = (packet.payload.flame || packet.payload.vibration) &&
                          (!node.sensor.flame && !node.sensor.vibration);
    if (node.seen && packet.header.sequence > node.lastSequence + 1) {
        node.packetsMissed += packet.header.sequence - node.lastSequence - 1;
    }
    node.lastSequence = packet.header.sequence;
    node.lastFrame = packet.header.frameNumber;
    node.lastSeen = millis();
    node.seen = true;
    node.online = true;
    node.packetsReceived++;
    node.sensor = packet.payload;
    node.rssi = rssi;
    node.snr = snr;
    if (newSensorAlert) {
        triggerBuzzer();
    }

    Serial.printf("DATA node=%u frame=%lu seq=%u RSSI=%d SNR=%.1f\n",
                  node.nodeId,
                  static_cast<unsigned long>(node.lastFrame),
                  node.lastSequence,
                  node.rssi,
                  node.snr);
}

void pollRadio() {
    if (digitalRead(LORA_DIO1) != HIGH) {
        return;
    }
    if (radio.getPacketLength() == sizeof(DataPacket)) {
        processDataPacket();
    } else {
        radio.startReceive();
    }
}

void updateNodeTimeouts() {
    uint32_t now = millis();
    for (uint8_t index = 0; index < NODE_COUNT; ++index) {
        if (nodes[index].seen && now - nodes[index].lastSeen > OFFLINE_TIMEOUT_MS) {
            if (nodes[index].online) {
                //triggerBuzzer();
            }
            nodes[index].online = false;
        }
    }
}

void initializeWebServer() {
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", DASHBOARD_HTML);
    });
    server.on("/api/nodes", HTTP_GET, sendJsonResponse);
    server.onNotFound([]() {
        server.send(404, "text/plain", "Not found");
    });
    server.begin();
}

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts++ < 30) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Dashboard: http://");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Wi-Fi unavailable; LoRa will continue running.");
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    pinMode(GATEWAY_LED, OUTPUT);
    pinMode(GATEWAY_BUTTON, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    initializeNodeStatus();
    initializeDisplay();
    initializeRadio();
    connectWiFi();
    initializeWebServer();

    frameNumber = 0;
    sendSync();
    lastSyncAt = millis();
}

void loop() {
    uint32_t now = millis();
    if (now - lastSyncAt >= SUPERFRAME_MS) {
        ++frameNumber;
        sendSync();
        lastSyncAt = now;
    }

    pollRadio();
    updateNodeTimeouts();
    updateBuzzer();
    updateGatewayLed();
    updateDisplay();
    server.handleClient();
    delay(2);
}