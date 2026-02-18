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
      --glow1:     0 0 18px #ff124f88;
      --glow2:     0 0 18px #ff00a088;
      --glow3:     0 0 18px #fe75fe88;
      --glow4:     0 0 18px #7a04eb88;
    }

    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      background: var(--bg-page);
      color: var(--text-main);
      font-family: 'Courier New', Courier, monospace;
      min-height: 100vh;
    }

    /* ---- HEADER ---- */
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
      z-index: 10;
    }

    header h1 {
      font-size: 1.25rem;
      letter-spacing: 0.15em;
      text-transform: uppercase;
      color: var(--c1);
      text-shadow: var(--glow1);
      flex: 1;
    }

    #ws-dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: var(--c1);
      box-shadow: var(--glow1);
      transition: background 0.4s, box-shadow 0.4s;
      flex-shrink: 0;
    }
    #ws-dot.connected    { background: #39ff14; box-shadow: 0 0 12px #39ff1488; }
    #ws-dot.disconnected { background: var(--c1); box-shadow: var(--glow1); }

    #ip-label {
      font-size: 0.72rem;
      color: var(--text-dim);
      letter-spacing: 0.05em;
    }

    /* ---- GRID ---- */
    #grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 1.2rem;
      padding: 1.2rem;
    }

    /* ---- CARD ---- */
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
      margin-bottom: 0.6rem;
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

    /* ---- GAUGE ---- */
    .gauge-wrap {
      display: flex;
      justify-content: center;
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
      transition: stroke-dasharray 0.5s cubic-bezier(0.4, 0, 0.2, 1);
    }

    .gauge-value {
      font-size: 1.55rem;
      font-weight: 700;
      font-family: 'Courier New', Courier, monospace;
      fill: var(--text-main);
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

    /* ---- CHART WRAP (expand/collapse via max-height) ---- */
    .chart-wrap {
      max-height: 0;
      overflow: hidden;
      transition: max-height 0.38s cubic-bezier(0.4, 0, 0.2, 1),
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

    /* ---- SCANLINE EFFECT (decorativo) ---- */
    body::after {
      content: '';
      position: fixed;
      inset: 0;
      background: repeating-linear-gradient(
        0deg,
        transparent,
        transparent 2px,
        #00000018 2px,
        #00000018 4px
      );
      pointer-events: none;
      z-index: 999;
    }
  </style>
</head>
<body>

<header>
  <h1>&#9672; Greenhouse</h1>
  <span id="ip-label"></span>
  <span id="ws-dot" class="disconnected"></span>
</header>

<div id="grid">

  <!-- TEMPERATURE -->
  <div class="card" data-sensor="temp" style="--card-color:#ff124f;">
    <div class="card-header">
      <span class="card-title">Temperatura</span>
      <span class="expand-hint">[ expandir ]</span>
    </div>
    <div class="gauge-wrap">
      <svg class="gauge-svg" viewBox="0 0 160 100">
        <path class="gauge-track"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <path class="gauge-arc" id="arc-temp"
          stroke="#ff124f"
          stroke-dasharray="0 195"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <text class="gauge-value" id="val-temp" x="80" y="76" text-anchor="middle">--</text>
        <text class="gauge-unit"  id="unit-temp" x="80" y="90" text-anchor="middle">°C</text>
        <text class="gauge-min" x="14" y="100" text-anchor="middle">0</text>
        <text class="gauge-max" x="146" y="100" text-anchor="middle">50</text>
        <text class="gauge-label-text" x="80" y="100" text-anchor="middle">TEMP</text>
      </svg>
    </div>
    <div class="chart-wrap">
      <div class="chart-inner">
        <canvas id="canvas-temp"></canvas>
      </div>
    </div>
  </div>

  <!-- HUMIDITY -->
  <div class="card" data-sensor="rh" style="--card-color:#ff00a0;">
    <div class="card-header">
      <span class="card-title">Humedad Aire</span>
      <span class="expand-hint">[ expandir ]</span>
    </div>
    <div class="gauge-wrap">
      <svg class="gauge-svg" viewBox="0 0 160 100">
        <path class="gauge-track"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <path class="gauge-arc" id="arc-rh"
          stroke="#ff00a0"
          stroke-dasharray="0 195"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <text class="gauge-value" id="val-rh" x="80" y="76" text-anchor="middle">--</text>
        <text class="gauge-unit"  id="unit-rh" x="80" y="90" text-anchor="middle">%</text>
        <text class="gauge-min" x="14" y="100" text-anchor="middle">0</text>
        <text class="gauge-max" x="146" y="100" text-anchor="middle">100</text>
        <text class="gauge-label-text" x="80" y="100" text-anchor="middle">HR</text>
      </svg>
    </div>
    <div class="chart-wrap">
      <div class="chart-inner">
        <canvas id="canvas-rh"></canvas>
      </div>
    </div>
  </div>

  <!-- SOIL MOISTURE -->
  <div class="card" data-sensor="soil" style="--card-color:#fe75fe;">
    <div class="card-header">
      <span class="card-title">Humedad Suelo</span>
      <span class="expand-hint">[ expandir ]</span>
    </div>
    <div class="gauge-wrap">
      <svg class="gauge-svg" viewBox="0 0 160 100">
        <path class="gauge-track"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <path class="gauge-arc" id="arc-soil"
          stroke="#fe75fe"
          stroke-dasharray="0 195"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <text class="gauge-value" id="val-soil" x="80" y="76" text-anchor="middle">--</text>
        <text class="gauge-unit"  id="unit-soil" x="80" y="90" text-anchor="middle">%</text>
        <text class="gauge-min" x="14" y="100" text-anchor="middle">0</text>
        <text class="gauge-max" x="146" y="100" text-anchor="middle">100</text>
        <text class="gauge-label-text" x="80" y="100" text-anchor="middle">SUELO</text>
      </svg>
    </div>
    <div class="chart-wrap">
      <div class="chart-inner">
        <canvas id="canvas-soil"></canvas>
      </div>
    </div>
  </div>

  <!-- AIR QUALITY -->
  <div class="card" data-sensor="mq" style="--card-color:#7a04eb;">
    <div class="card-header">
      <span class="card-title">Calidad Aire</span>
      <span class="expand-hint">[ expandir ]</span>
    </div>
    <div class="gauge-wrap">
      <svg class="gauge-svg" viewBox="0 0 160 100">
        <path class="gauge-track"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <path class="gauge-arc" id="arc-mq"
          stroke="#7a04eb"
          stroke-dasharray="0 195"
          d="M 18 88 A 62 62 0 0 1 142 88"/>
        <text class="gauge-value" id="val-mq" x="80" y="76" text-anchor="middle">--</text>
        <text class="gauge-unit"  id="unit-mq" x="80" y="90" text-anchor="middle">ADC</text>
        <text class="gauge-min" x="14" y="100" text-anchor="middle">0</text>
        <text class="gauge-max" x="146" y="100" text-anchor="middle">4095</text>
        <text class="gauge-label-text" x="80" y="100" text-anchor="middle">MQ-135</text>
      </svg>
    </div>
    <div class="chart-wrap">
      <div class="chart-inner">
        <canvas id="canvas-mq"></canvas>
      </div>
    </div>
  </div>

</div>

<script>
(function () {
  'use strict';

  // ── Config ──────────────────────────────────────────────
  const GAUGE_CONFIG = {
    temp: { min: 0,    max: 50,   color: '#ff124f', unit: '°C',  decimals: 1 },
    rh:   { min: 0,    max: 100,  color: '#ff00a0', unit: '%',   decimals: 1 },
    soil: { min: 0,    max: 100,  color: '#fe75fe', unit: '%',   decimals: 0 },
    mq:   { min: 0,    max: 4095, color: '#7a04eb', unit: 'ADC', decimals: 0 },
  };
  const ARC_LENGTH   = 195;   // px — longitud del arco semicircular viewBox 160x100 r=62
  const MAX_HISTORY  = 90;    // últimas 3 minutos (90 lecturas × 2 s)
  const WS_RETRY_MS  = 3000;

  // ── History ring buffers ─────────────────────────────────
  const history = { temp: [], rh: [], soil: [], mq: [] };

  // ── Chart.js instances (lazy) ───────────────────────────
  const charts = {};

  // ── Update gauge arc + text ─────────────────────────────
  function updateGauge(id, value, valid) {
    const cfg   = GAUGE_CONFIG[id];
    const arcEl = document.getElementById('arc-' + id);
    const valEl = document.getElementById('val-' + id);

    if (!valid || value === null || value === undefined) {
      arcEl.setAttribute('stroke-dasharray', '0 ' + ARC_LENGTH);
      arcEl.style.opacity = '0.25';
      valEl.textContent = '--';
      return;
    }

    arcEl.style.opacity = '1';
    const ratio = Math.max(0, Math.min(1, (value - cfg.min) / (cfg.max - cfg.min)));
    const dash  = (ratio * ARC_LENGTH).toFixed(2);
    arcEl.setAttribute('stroke-dasharray', dash + ' ' + (ARC_LENGTH - dash).toFixed(2));
    valEl.textContent = value.toFixed(cfg.decimals);
  }

  // ── Push data into history and refresh active chart ─────
  function pushHistory(id, value, valid) {
    if (!valid || value === null || value === undefined) return;
    const buf = history[id];
    buf.push({ x: Date.now(), y: value });
    if (buf.length > MAX_HISTORY) buf.shift();
    if (charts[id]) charts[id].update('none');
  }

  // ── Lazy Chart.js init ───────────────────────────────────
  function initChart(id) {
    if (charts[id]) return;
    const cfg    = GAUGE_CONFIG[id];
    const canvas = document.getElementById('canvas-' + id);

    charts[id] = new Chart(canvas, {
      type: 'line',
      data: {
        datasets: [{
          data: history[id],
          borderColor: cfg.color,
          backgroundColor: cfg.color + '22',
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
            time: {
              unit: 'second',
              displayFormats: { second: 'HH:mm:ss' },
            },
            ticks: {
              color: '#7a04ebaa',
              maxTicksLimit: 5,
              font: { family: 'Courier New', size: 10 },
            },
            grid: { color: '#7a04eb22' },
          },
          y: {
            min: cfg.min,
            max: cfg.max,
            ticks: {
              color: '#7a04ebaa',
              font: { family: 'Courier New', size: 10 },
            },
            grid: { color: '#7a04eb22' },
          },
        },
        plugins: {
          legend: { display: false },
          tooltip: {
            backgroundColor: '#120458ee',
            borderColor: cfg.color,
            borderWidth: 1,
            titleColor: cfg.color,
            bodyColor: '#fe75fe',
            titleFont: { family: 'Courier New' },
            bodyFont:  { family: 'Courier New' },
            callbacks: {
              label: ctx => ctx.parsed.y.toFixed(cfg.decimals) + ' ' + cfg.unit,
            },
          },
        },
      },
    });
  }

  // ── Card expand / collapse ───────────────────────────────
  document.querySelectorAll('.card').forEach(function (card) {
    card.addEventListener('click', function () {
      const id          = card.dataset.sensor;
      const wasExpanded = card.classList.contains('expanded');

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

  // ── WebSocket ────────────────────────────────────────────
  const dot = document.getElementById('ws-dot');
  let ws, retryTimer;

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

      updateGauge('temp', valid ? d.temp_c   : null, valid);
      updateGauge('rh',   valid ? d.rh_pct   : null, valid);
      updateGauge('soil', d.soil_pct, true);
      updateGauge('mq',   d.mq_raw,   true);

      pushHistory('temp', valid ? d.temp_c   : null, valid);
      pushHistory('rh',   valid ? d.rh_pct   : null, valid);
      pushHistory('soil', d.soil_pct, true);
      pushHistory('mq',   d.mq_raw,   true);
    };

    ws.onclose = ws.onerror = function () {
      dot.className = 'disconnected';
      retryTimer = setTimeout(connect, WS_RETRY_MS);
    };
  }

  // Set IP label and start connection
  document.getElementById('ip-label').textContent = location.hostname;
  connect();
}());
</script>
</body>
</html>
)rawliteral";
