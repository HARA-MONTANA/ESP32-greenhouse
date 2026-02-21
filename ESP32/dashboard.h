#pragma once
#include <Arduino.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Greenhouse Dashboard</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.2/dist/chart.umd.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@3.0.0/dist/chartjs-adapter-date-fns.bundle.min.js"></script>
  <style>
    :root {
      --bg-page:   #060115;
      --bg-card:   #120458;
      --c1:        #ff124f;
      --c2:        #ff00a0;
      --c3:        #fe75fe;
      --c4:        #7a04eb;
      --text-main: #fe75fe;
      --text-dim:  #9b59b6;
      --border:    #7a04eb55;
      --col-low:   #39ff14;
      --col-mid:   #ff8c00;
      --col-high:  #ff124f;
    }

    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      background: var(--bg-page);
      color: var(--text-main);
      font-family: 'Courier New', Courier, monospace;
      min-height: 100vh;
    }

    /* ── HEADER ── */
    header {
      display: flex;
      align-items: center;
      gap: 0.75rem;
      padding: 1rem 1.4rem;
      border-bottom: 1px solid var(--border);
      background: #0a021888;
      backdrop-filter: blur(6px);
      position: sticky;
      top: 0;
      z-index: 20;
    }

    header h1 {
      font-size: 1.25rem;
      letter-spacing: 0.15em;
      text-transform: uppercase;
      color: var(--c1);
      text-shadow: 0 0 16px #ff124f99;
      flex: 1;
    }

    #ip-label {
      font-size: 0.72rem;
      color: var(--text-dim);
      letter-spacing: 0.05em;
    }

    #ws-dot {
      width: 10px; height: 10px;
      border-radius: 50%;
      background: var(--c1);
      box-shadow: 0 0 12px #ff124f88;
      transition: background 0.4s, box-shadow 0.4s;
      flex-shrink: 0;
    }
    #ws-dot.connected    { background: #39ff14; box-shadow: 0 0 12px #39ff1488; }
    #ws-dot.disconnected { background: var(--c1); box-shadow: 0 0 12px #ff124f88; }

    /* settings gear button */
    #btn-settings {
      background: none;
      border: 1px solid var(--border);
      color: var(--text-dim);
      border-radius: 6px;
      padding: 0.35rem 0.6rem;
      cursor: pointer;
      font-size: 1rem;
      line-height: 1;
      transition: border-color 0.2s, color 0.2s, box-shadow 0.2s;
      flex-shrink: 0;
    }
    #btn-settings:hover,
    #btn-settings.active {
      border-color: var(--c4);
      color: var(--c3);
      box-shadow: 0 0 10px #7a04eb66;
    }

    /* ── SETTINGS PANEL ── */
    #settings-panel {
      max-height: 0;
      overflow: hidden;
      transition: max-height 0.38s cubic-bezier(0.4,0,0.2,1),
                  opacity    0.28s ease;
      opacity: 0;
      background: #0d0330;
      border-bottom: 1px solid var(--border);
      z-index: 15;
      position: relative;
    }
    #settings-panel.open {
      max-height: 700px;
      opacity: 1;
    }

    .settings-inner {
      padding: 1rem 1.4rem 1.2rem;
    }

    .settings-title {
      font-size: 0.68rem;
      letter-spacing: 0.2em;
      text-transform: uppercase;
      color: var(--c4);
      margin-bottom: 0.9rem;
    }

    .settings-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 1rem;
    }

    .sensor-thresholds {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 0.85rem 1rem;
    }

    .sensor-thresholds h3 {
      font-size: 0.65rem;
      letter-spacing: 0.15em;
      text-transform: uppercase;
      margin-bottom: 0.7rem;
    }

    .threshold-row {
      display: flex;
      align-items: center;
      gap: 0.6rem;
      margin-bottom: 0.55rem;
    }

    .threshold-row label {
      font-size: 0.62rem;
      color: var(--text-dim);
      width: 5.5rem;
      flex-shrink: 0;
      white-space: nowrap;
    }

    /* zone dot preview */
    .zone-dot {
      width: 8px; height: 8px;
      border-radius: 50%;
      flex-shrink: 0;
    }
    .zone-dot.low  { background: var(--col-low);  box-shadow: 0 0 6px #39ff1488; }
    .zone-dot.mid  { background: var(--col-mid);  box-shadow: 0 0 6px #ff8c0088; }
    .zone-dot.high { background: var(--col-high); box-shadow: 0 0 6px #ff124f88; }

    /* range slider */
    input[type=range] {
      flex: 1;
      -webkit-appearance: none;
      height: 4px;
      border-radius: 2px;
      background: #2a1060;
      outline: none;
      cursor: pointer;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 14px; height: 14px;
      border-radius: 50%;
      background: var(--c4);
      box-shadow: 0 0 6px #7a04ebaa;
      transition: background 0.2s, box-shadow 0.2s;
    }
    input[type=range]::-webkit-slider-thumb:hover {
      background: var(--c3);
      box-shadow: 0 0 10px #fe75feaa;
    }

    .threshold-val {
      font-size: 0.62rem;
      color: var(--text-main);
      width: 3.2rem;
      text-align: right;
      flex-shrink: 0;
    }

    /* zone color strip (visual reference) */
    .color-strip {
      height: 4px;
      border-radius: 2px;
      margin-top: 0.6rem;
      background: linear-gradient(90deg,
        var(--col-low) 0%,
        var(--col-mid) 50%,
        var(--col-high) 100%);
      opacity: 0.65;
    }

    .settings-footer {
      margin-top: 0.85rem;
      display: flex;
      gap: 0.6rem;
    }

    .btn-reset {
      background: none;
      border: 1px solid #7a04eb55;
      color: var(--text-dim);
      border-radius: 6px;
      padding: 0.3rem 0.8rem;
      font-family: 'Courier New', Courier, monospace;
      font-size: 0.65rem;
      letter-spacing: 0.1em;
      text-transform: uppercase;
      cursor: pointer;
      transition: border-color 0.2s, color 0.2s;
    }
    .btn-reset:hover {
      border-color: var(--c4);
      color: var(--c3);
    }

    /* ── GRID ── */
    #grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 1.2rem;
      padding: 1.2rem;
    }

    /* ── CARD ── */
    .card {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 14px;
      padding: 1.1rem 1.1rem 0.9rem;
      cursor: pointer;
      transition: border-color 0.2s, box-shadow 0.2s;
      position: relative;
      overflow: hidden;
    }

    .card::before {
      content: '';
      position: absolute;
      top: 0; left: 0; right: 0;
      height: 2px;
      background: linear-gradient(90deg, transparent, var(--card-color, var(--c4)), transparent);
      opacity: 0.8;
    }

    .card:hover {
      border-color: var(--card-color, var(--c4));
      box-shadow: 0 0 22px color-mix(in srgb, var(--card-color, var(--c4)) 40%, transparent);
    }

    .card.expanded {
      border-color: var(--card-color, var(--c4));
      box-shadow: 0 0 28px color-mix(in srgb, var(--card-color, var(--c4)) 50%, transparent);
    }

    .card-header {
      display: flex;
      align-items: baseline;
      justify-content: space-between;
      margin-bottom: 0.4rem;
    }

    .card-title {
      font-size: 0.72rem;
      letter-spacing: 0.18em;
      text-transform: uppercase;
      color: var(--text-dim);
    }

    .expand-hint {
      font-size: 0.62rem;
      color: var(--text-dim);
      opacity: 0.5;
      letter-spacing: 0.05em;
    }

    /* zone badge: shows current zone name */
    .zone-badge {
      font-size: 0.6rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      padding: 0.15rem 0.45rem;
      border-radius: 4px;
      border: 1px solid currentColor;
      opacity: 0.85;
      transition: color 0.5s, border-color 0.5s;
      flex-shrink: 0;
    }

    /* zone strip below card title */
    .card-zones {
      display: flex;
      gap: 3px;
      margin-bottom: 0.5rem;
      height: 3px;
    }
    .zone-seg {
      border-radius: 2px;
      height: 3px;
      flex: 1;
      transition: opacity 0.4s;
      opacity: 0.3;
    }
    .zone-seg.active { opacity: 1; }
    .zone-seg.low  { background: var(--col-low); }
    .zone-seg.mid  { background: var(--col-mid); }
    .zone-seg.high { background: var(--col-high); }

    /* ── GAUGE ── */
    .gauge-wrap {
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    .gauge-svg {
      width: 100%;
      max-width: 200px;
      overflow: visible;
    }

    .gauge-track {
      fill: none;
      stroke: #1a0a3a;
      stroke-width: 9;
      stroke-linecap: round;
    }

    .gauge-arc {
      fill: none;
      stroke-width: 9;
      stroke-linecap: round;
      transition: stroke-dasharray 0.5s cubic-bezier(0.4,0,0.2,1),
                  stroke 0.5s ease,
                  filter 0.5s ease;
    }

    .gauge-value {
      font-size: 1.55rem;
      font-weight: 700;
      font-family: 'Courier New', Courier, monospace;
      fill: var(--text-main);
      transition: fill 0.5s ease;
    }

    .gauge-unit {
      font-size: 0.78rem;
      fill: var(--text-dim);
      font-family: 'Courier New', Courier, monospace;
    }

    .gauge-min, .gauge-max {
      font-size: 0.62rem;
      fill: #7a04eb88;
      font-family: 'Courier New', Courier, monospace;
    }

    .gauge-label-text {
      font-size: 0.65rem;
      fill: var(--text-dim);
      font-family: 'Courier New', Courier, monospace;
      letter-spacing: 0.12em;
    }

    /* ── CHART WRAP ── */
    .chart-wrap {
      max-height: 0;
      overflow: hidden;
      transition: max-height 0.38s cubic-bezier(0.4,0,0.2,1),
                  opacity 0.3s ease;
      opacity: 0;
    }

    .card.expanded .chart-wrap {
      max-height: 260px;
      opacity: 1;
    }

    .chart-inner {
      position: relative;
      height: 220px;
      margin-top: 0.8rem;
      border-top: 1px solid var(--border);
      padding-top: 0.6rem;
    }

    /* ── SCANLINE ── */
    body::after {
      content: '';
      position: fixed;
      inset: 0;
      background: repeating-linear-gradient(
        0deg,
        transparent, transparent 2px,
        #00000018 2px, #00000018 4px
      );
      pointer-events: none;
      z-index: 999;
    }
  </style>
</head>
<body>

<!-- HEADER -->
<header>
  <h1>&#9672; Greenhouse</h1>
  <span id="ip-label"></span>
  <span id="ws-dot" class="disconnected"></span>
  <button id="btn-settings" title="Ajustar umbrales de color">&#9881;</button>
</header>

<!-- SETTINGS PANEL -->
<div id="settings-panel">
  <div class="settings-inner">
    <div class="settings-title">&#9881; Umbrales de color de los gauges</div>
    <p style="font-size:0.62rem;color:var(--text-dim);margin-bottom:0.9rem;letter-spacing:0.05em;">
      Defini en qué valores cambia el color del arco: bajo &#9679; medio &#9679; alto.<br>
      Los cambios se guardan automáticamente en el navegador.
    </p>

    <div class="settings-grid" id="settings-grid">
      <!-- generado por JS -->
    </div>

    <div class="settings-footer">
      <button class="btn-reset" id="btn-reset-all">&#8635; Restaurar defaults</button>
    </div>
  </div>
</div>

<!-- SENSOR GRID -->
<div id="grid">

  <!-- TEMPERATURE -->
  <div class="card" data-sensor="temp" style="--card-color:#ff124f;">
    <div class="card-header">
      <span class="card-title">Temperatura</span>
      <span class="zone-badge" id="badge-temp">--</span>
      <span class="expand-hint">[ expandir ]</span>
    </div>
    <div class="card-zones">
      <div class="zone-seg low"  id="zseg-temp-low"></div>
      <div class="zone-seg mid"  id="zseg-temp-mid"></div>
      <div class="zone-seg high" id="zseg-temp-high"></div>
    </div>
    <div class="gauge-wrap">
      <svg class="gauge-svg" viewBox="0 0 160 100">
        <path class="gauge-track" d="M 18 88 A 62 62 0 0 1 142 88"/>
        <path class="gauge-arc" id="arc-temp"
          stroke="#39ff14"
          stroke-dasharray="0 195"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <text class="gauge-value" id="val-temp" x="80" y="76" text-anchor="middle">--</text>
        <text class="gauge-unit"  id="unit-temp" x="80" y="90" text-anchor="middle">°C</text>
        <text class="gauge-min"  x="14"  y="100" text-anchor="middle">0</text>
        <text class="gauge-max"  x="146" y="100" text-anchor="middle">50</text>
        <text class="gauge-label-text" x="80" y="100" text-anchor="middle">TEMP</text>
      </svg>
    </div>
    <div class="chart-wrap">
      <div class="chart-inner"><canvas id="canvas-temp"></canvas></div>
    </div>
  </div>

  <!-- HUMIDITY -->
  <div class="card" data-sensor="rh" style="--card-color:#ff00a0;">
    <div class="card-header">
      <span class="card-title">Humedad Aire</span>
      <span class="zone-badge" id="badge-rh">--</span>
      <span class="expand-hint">[ expandir ]</span>
    </div>
    <div class="card-zones">
      <div class="zone-seg low"  id="zseg-rh-low"></div>
      <div class="zone-seg mid"  id="zseg-rh-mid"></div>
      <div class="zone-seg high" id="zseg-rh-high"></div>
    </div>
    <div class="gauge-wrap">
      <svg class="gauge-svg" viewBox="0 0 160 100">
        <path class="gauge-track" d="M 18 88 A 62 62 0 0 1 142 88"/>
        <path class="gauge-arc" id="arc-rh"
          stroke="#39ff14"
          stroke-dasharray="0 195"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <text class="gauge-value" id="val-rh" x="80" y="76" text-anchor="middle">--</text>
        <text class="gauge-unit"  id="unit-rh" x="80" y="90" text-anchor="middle">%</text>
        <text class="gauge-min"  x="14"  y="100" text-anchor="middle">0</text>
        <text class="gauge-max"  x="146" y="100" text-anchor="middle">100</text>
        <text class="gauge-label-text" x="80" y="100" text-anchor="middle">HR</text>
      </svg>
    </div>
    <div class="chart-wrap">
      <div class="chart-inner"><canvas id="canvas-rh"></canvas></div>
    </div>
  </div>

  <!-- SOIL MOISTURE -->
  <div class="card" data-sensor="soil" style="--card-color:#fe75fe;">
    <div class="card-header">
      <span class="card-title">Humedad Suelo</span>
      <span class="zone-badge" id="badge-soil">--</span>
      <span class="expand-hint">[ expandir ]</span>
    </div>
    <div class="card-zones">
      <div class="zone-seg low"  id="zseg-soil-low"></div>
      <div class="zone-seg mid"  id="zseg-soil-mid"></div>
      <div class="zone-seg high" id="zseg-soil-high"></div>
    </div>
    <div class="gauge-wrap">
      <svg class="gauge-svg" viewBox="0 0 160 100">
        <path class="gauge-track" d="M 18 88 A 62 62 0 0 1 142 88"/>
        <path class="gauge-arc" id="arc-soil"
          stroke="#39ff14"
          stroke-dasharray="0 195"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <text class="gauge-value" id="val-soil" x="80" y="76" text-anchor="middle">--</text>
        <text class="gauge-unit"  id="unit-soil" x="80" y="90" text-anchor="middle">%</text>
        <text class="gauge-min"  x="14"  y="100" text-anchor="middle">0</text>
        <text class="gauge-max"  x="146" y="100" text-anchor="middle">100</text>
        <text class="gauge-label-text" x="80" y="100" text-anchor="middle">SUELO</text>
      </svg>
    </div>
    <div class="chart-wrap">
      <div class="chart-inner"><canvas id="canvas-soil"></canvas></div>
    </div>
  </div>

  <!-- AIR QUALITY -->
  <div class="card" data-sensor="mq" style="--card-color:#7a04eb;">
    <div class="card-header">
      <span class="card-title">Calidad Aire</span>
      <span class="zone-badge" id="badge-mq">--</span>
      <span class="expand-hint">[ expandir ]</span>
    </div>
    <div class="card-zones">
      <div class="zone-seg low"  id="zseg-mq-low"></div>
      <div class="zone-seg mid"  id="zseg-mq-mid"></div>
      <div class="zone-seg high" id="zseg-mq-high"></div>
    </div>
    <div class="gauge-wrap">
      <svg class="gauge-svg" viewBox="0 0 160 100">
        <path class="gauge-track" d="M 18 88 A 62 62 0 0 1 142 88"/>
        <path class="gauge-arc" id="arc-mq"
          stroke="#39ff14"
          stroke-dasharray="0 195"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <text class="gauge-value" id="val-mq" x="80" y="76" text-anchor="middle">--</text>
        <text class="gauge-unit"  id="unit-mq" x="80" y="90" text-anchor="middle">ADC</text>
        <text class="gauge-min"  x="14"  y="100" text-anchor="middle">0</text>
        <text class="gauge-max"  x="146" y="100" text-anchor="middle">4095</text>
        <text class="gauge-label-text" x="80" y="100" text-anchor="middle">MQ-135</text>
      </svg>
    </div>
    <div class="chart-wrap">
      <div class="chart-inner"><canvas id="canvas-mq"></canvas></div>
    </div>
  </div>

</div>

<script>
(function () {
  'use strict';

  // ── Sensor config ─────────────────────────────────────────
  var SENSORS = {
    temp: { min: 0,    max: 50,   unit: '°C',  decimals: 1,
            defT1: 24,  defT2: 30,  label: 'Temperatura' },
    rh:   { min: 0,    max: 100,  unit: '%',   decimals: 1,
            defT1: 40,  defT2: 65,  label: 'Humedad Aire' },
    soil: { min: 0,    max: 100,  unit: '%',   decimals: 0,
            defT1: 25,  defT2: 60,  label: 'Humedad Suelo' },
    mq:   { min: 0,    max: 4095, unit: 'ADC', decimals: 0,
            defT1: 400, defT2: 800, label: 'Calidad Aire' },
  };

  var ZONE_COLORS = { low: '#39ff14', mid: '#ff8c00', high: '#ff124f' };
  var ZONE_LABELS = { low: 'BAJO', mid: 'MEDIO', high: 'ALTO' };
  var ARC_LENGTH  = 195;
  var MAX_HISTORY = 90;
  var WS_RETRY_MS = 3000;
  var LS_KEY      = 'gh_thresholds';

  // ── Thresholds (loaded from localStorage or defaults) ─────
  var thresholds = loadThresholds();

  function defaultThresholds() {
    var t = {};
    Object.keys(SENSORS).forEach(function (id) {
      t[id] = { t1: SENSORS[id].defT1, t2: SENSORS[id].defT2 };
    });
    return t;
  }

  function loadThresholds() {
    try {
      var raw = localStorage.getItem(LS_KEY);
      if (raw) {
        var parsed = JSON.parse(raw);
        // validate keys
        var ok = Object.keys(SENSORS).every(function (id) {
          return parsed[id] &&
                 typeof parsed[id].t1 === 'number' &&
                 typeof parsed[id].t2 === 'number';
        });
        if (ok) return parsed;
      }
    } catch (e) {}
    return defaultThresholds();
  }

  function saveThresholds() {
    try { localStorage.setItem(LS_KEY, JSON.stringify(thresholds)); } catch (e) {}
  }

  // ── Color helpers ─────────────────────────────────────────
  function hexToRgb(hex) {
    var r = parseInt(hex.slice(1,3),16);
    var g = parseInt(hex.slice(3,5),16);
    var b = parseInt(hex.slice(5,7),16);
    return [r,g,b];
  }

  function lerpColor(hex1, hex2, t) {
    var a = hexToRgb(hex1), b = hexToRgb(hex2);
    var r = Math.round(a[0] + (b[0]-a[0])*t);
    var g = Math.round(a[1] + (b[1]-a[1])*t);
    var bv= Math.round(a[2] + (b[2]-a[2])*t);
    return '#' + [r,g,bv].map(function(x){ return x.toString(16).padStart(2,'0'); }).join('');
  }

  // Returns { color: '#rrggbb', zone: 'low'|'mid'|'high' }
  function getZone(id, value) {
    var t  = thresholds[id];
    var t1 = Math.min(t.t1, t.t2);
    var t2 = Math.max(t.t1, t.t2);
    if (t1 === t2) t2 = t1 + 0.001;

    if (value <= t1) {
      return { color: ZONE_COLORS.low, zone: 'low' };
    }
    if (value >= t2) {
      return { color: ZONE_COLORS.high, zone: 'high' };
    }
    // interpolate through mid
    var ratio = (value - t1) / (t2 - t1);
    var col;
    if (ratio < 0.5) {
      col = lerpColor(ZONE_COLORS.low, ZONE_COLORS.mid, ratio * 2);
    } else {
      col = lerpColor(ZONE_COLORS.mid, ZONE_COLORS.high, (ratio - 0.5) * 2);
    }
    return { color: col, zone: 'mid' };
  }

  // ── Gauge update ──────────────────────────────────────────
  function updateGauge(id, value, valid) {
    var cfg   = SENSORS[id];
    var arcEl = document.getElementById('arc-' + id);
    var valEl = document.getElementById('val-' + id);
    var badge = document.getElementById('badge-' + id);

    // reset zone segments
    ['low','mid','high'].forEach(function(z) {
      var el = document.getElementById('zseg-' + id + '-' + z);
      if (el) el.classList.remove('active');
    });

    if (!valid || value === null || value === undefined) {
      arcEl.setAttribute('stroke-dasharray', '0 ' + ARC_LENGTH);
      arcEl.style.stroke  = '#7a04eb44';
      arcEl.style.filter  = 'none';
      valEl.textContent   = '--';
      badge.textContent   = '--';
      badge.style.color   = 'var(--text-dim)';
      return;
    }

    var zoneInfo = getZone(id, value);
    var col      = zoneInfo.color;
    var ratio    = Math.max(0, Math.min(1, (value - cfg.min) / (cfg.max - cfg.min)));
    var dash     = (ratio * ARC_LENGTH).toFixed(2);

    arcEl.setAttribute('stroke-dasharray', dash + ' ' + (ARC_LENGTH - dash).toFixed(2));
    arcEl.style.stroke = col;
    arcEl.style.filter = 'drop-shadow(0 0 6px ' + col + '88)';
    valEl.textContent  = value.toFixed(cfg.decimals);
    valEl.setAttribute('fill', col);

    badge.textContent   = ZONE_LABELS[zoneInfo.zone];
    badge.style.color   = col;

    var activeSegEl = document.getElementById('zseg-' + id + '-' + zoneInfo.zone);
    if (activeSegEl) activeSegEl.classList.add('active');
  }

  // ── History + Chart.js ────────────────────────────────────
  var history = { temp: [], rh: [], soil: [], mq: [] };
  var charts  = {};

  function pushHistory(id, value, valid) {
    if (!valid || value === null || value === undefined) return;
    var buf = history[id];
    buf.push({ x: Date.now(), y: value });
    if (buf.length > MAX_HISTORY) buf.shift();
    if (charts[id]) charts[id].update('none');
  }

  function initChart(id) {
    if (charts[id]) return;
    var cfg    = SENSORS[id];
    var canvas = document.getElementById('canvas-' + id);

    // Use current threshold color for the chart line
    var col = thresholds[id]
      ? ZONE_COLORS.mid
      : '#7a04eb';

    charts[id] = new Chart(canvas, {
      type: 'line',
      data: {
        datasets: [{
          data: history[id],
          borderColor: col,
          backgroundColor: col + '22',
          borderWidth: 2,
          pointRadius: 0,
          fill: true,
          tension: 0.35,
        }]
      },
      options: {
        animation: false,
        responsive: true,
        maintainAspectRatio: false,
        parsing: false,
        interaction: { mode: 'nearest', axis: 'x', intersect: false },
        scales: {
          x: {
            type: 'time',
            time: { unit: 'second', displayFormats: { second: 'HH:mm:ss' } },
            ticks: { color: '#7a04ebaa', maxTicksLimit: 5,
                     font: { family: 'Courier New', size: 10 } },
            grid: { color: '#7a04eb22' },
          },
          y: {
            min: cfg.min, max: cfg.max,
            ticks: { color: '#7a04ebaa', font: { family: 'Courier New', size: 10 } },
            grid: { color: '#7a04eb22' },
          },
        },
        plugins: {
          legend: { display: false },
          tooltip: {
            backgroundColor: '#120458ee',
            borderColor: col, borderWidth: 1,
            titleColor: col, bodyColor: '#fe75fe',
            titleFont: { family: 'Courier New' },
            bodyFont:  { family: 'Courier New' },
            callbacks: {
              label: function(ctx) {
                return ctx.parsed.y.toFixed(cfg.decimals) + ' ' + cfg.unit;
              },
            },
          },
        },
      },
    });
  }

  // ── Card expand/collapse ──────────────────────────────────
  document.querySelectorAll('.card').forEach(function (card) {
    card.addEventListener('click', function () {
      var id          = card.dataset.sensor;
      var wasExpanded = card.classList.contains('expanded');

      document.querySelectorAll('.card.expanded').forEach(function (c) {
        c.classList.remove('expanded');
        c.querySelector('.expand-hint').textContent = '[ expandir ]';
      });

      if (!wasExpanded) {
        card.classList.add('expanded');
        card.querySelector('.expand-hint').textContent = '[ colapsar ]';
        initChart(id);
        if (charts[id]) charts[id].update('none');
      }
    });
  });

  // ── Settings panel build ──────────────────────────────────
  var settingsGrid = document.getElementById('settings-grid');

  Object.keys(SENSORS).forEach(function (id) {
    var cfg = SENSORS[id];
    var t   = thresholds[id];

    var block = document.createElement('div');
    block.className = 'sensor-thresholds';
    block.innerHTML =
      '<h3 style="color:var(--text-main);">' + cfg.label + '</h3>' +

      // threshold 1
      '<div class="threshold-row">' +
        '<span class="zone-dot low"></span>' +
        '<label>Bajo &rarr; Medio</label>' +
        '<input type="range" id="t1-' + id + '"' +
          ' min="' + cfg.min + '" max="' + cfg.max + '"' +
          ' step="' + (cfg.decimals > 0 ? '0.5' : '1') + '"' +
          ' value="' + t.t1 + '">' +
        '<span class="threshold-val" id="t1v-' + id + '">' +
          t.t1 + cfg.unit + '</span>' +
      '</div>' +

      // threshold 2
      '<div class="threshold-row">' +
        '<span class="zone-dot high"></span>' +
        '<label>Medio &rarr; Alto</label>' +
        '<input type="range" id="t2-' + id + '"' +
          ' min="' + cfg.min + '" max="' + cfg.max + '"' +
          ' step="' + (cfg.decimals > 0 ? '0.5' : '1') + '"' +
          ' value="' + t.t2 + '">' +
        '<span class="threshold-val" id="t2v-' + id + '">' +
          t.t2 + cfg.unit + '</span>' +
      '</div>' +

      '<div class="color-strip"></div>';

    settingsGrid.appendChild(block);

    // Slider events
    function onSliderChange() {
      var v1 = parseFloat(document.getElementById('t1-' + id).value);
      var v2 = parseFloat(document.getElementById('t2-' + id).value);
      document.getElementById('t1v-' + id).textContent = v1 + cfg.unit;
      document.getElementById('t2v-' + id).textContent = v2 + cfg.unit;
      thresholds[id].t1 = v1;
      thresholds[id].t2 = v2;
      saveThresholds();
      // Re-render current gauge with new thresholds (use last cached value)
      var val = lastValues[id];
      if (val !== null && val !== undefined) {
        updateGauge(id, val, true);
      }
    }

    document.getElementById('t1-' + id).addEventListener('input', onSliderChange);
    document.getElementById('t2-' + id).addEventListener('input', onSliderChange);
  });

  // ── Reset button ──────────────────────────────────────────
  document.getElementById('btn-reset-all').addEventListener('click', function () {
    thresholds = defaultThresholds();
    saveThresholds();
    // update sliders UI
    Object.keys(SENSORS).forEach(function (id) {
      var cfg = SENSORS[id];
      var t   = thresholds[id];
      document.getElementById('t1-' + id).value  = t.t1;
      document.getElementById('t2-' + id).value  = t.t2;
      document.getElementById('t1v-' + id).textContent = t.t1 + cfg.unit;
      document.getElementById('t2v-' + id).textContent = t.t2 + cfg.unit;
      var val = lastValues[id];
      if (val !== null && val !== undefined) updateGauge(id, val, true);
    });
  });

  // ── Settings gear toggle ──────────────────────────────────
  var btnSettings   = document.getElementById('btn-settings');
  var settingsPanel = document.getElementById('settings-panel');

  btnSettings.addEventListener('click', function () {
    var open = settingsPanel.classList.toggle('open');
    btnSettings.classList.toggle('active', open);
  });

  // ── WebSocket ─────────────────────────────────────────────
  var dot       = document.getElementById('ws-dot');
  var ws, retryTimer;
  var lastValues = { temp: null, rh: null, soil: null, mq: null };

  function connect() {
    ws = new WebSocket('ws://' + location.hostname + '/ws');

    ws.onopen = function () {
      dot.className = 'connected';
      clearTimeout(retryTimer);
    };

    ws.onmessage = function (evt) {
      var d;
      try { d = JSON.parse(evt.data); } catch (e) { return; }
      var valid = d.temp_valid === true;

      var readings = {
        temp: valid ? d.temp_c   : null,
        rh:   valid ? d.rh_pct   : null,
        soil: d.soil_pct,
        mq:   d.mq_raw,
      };
      var valids = { temp: valid, rh: valid, soil: true, mq: true };

      Object.keys(readings).forEach(function (id) {
        lastValues[id] = readings[id];
        updateGauge(id, readings[id], valids[id]);
        pushHistory(id, readings[id], valids[id]);
      });
    };

    ws.onclose = ws.onerror = function () {
      dot.className = 'disconnected';
      retryTimer = setTimeout(connect, WS_RETRY_MS);
    };
  }

  document.getElementById('ip-label').textContent = location.hostname;
  connect();

}());
</script>
</body>
</html>
)rawliteral";
