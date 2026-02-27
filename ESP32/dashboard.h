#pragma once
#include <Arduino.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Greenhouse</title>
<style>
:root{
  --bg:#060115;--bg2:#0d0230;--bg3:#120458;
  --c1:#7a04eb;--c2:#fe75fe;--c3:#ff00a0;--c4:#ff124f;
  --neon:#39ff14;--cyan:#00e5ff;--warn:#ff8c00;
  --text:#e8d5ff;--text2:#9b59b6;--border:#7a04eb55;
  --r:10px;--gap:12px;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:'Courier New',monospace;min-height:100vh}
a{color:inherit;text-decoration:none}
/* Header */
header{
  display:flex;align-items:center;flex-wrap:wrap;gap:8px;
  background:var(--bg2);padding:9px 14px;
  border-bottom:1px solid var(--border);position:sticky;top:0;z-index:100;
}
h1{font-size:1rem;color:var(--c2);white-space:nowrap;letter-spacing:.06em}
nav{display:flex;gap:3px}
.tab-btn{
  background:none;border:none;color:var(--text2);
  padding:5px 12px;border-radius:5px;cursor:pointer;font-size:.78rem;font-family:inherit;
  border-bottom:2px solid transparent;transition:all .2s;
}
.tab-btn.active{color:var(--c2);border-bottom-color:var(--c2);background:rgba(254,117,254,.07)}
.tab-btn:hover:not(.active){background:rgba(255,255,255,.04)}
#tg-link:hover{background:rgba(0,172,238,.32)!important}
.hdr-right{display:flex;align-items:center;gap:10px;margin-left:auto;font-size:.75rem;color:var(--text2)}
#ws-dot{width:8px;height:8px;border-radius:50%;background:var(--c4);flex-shrink:0}
#ws-dot.ok{background:var(--neon)}
/* Main */
main{max-width:1200px;margin:0 auto;padding:var(--gap)}
/* Card */
.card{
  background:var(--bg2);border:1px solid var(--border);
  border-radius:var(--r);padding:12px;
}
.card-title{font-size:.72rem;color:var(--text2);text-transform:uppercase;letter-spacing:.06em;margin-bottom:10px}
/* Sensor grid */
#sensor-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:var(--gap);margin-bottom:var(--gap)}
@media(max-width:680px){#sensor-grid{grid-template-columns:repeat(2,1fr)}}
.s-card{text-align:center;transition:border-color .3s}
.s-card.card--alert{border-color:var(--c4)!important;animation:pulse-alert 1.4s infinite}
@keyframes pulse-alert{0%,100%{box-shadow:0 0 16px #ff124f44}50%{box-shadow:0 0 28px #ff124faa}}
.gauge-wrap{position:relative;width:100%;padding-top:58%;margin-bottom:6px}
.gauge-wrap svg{position:absolute;top:0;left:0;width:100%;height:100%}
.gauge-label{font-size:.76rem;color:var(--text2);display:flex;justify-content:space-between;align-items:center;cursor:pointer;user-select:none}
.spk-arrow{font-size:.65rem;color:var(--text2);opacity:.6;transition:transform .2s}
.spk-arrow.open{transform:rotate(90deg)}
.spk-wrap{overflow:hidden;margin-top:5px}
/* Mid row */
#mid-row{display:grid;grid-template-columns:1fr 1fr;gap:var(--gap);margin-bottom:var(--gap)}
@media(max-width:680px){#mid-row{grid-template-columns:1fr}}
/* Camera */
.cam-ctrl{display:flex;gap:5px;margin-bottom:8px}
.cam-ctrl input{flex:1}
.cam-wrap{position:relative;background:#000;border-radius:7px;overflow:hidden;aspect-ratio:16/9}
#cam-feed{width:100%;height:100%;object-fit:cover;display:none}
#cam-ph{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;color:var(--text2);font-size:.82rem}
/* Events */
#ev-list{list-style:none;display:flex;flex-direction:column;gap:5px}
.ev-row{display:flex;align-items:center;gap:7px;font-size:.76rem;padding:3px 0;border-bottom:1px solid var(--border)}
.ev-badge{font-size:.65rem;font-weight:700;padding:1px 5px;border-radius:3px;white-space:nowrap}
.ev-RIEGO{background:#00e5ff18;color:#00e5ff}
.ev-LUZ_ON{background:#39ff1418;color:#39ff14}
.ev-LUZ_OFF{background:#ffffff0a;color:#888}
.ev-ALERTA_ON{background:#ff124f18;color:#ff124f}
.ev-ALERTA_OFF{background:#39ff1418;color:#39ff14}
.ev-time{color:var(--text2);flex-shrink:0}
.ev-detail{color:var(--text2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:180px}
/* Actions */
#actions-section{display:grid;grid-template-columns:repeat(2,1fr);gap:var(--gap);margin-bottom:var(--gap)}
@media(max-width:580px){#actions-section{grid-template-columns:1fr}}
.badge{display:inline-block;font-size:.66rem;font-weight:700;padding:2px 7px;border-radius:10px;margin-bottom:7px;letter-spacing:.04em}
.b-on{background:#39ff1418;color:#39ff14;border:1px solid #39ff1440}
.b-off{background:#ff124f18;color:#ff124f;border:1px solid #ff124f40}
.b-auto{background:#00e5ff18;color:#00e5ff;border:1px solid #00e5ff40}
.b-manual{background:#fe75fe18;color:#fe75fe;border:1px solid #fe75fe40}
.btn{
  border:none;border-radius:5px;padding:5px 12px;cursor:pointer;font-size:.76rem;
  font-family:inherit;font-weight:700;transition:all .15s;
}
.btn-p{background:var(--c1);color:#fff}
.btn-p:hover{background:#9010ff}
.btn-o{background:transparent;border:1px solid var(--border);color:var(--text2)}
.btn-o:hover{border-color:var(--c2);color:var(--c2)}
.btn-d{background:var(--c4);color:#fff}
.btn.fok{background:var(--neon)!important;color:#000!important}
.btn.ferr{background:var(--c4)!important;color:#fff!important}
.ctrl-row{display:flex;align-items:center;gap:7px;margin-top:6px;flex-wrap:wrap}
.ctrl-lbl{font-size:.72rem;color:var(--text2);min-width:65px}
input[type=range]{
  -webkit-appearance:none;width:100%;height:3px;border-radius:2px;
  background:var(--border);outline:none;cursor:pointer;
}
input[type=range]::-webkit-slider-thumb{
  -webkit-appearance:none;width:13px;height:13px;border-radius:50%;
  background:var(--c2);cursor:pointer;
}
.sval{font-size:.72rem;color:var(--c2);min-width:32px;text-align:right}
input[type=number],input[type=text],input[type=password],select{
  background:var(--bg3);border:1px solid var(--border);
  color:var(--text);padding:5px 9px;border-radius:5px;font-size:.76rem;
  font-family:inherit;width:100%;
}
select option{background:var(--bg2)}
.irow{display:flex;gap:5px;align-items:center;margin-top:6px}
.irow input{flex:1}
.tank{font-size:.76rem;margin-top:4px}
/* Logs */
#log-tb{display:flex;gap:7px;margin-bottom:10px;flex-wrap:wrap;align-items:center}
#log-month,#log-file{width:auto;min-width:120px}
#log-wrap{overflow-x:auto;max-height:55vh;font-size:.72rem}
table{border-collapse:collapse;width:100%;min-width:480px}
th,td{padding:4px 9px;text-align:left;border-bottom:1px solid var(--border)}
th{color:var(--text2);position:sticky;top:0;background:var(--bg2)}
.lt-RIEGO{color:#00e5ff}.lt-LUZ_ON{color:#39ff14}.lt-LUZ_OFF{color:#777}
.lt-ALERTA_ON{color:#ff124f}.lt-ALERTA_OFF{color:#39ff14}.lt-INFO{color:#aaa}
/* Config */
#view-config{display:flex;flex-direction:column;gap:9px}
.csec{border-radius:var(--r);overflow:hidden;border:1px solid var(--border)}
.csec summary{
  display:flex;align-items:center;gap:9px;
  background:var(--bg2);padding:11px 14px;cursor:pointer;
  list-style:none;font-weight:700;font-size:.82rem;
  border-left:3px solid var(--c1);
}
.csec summary::-webkit-details-marker{display:none}
.csec[open] summary{border-bottom:1px solid var(--border)}
.cbody{background:var(--bg2);padding:14px}
.cfield{display:grid;grid-template-columns:150px 1fr auto;gap:7px;align-items:center;margin-bottom:9px}
.cfield label{font-size:.76rem;color:var(--text2)}.cfield-note{font-size:.68rem;color:var(--text2);opacity:.65;font-style:italic;display:block;margin-top:1px}
@media(max-width:580px){.cfield{grid-template-columns:1fr}}
.cnote{font-size:.72rem;color:var(--text2);margin-bottom:9px;line-height:1.5}
.cact{margin-top:11px;display:flex;gap:7px;flex-wrap:wrap}
.ml-tbl{width:100%;border-collapse:collapse;font-size:.76rem;margin:7px 0}
.ml-tbl td{padding:4px;border-bottom:1px solid var(--border)}
.ml-tbl td:first-child{color:var(--text2);width:110px}
.pwd-wrap{position:relative;display:flex}
.pwd-wrap input{flex:1;padding-right:34px}
.eye{position:absolute;right:7px;top:50%;transform:translateY(-50%);background:none;border:none;color:var(--text2);cursor:pointer;font-size:.8rem}
.srow{display:flex;align-items:center;gap:9px;margin-bottom:7px}
.srow label{width:130px;font-size:.76rem;color:var(--text2);flex-shrink:0}
.srow input[type=range]{flex:1}
/* ── Config enhancements */
.csec-meta{margin-left:auto;display:flex;gap:6px;align-items:center}
.csec-tag{font-size:.68rem;font-weight:600;padding:2px 8px;border-radius:10px;
  background:rgba(122,4,235,.15);color:var(--c2);border:1px solid var(--border);letter-spacing:.04em}
.csec-hint{font-size:.72rem;color:var(--text2);font-weight:400}
.cfg-ro{background:var(--bg);border:1px solid var(--border);color:var(--text2);
  padding:5px 9px;border-radius:5px;font-size:.76rem;font-family:inherit;width:100%;cursor:default}
.cfg-ro.unlocked{color:var(--text);background:var(--bg3);cursor:text}
.cfg-unlock{font-size:.7rem;color:var(--text2);background:transparent;
  border:1px solid var(--border);border-radius:5px;padding:4px 10px;cursor:pointer;
  font-family:inherit;white-space:nowrap;transition:all .15s}
.cfg-unlock:hover{color:var(--c2);border-color:var(--c2)}
.cfg-divider{border:none;border-top:1px solid var(--border);margin:13px 0 10px}
.ml-tbl input{width:100%}
/* Garantiza que [hidden] siempre gane sobre cualquier display CSS */
[hidden]{display:none!important}
/* WiFi network list */
.wifi-row{display:flex;align-items:center;justify-content:space-between;padding:7px 9px;border:1px solid var(--border);border-radius:5px;margin-bottom:6px;background:var(--bg1)}
.wifi-ssid{font-size:.8rem;font-weight:600;cursor:pointer;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.wifi-ssid:hover{color:var(--c1)}
.wifi-acts{display:flex;gap:5px;flex-shrink:0;margin-left:8px}
</style>
</head>
<body>

<!-- HEADER -->
<header>
  <h1>&#9670; Greenhouse</h1>
  <nav>
    <button class="tab-btn active" data-tab="dashboard" onclick="showTab('dashboard')">Dashboard</button>
    <button class="tab-btn" data-tab="logs" onclick="showTab('logs')">Logs</button>
    <button class="tab-btn" data-tab="config" onclick="showTab('config')">Configuraci&#243;n</button>
  </nav>
  <div id="bot-name-wrap" style="display:flex;align-items:center;gap:5px">
    <input type="text" id="bot-name-hdr" placeholder="@NombreBot"
      style="background:rgba(0,172,238,.08);border:1px solid #00acee44;color:#00acee;
             padding:4px 8px;border-radius:5px;font-size:.72rem;font-family:inherit;width:130px"
      oninput="onBotNameInput(this.value)"
      onkeydown="if(event.key==='Enter')saveBotNameHdr(this.value)">
    <a id="tg-link" href="#" target="_blank" rel="noopener"
      style="display:none;align-items:center;gap:4px;background:rgba(0,172,238,.18);
             border:1px solid #00acee66;color:#00acee;padding:4px 9px;border-radius:5px;
             font-size:.72rem;white-space:nowrap;text-decoration:none">&#9992; @bot</a>
    <button id="tg-edit-btn" onclick="editBotNameHdr()" title="Editar nombre del bot"
      style="display:none;background:rgba(0,172,238,.08);border:1px solid #00acee44;
             color:#00acee;padding:3px 7px;border-radius:5px;font-size:.72rem;
             cursor:pointer;font-family:inherit">&#9998;</button>
  </div>
  <div class="hdr-right">
    <span id="ip-lbl"></span>
    <span id="clock">--:--:--</span>
    <span id="ws-dot" title="WebSocket"></span>
  </div>
</header>

<main>

<!-- ===== DASHBOARD ===== -->
<div id="view-dashboard">

  <!-- Sensor gauges -->
  <div id="sensor-grid">

    <div class="card s-card" id="card-temp">
      <div class="gauge-wrap">
        <svg viewBox="0 0 120 72" xmlns="http://www.w3.org/2000/svg">
          <path d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#1a0840" stroke-width="9" stroke-linecap="round"/>
          <path id="arc-temp" d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#7a04eb" stroke-width="9" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .6s,stroke .4s"/>
          <text x="60" y="62" text-anchor="middle" fill="#e8d5ff" font-size="19" font-weight="bold" id="txt-temp" font-family="Courier New">--</text>
          <text x="60" y="70" text-anchor="middle" fill="#9b59b6" font-size="8" font-family="Courier New">C</text>
        </svg>
      </div>
      <div class="gauge-label" onclick="toggleSpk('temp',this)"><span>&#127777; Temperatura</span><span class="spk-arrow">&#9656;</span></div>
      <div class="spk-wrap" id="spk-wrap-temp" style="display:none"><svg id="spk-temp" viewBox="0 0 600 280" preserveAspectRatio="none" style="width:100%;height:160px;background:rgba(122,4,235,.06);border-radius:4px;display:block"></svg></div>
    </div>

    <div class="card s-card" id="card-rh">
      <div class="gauge-wrap">
        <svg viewBox="0 0 120 72" xmlns="http://www.w3.org/2000/svg">
          <path d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#1a0840" stroke-width="9" stroke-linecap="round"/>
          <path id="arc-rh" d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#00e5ff" stroke-width="9" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .6s,stroke .4s"/>
          <text x="60" y="62" text-anchor="middle" fill="#e8d5ff" font-size="19" font-weight="bold" id="txt-rh" font-family="Courier New">--</text>
          <text x="60" y="70" text-anchor="middle" fill="#9b59b6" font-size="8" font-family="Courier New">%</text>
        </svg>
      </div>
      <div class="gauge-label" onclick="toggleSpk('rh',this)"><span>&#128167; Humedad</span><span class="spk-arrow">&#9656;</span></div>
      <div class="spk-wrap" id="spk-wrap-rh" style="display:none"><svg id="spk-rh" viewBox="0 0 600 280" preserveAspectRatio="none" style="width:100%;height:160px;background:rgba(0,229,255,.06);border-radius:4px;display:block"></svg></div>
    </div>

    <div class="card s-card" id="card-soil">
      <div class="gauge-wrap">
        <svg viewBox="0 0 120 72" xmlns="http://www.w3.org/2000/svg">
          <path d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#1a0840" stroke-width="9" stroke-linecap="round"/>
          <path id="arc-soil" d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#39ff14" stroke-width="9" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .6s,stroke .4s"/>
          <text x="60" y="62" text-anchor="middle" fill="#e8d5ff" font-size="19" font-weight="bold" id="txt-soil" font-family="Courier New">--</text>
          <text x="60" y="70" text-anchor="middle" fill="#9b59b6" font-size="8" font-family="Courier New">%</text>
        </svg>
      </div>
      <div class="gauge-label" onclick="toggleSpk('soil',this)"><span>&#127807; Suelo</span><span class="spk-arrow">&#9656;</span></div>
      <div class="spk-wrap" id="spk-wrap-soil" style="display:none"><svg id="spk-soil" viewBox="0 0 600 280" preserveAspectRatio="none" style="width:100%;height:160px;background:rgba(57,255,20,.06);border-radius:4px;display:block"></svg></div>
    </div>

    <div class="card s-card" id="card-mq">
      <div class="gauge-wrap">
        <svg viewBox="0 0 120 72" xmlns="http://www.w3.org/2000/svg">
          <path d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#1a0840" stroke-width="9" stroke-linecap="round"/>
          <path id="arc-mq" d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#fe75fe" stroke-width="9" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .6s,stroke .4s"/>
          <text x="60" y="62" text-anchor="middle" fill="#e8d5ff" font-size="19" font-weight="bold" id="txt-mq" font-family="Courier New">--</text>
          <text x="60" y="70" text-anchor="middle" fill="#9b59b6" font-size="8" font-family="Courier New">raw</text>
        </svg>
      </div>
      <div class="gauge-label" onclick="toggleSpk('mq',this)"><span>&#127787; Aire MQ</span><span class="spk-arrow">&#9656;</span></div>
      <div class="spk-wrap" id="spk-wrap-mq" style="display:none"><svg id="spk-mq" viewBox="0 0 600 280" preserveAspectRatio="none" style="width:100%;height:160px;background:rgba(254,117,254,.06);border-radius:4px;display:block"></svg></div>
    </div>

    <div class="card s-card" id="card-fan-rpm">
      <div class="gauge-wrap">
        <svg viewBox="0 0 120 72" xmlns="http://www.w3.org/2000/svg">
          <path d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#1a0840" stroke-width="9" stroke-linecap="round"/>
          <path id="arc-fan-rpm" d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#00e5ff" stroke-width="9" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .6s,stroke .4s"/>
          <text x="60" y="62" text-anchor="middle" fill="#e8d5ff" font-size="19" font-weight="bold" id="txt-fan-rpm" font-family="Courier New">--</text>
          <text x="60" y="70" text-anchor="middle" fill="#9b59b6" font-size="8" font-family="Courier New">RPM</text>
        </svg>
      </div>
      <div class="gauge-label" onclick="toggleSpk('fan-rpm',this)"><span>&#127744; Fan RPM</span><span class="spk-arrow">&#9656;</span></div>
      <div class="spk-wrap" id="spk-wrap-fan-rpm" style="display:none"><svg id="spk-fan-rpm" viewBox="0 0 600 280" preserveAspectRatio="none" style="width:100%;height:160px;background:rgba(0,229,255,.06);border-radius:4px;display:block"></svg></div>
    </div>

    <div class="card s-card" id="card-soil-raw">
      <div class="gauge-wrap">
        <svg viewBox="0 0 120 72" xmlns="http://www.w3.org/2000/svg">
          <path d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#1a0840" stroke-width="9" stroke-linecap="round"/>
          <path id="arc-soil-raw" d="M10,67 A55,55 0 0,1 110,67" fill="none" stroke="#ff8c00" stroke-width="9" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .6s,stroke .4s"/>
          <text x="60" y="62" text-anchor="middle" fill="#e8d5ff" font-size="19" font-weight="bold" id="txt-soil-raw" font-family="Courier New">--</text>
          <text x="60" y="70" text-anchor="middle" fill="#9b59b6" font-size="8" font-family="Courier New">ADC</text>
        </svg>
      </div>
      <div class="gauge-label" onclick="toggleSpk('soil-raw',this)"><span>&#127807; Suelo RAW</span><span class="spk-arrow">&#9656;</span></div>
      <div class="spk-wrap" id="spk-wrap-soil-raw" style="display:none"><svg id="spk-soil-raw" viewBox="0 0 600 280" preserveAspectRatio="none" style="width:100%;height:160px;background:rgba(255,140,0,.06);border-radius:4px;display:block"></svg></div>
    </div>

  </div><!-- /sensor-grid -->

  <!-- Mid row -->
  <div id="mid-row">

    <!-- Camera -->
    <div class="card">
      <div class="card-title">&#128249; Camara</div>
      <div class="cam-ctrl">
        <input type="text" id="cam-url" placeholder="http://192.168.x.x/stream">
        <button class="btn btn-p" onclick="camConnect()">Conectar</button>
        <button class="btn btn-o" onclick="camDisconnect()">&#10005;</button>
      </div>
      <div class="cam-wrap">
        <img id="cam-feed" alt="" onerror="camErr()">
        <div id="cam-ph">Sin senial</div>
      </div>
    </div>

    <!-- Events -->
    <div class="card">
      <div class="card-title" style="display:flex;justify-content:space-between;align-items:center">
        <span>&#128203; Ultima actividad</span>
        <button class="btn btn-o" style="padding:2px 7px;font-size:.7rem" onclick="loadEvents()">&#8635;</button>
      </div>
      <ul id="ev-list"><li style="color:var(--text2);font-size:.76rem">Cargando...</li></ul>
    </div>

  </div><!-- /mid-row -->

  <!-- Action cards -->
  <div id="actions-section">

    <!-- LED -->
    <div class="card">
      <div class="card-title">&#128161; LED Morado</div>
      <span id="led-badge" class="badge b-off">OFF</span>
      <div class="ctrl-row">
        <button class="btn btn-p" onclick="sc({cmd:'led',args:'on'},this)">ON</button>
        <button class="btn btn-o" onclick="sc({cmd:'led',args:'off'},this)">OFF</button>
      </div>
      <div class="ctrl-row">
        <span class="ctrl-lbl">Intensidad</span>
        <input type="range" id="led-sl" min="1" max="100" value="80"
          oninput="document.getElementById('led-sv').textContent=this.value+'%'"
          onchange="sc({cmd:'led',args:this.value},this)">
        <span id="led-sv" class="sval">80%</span>
      </div>
    </div>

    <!-- Fan -->
    <div class="card">
      <div class="card-title">&#127744; Ventiladores</div>
      <span id="fan-badge" class="badge b-auto">AUTO</span>
      <div class="ctrl-row">
        <span class="ctrl-lbl">Velocidad</span>
        <input type="range" id="fan-sl" min="0" max="100" value="0"
          oninput="document.getElementById('fan-sv').textContent=this.value+'%'"
          onchange="sc({cmd:'vent',args:this.value},this)">
        <span id="fan-sv" class="sval">0%</span>
      </div>
      <div class="ctrl-row">
        <button class="btn btn-o" id="fan-auto-btn" onclick="toggleFanAuto(this)">Auto: ON</button>
        <span style="font-size:.72rem;color:var(--cyan);margin-left:6px" id="fan-rpm-lbl">-- RPM</span>
      </div>
      <div style="margin-top:8px">
        <svg id="rpm-chart" viewBox="0 0 600 280" preserveAspectRatio="none"
          style="width:100%;height:280px;background:rgba(0,229,255,.05);border-radius:4px;display:block"></svg>
      </div>
    </div>

    <!-- Riego -->
    <div class="card">
      <div class="card-title">&#128167; Riego</div>
      <span id="irr-badge" class="badge b-auto">AUTO ON</span>
      <div class="ctrl-row">
        <button class="btn btn-p" id="irr-btn" onclick="toggleAutoIrr(this)">Auto-riego</button>
      </div>
      <div class="irow">
        <input type="number" id="irr-ml" min="50" max="1500" value="300" placeholder="mL">
        <button class="btn btn-o" onclick="sc({cmd:'regar',args:document.getElementById('irr-ml').value},this)">Regar</button>
      </div>
      <div class="tank" id="tank">&#11036; Tanque: ---</div>
    </div>

  </div><!-- /actions-section -->

</div><!-- /view-dashboard -->

<!-- ===== LOGS ===== -->
<div id="view-logs" hidden>
  <div class="card">
    <div id="log-tb">
      <select id="log-month" onchange="loadMonth()"><option>-- Mes --</option></select>
      <select id="log-file" onchange="loadLogFile()"><option>-- Archivo --</option></select>
      <a id="log-dl" class="btn btn-o" style="text-decoration:none;padding:5px 12px" hidden>&#11015; Descargar</a>
    </div>
    <div id="log-wrap"><p style="color:var(--text2);font-size:.8rem">Selecciona un mes y archivo.</p></div>
  </div>
</div>

<!-- ===== CONFIG ===== -->
<div id="view-config" hidden>

  <!-- Telegram (cerrado por defecto - configuracion inicial) -->
  <details class="csec">
    <summary style="border-left-color:#00acee">
      &#129302; Telegram
      <span class="csec-meta"><span class="csec-hint">Notificaciones y control remoto</span></span>
    </summary>
    <div class="cbody">
      <p class="cnote">El token no se muestra por seguridad. Ingresa uno nuevo solo si necesitas cambiarlo.</p>
      <div class="cfield">
        <label>Token del bot</label>
        <div class="pwd-wrap">
          <input type="password" id="tg-tok" placeholder="&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;">
          <button class="eye" onclick="togglePwd('tg-tok',this)">&#128065;</button>
        </div>
      </div>
      <div class="cfield">
        <label>Nombre del bot</label>
        <input type="text" id="tg-name" placeholder="@MiGreenhouseBot">
      </div>
      <div id="tg-prev" style="font-size:.75rem;color:#00acee;margin:5px 0 9px;display:none">
        Enlace: <a id="tg-prev-lnk" href="#" target="_blank" style="color:#00acee"></a>
      </div>
      <div class="cact">
        <button class="btn btn-p" onclick="saveTelegram()">&#128190; Guardar Telegram</button>
      </div>
    </div>
  </details>

  <!-- Planta -->
  <details class="csec" open>
    <summary style="border-left-color:#39ff14">
      &#127807; Planta
      <span class="csec-meta"><span id="cfg-stage-tag" class="csec-tag">---</span></span>
    </summary>
    <div class="cbody">
      <div class="cfield">
        <label>Etapa</label>
        <select id="cfg-stage" onchange="sc({cmd:'etapa',args:this.value});updateStageTag(this.value)">
          <option value="pl">Plantula</option>
          <option value="veg">Vegetativo</option>
          <option value="pre">Pre-floracion</option>
          <option value="flo">Floracion</option>
          <option value="fin">Final</option>
        </select>
      </div>
      <div class="cfield">
        <label>Maceta (L)</label>
        <input type="number" id="cfg-pot" min="1" max="50" value="5">
      </div>
      <div class="cfield">
        <label>Intensidad LED (%)</label>
        <div class="srow" style="flex:1;margin:0">
          <input type="range" min="1" max="100" id="cfg-led" oninput="document.getElementById('cfg-led-v').textContent=this.value+'%'">
          <span id="cfg-led-v" class="sval">80%</span>
        </div>
      </div>
      <hr class="cfg-divider">
      <p style="font-size:.74rem;color:var(--text2);margin-bottom:7px">&#128167; mL por litro de maceta, por etapa:</p>
      <table class="ml-tbl">
        <tr><td>Plantula</td><td><input type="number" id="ml-pl" min="5" max="200"></td></tr>
        <tr><td>Vegetativo</td><td><input type="number" id="ml-veg" min="5" max="300"></td></tr>
        <tr><td>Pre-floracion</td><td><input type="number" id="ml-pre" min="5" max="400"></td></tr>
        <tr><td>Floracion</td><td><input type="number" id="ml-flo" min="5" max="500"></td></tr>
        <tr><td>Final</td><td><input type="number" id="ml-fin" min="5" max="300"></td></tr>
      </table>
      <hr class="cfg-divider">
      <p style="font-size:.74rem;color:var(--text2);margin-bottom:7px">&#9728;&#65039; Horas de luz diarias:</p>
      <div class="cfield">
        <label>Plantula</label>
        <div class="srow" style="flex:1;margin:0">
          <input type="range" min="12" max="20" id="cfg-luz-pl" oninput="document.getElementById('cfg-luz-pl-v').textContent=this.value+'h'">
          <span id="cfg-luz-pl-v" class="sval">18h</span>
        </div>
      </div>
      <div class="cfield">
        <label>Vegetativo</label>
        <div class="srow" style="flex:1;margin:0">
          <input type="range" min="12" max="20" id="cfg-luz-veg" oninput="document.getElementById('cfg-luz-veg-v').textContent=this.value+'h'">
          <span id="cfg-luz-veg-v" class="sval">18h</span>
        </div>
      </div>
      <p style="font-size:.72rem;color:var(--text2);margin-top:4px">Pre-floracion / Floracion / Final: 12&nbsp;h &mdash; fijo</p>
      <div class="cact" style="margin-top:14px">
        <button class="btn btn-p" onclick="savePlanta(this)">&#128190; Guardar planta</button>
      </div>
    </div>
  </details>

  <!-- Alertas -->
  <details class="csec" open>
    <summary style="border-left-color:#ff8c00">&#9888;&#65039; Alertas</summary>
    <div class="cbody">
      <div class="cfield">
        <label>Temp maxima (&#176;C)</label>
        <input type="number" id="cfg-tmax" min="20" max="45" value="30">
      </div>
      <div class="cfield">
        <label>Humedad minima (%) <span class="cfield-note">Plantula &amp; Vegetativo</span></label>
        <input type="number" id="cfg-hmin" min="10" max="80" value="45">
      </div>
      <div class="cfield">
        <label>Humedad maxima (%) <span class="cfield-note">Pre-flor &middot; Floracion &middot; Final</span></label>
        <input type="number" id="cfg-hmax" min="20" max="95" value="60">
      </div>
      <div class="cfield">
        <label>Calidad aire max (raw)</label>
        <input type="number" id="cfg-mq" min="100" max="4095" value="500">
      </div>
      <div class="cact">
        <button class="btn btn-p" onclick="saveAlertas()">&#128190; Guardar alertas</button>
      </div>
    </div>
  </details>

  <!-- Suelo y Bomba -->
  <details class="csec" open>
    <summary style="border-left-color:#00e5ff">
      &#128167; Suelo y Bomba
      <span class="csec-meta"><span id="cfg-pump-tag" class="csec-tag" style="background:rgba(255,18,79,.12);color:var(--c4);border-color:#ff124f44">Sin calibrar</span></span>
    </summary>
    <div class="cbody">
      <p class="cnote">Regar cuando el suelo este por debajo del umbral seco; omitir si supera el umbral humedo.</p>
      <div class="cfield">
        <label>Umbral seco (%)</label>
        <input type="number" id="cfg-smin" min="0" max="50" value="25">
      </div>
      <div class="cfield">
        <label>Umbral humedo (%)</label>
        <input type="number" id="cfg-smax" min="10" max="100" value="40">
      </div>
      <div class="cact">
        <button class="btn btn-p" onclick="saveUmbralesSuelo(this)">&#128190; Guardar umbrales</button>
      </div>
      <hr class="cfg-divider">
      <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:7px">
        <p style="font-size:.74rem;color:var(--text2);margin:0">Calibracion ADC del sensor de suelo</p>
        <button class="cfg-unlock" id="adc-unlock-btn" onclick="unlockAdc(this)">&#128275; Editar</button>
      </div>
      <p class="cnote" style="margin-bottom:9px">Valores obtenidos midiendo el sensor en suelo seco y completamente humedo (ver gauge Suelo RAW en dashboard).</p>
      <div class="cfield">
        <label>ADC seco (raw)</label>
        <input type="number" id="cfg-adry" class="cfg-ro" readonly min="0" max="4095" value="2150">
      </div>
      <div class="cfield">
        <label>ADC humedo (raw)</label>
        <input type="number" id="cfg-awet" class="cfg-ro" readonly min="0" max="4095" value="500">
      </div>
      <div class="cact" id="adc-save-row" style="display:none">
        <button class="btn btn-p" onclick="saveSuelo(this)">&#128190; Guardar calibracion ADC</button>
      </div>
    </div>
  </details>

  <!-- Bomba -->
  <details class="csec">
    <summary style="border-left-color:#ff124f">
      &#9881; Bomba
      <span class="csec-meta"><span id="cfg-pump-tag2" class="csec-tag" style="background:rgba(255,18,79,.12);color:var(--c4);border-color:#ff124f44">Sin calibrar</span></span>
    </summary>
    <div class="cbody">
      <div id="pump-st" style="font-size:.76rem;margin-bottom:10px">&#11036; Sin datos</div>
      <p class="cnote">Ejecuta la bomba 5 segundos y mide el agua recolectada para calibrar el caudal.</p>
      <div class="ctrl-row">
        <button class="btn btn-d" onclick="calPump(this)">Calibrar 5s</button>
      </div>
      <div class="irow" style="margin-top:10px">
        <input type="number" id="caudal-ml" min="1" max="9999" placeholder="mL recolectados">
        <button class="btn btn-o" onclick="sc({cmd:'caudal',args:document.getElementById('caudal-ml').value},this)">Guardar caudal</button>
      </div>
    </div>
  </details>

  <!-- Sistema (cerrado por defecto) -->
  <details class="csec">
    <summary style="border-left-color:#fe75fe">
      &#127757; Sistema
      <span class="csec-meta"><span class="csec-hint">Zona horaria</span></span>
    </summary>
    <div class="cbody">
      <p class="cnote">Offset respecto a UTC en horas. Ejemplo: -3 para UTC-3 (Argentina), -5 para UTC-5 (Mexico/Colombia).</p>
      <div class="cfield">
        <label>Zona horaria (UTC&#177;h)</label>
        <input type="number" id="cfg-tz" min="-12" max="14" value="-3">
        <button class="btn btn-p" onclick="sc({cmd:'timezone',args:document.getElementById('cfg-tz').value},this)">&#128190; Guardar</button>
      </div>
    </div>
  </details>

  <!-- Google Drive / Apps Script -->
  <details class="csec">
    <summary style="border-left-color:#34a853">
      &#128196; Google Drive
      <span class="csec-meta"><span id="gdrive-tag" class="csec-tag" style="background:rgba(52,168,83,.12);color:#34a853;border-color:#34a85344">Inactivo</span></span>
    </summary>
    <div class="cbody">
      <p class="cnote">Pega la URL del Apps Script desplegado como aplicacion web. Los logs del invernadero se guardaran automaticamente en tu Google Drive.</p>
      <div class="cfield">
        <label>URL del Apps Script</label>
        <input type="url" id="gdrive-url" placeholder="https://script.google.com/macros/s/.../exec" autocomplete="off">
      </div>
      <div id="gdrive-active-note" style="display:none;font-size:.75rem;color:#34a853;margin:4px 0 8px">
        &#9989; Activo &mdash; los logs se estan enviando a Drive.
      </div>
      <div class="cact">
        <button class="btn btn-d" onclick="saveGdrive('off',this)" style="background:rgba(255,18,79,.12);color:var(--c4);border-color:#ff124f44">&#10060; Desactivar</button>
        <button class="btn btn-p" onclick="saveGdrive(null,this)">&#128190; Guardar URL</button>
      </div>
    </div>
  </details>

  <!-- Redes WiFi guardadas -->
  <details class="csec" id="csec-wifi">
    <summary style="border-left-color:#ffb300">
      &#128246; Redes WiFi
      <span class="csec-meta"><span id="wifi-count-tag" class="csec-tag">0 / 5</span></span>
    </summary>
    <div class="cbody">
      <p class="cnote">Solo se guardan redes que hayan conectado exitosamente y obtenido IP. Si la red configurada falla al arrancar, el sistema prueba las guardadas automaticamente.</p>
      <div id="wifi-list"></div>
      <hr class="cfg-divider">
      <p style="font-size:.74rem;color:var(--text2);margin-bottom:8px" id="wifi-form-title">&#10133; Agregar red</p>
      <div class="cfield">
        <label>SSID</label>
        <input type="text" id="wifi-ssid-inp" placeholder="Nombre de la red" maxlength="32" autocomplete="off">
      </div>
      <div class="cfield">
        <label>Contrase&#241;a</label>
        <div class="pwd-wrap">
          <input type="password" id="wifi-pass-inp" placeholder="Contrase&#241;a" maxlength="64" autocomplete="new-password">
          <button class="eye" onclick="togglePwd('wifi-pass-inp',this)">&#128065;</button>
        </div>
      </div>
      <input type="hidden" id="wifi-edit-idx" value="-1">
      <div class="cact">
        <button class="btn btn-o" id="wifi-cancel-btn" onclick="wifiCancelEdit()" style="display:none">Cancelar</button>
        <button class="btn btn-p" onclick="wifiSave()">&#128190; Guardar red</button>
      </div>
    </div>
  </details>

</div><!-- /view-config -->

</main>

<script>
'use strict';
// ── State
var ws, tsBase=0, tsAt=0, fanAuto=true, autoIrr=true;
var logIdx={};
// Umbrales de visualizacion: se sincronizan desde /api/config al abrir Config
var V={tWarn:32,rhL:40,rhH:70,slL:25,mqW:500};
// Sensor sparkline histories — 20-minute rolling window
var SPK_WIN=10*60*1000; // ms  (ventana 10 min)
var spkData={
  temp:[],rh:[],soil:[],mq:[],'soil-raw':[],'fan-rpm':[]
};
var spkColor={
  temp:'#7a04eb',rh:'#00e5ff',soil:'#39ff14',mq:'#fe75fe',
  'soil-raw':'#ff8c00','fan-rpm':'#00e5ff'
};
// Fan RPM history — 20-minute rolling window
var rpmHist=[];

// ── WebSocket
function wsConn(){
  ws=new WebSocket('ws://'+location.host+'/ws');
  ws.onopen=function(){dot(true)};
  ws.onclose=function(){dot(false);setTimeout(wsConn,3000)};
  ws.onerror=function(){ws.close()};
  ws.onmessage=function(e){onWs(JSON.parse(e.data))};
}
function dot(ok){
  document.getElementById('ws-dot').className=ok?'ok':'';
}

function onWs(d){
  if(d.ts){tsBase=d.ts;tsAt=Date.now()}
  document.getElementById('ip-lbl').textContent=location.hostname;
  if(d.bot_name){
    var inp=document.getElementById('bot-name-hdr');
    if(inp&&!inp._dirty)inp.value=d.bot_name;
    updateBotLink(d.bot_name);
  }
  gauge('temp',d.temp_c,0,50,V.tWarn-6,V.tWarn);
  gauge('rh',d.rh_pct,0,100,V.rhL,V.rhH);
  gauge('soil',d.soil_pct,0,100,V.slL,80);
  gauge('mq',d.mq_raw,0,4095,V.mqW*0.7,V.mqW);
  // Soil raw ADC (menor = mas humedo; sin alerta de color, rango 0-4095)
  gauge('soil-raw',d.soil_adc,0,4095,9999,9999);
  // Fan RPM
  var spkTs=d.ts?d.ts*1000:Date.now();
  if(d.fan_rpm!==undefined){pushRpm(d.fan_rpm,spkTs)}
  gauge('fan-rpm',d.fan_rpm,0,3000,1500,2500);
  // Sparklines
  if(d.temp_c!=null)pushSpk('temp',d.temp_c,spkTs);
  if(d.rh_pct!=null)pushSpk('rh',d.rh_pct,spkTs);
  if(d.soil_pct!=null)pushSpk('soil',d.soil_pct,spkTs);
  if(d.mq_raw!=null)pushSpk('mq',d.mq_raw,spkTs);
  if(d.soil_adc!=null)pushSpk('soil-raw',d.soil_adc,spkTs);
  if(d.fan_rpm!=null)pushSpk('fan-rpm',d.fan_rpm,spkTs);
  alrt('card-temp',d.alert_temp);
  alrt('card-rh',d.alert_rh);
  alrt('card-mq',d.alert_mq);
  fanAuto=d.fan_auto;
  autoIrr=d.auto_irr;
  syncCtrl(d);
}

function alrt(id,on){
  document.getElementById(id).classList.toggle('card--alert',!!on);
}

// ── Gauges
var ARC=172.8;
function gauge(id,val,mn,mx,wL,wH){
  var arc=document.getElementById('arc-'+id);
  var txt=document.getElementById('txt-'+id);
  if(val==null||val===undefined){txt.textContent='--';return}
  var p=Math.max(0,Math.min(1,(val-mn)/(mx-mn)));
  arc.style.strokeDashoffset=ARC*(1-p);
  txt.textContent=(val%1!==0)?val.toFixed(1):val;
  arc.style.stroke=spkColor[id]||'#39ff14';
}

// ── Clock
setInterval(function(){
  if(!tsBase)return;
  var n=new Date((tsBase+Math.floor((Date.now()-tsAt)/1000))*1000);
  var hh=('0'+n.getHours()).slice(-2);
  var mm=('0'+n.getMinutes()).slice(-2);
  var ss=('0'+n.getSeconds()).slice(-2);
  document.getElementById('clock').textContent=hh+':'+mm+':'+ss;
},1000);

// ── Live chart scroll — redraw visible sparklines every 5 s so the
//    time axis advances in real time even between sensor readings
setInterval(function(){
  var keys=['temp','rh','soil','mq','soil-raw','fan-rpm'];
  keys.forEach(function(k){
    var wrap=document.getElementById('spk-wrap-'+k);
    if(wrap&&wrap.style.display!=='none')drawSpk(k);
  });
  var rpmWrap=document.getElementById('rpm-chart');
  if(rpmWrap)drawRpmChart();
},5000);

// ── Tabs
function showTab(name){
  ['dashboard','logs','config'].forEach(function(t){
    document.getElementById('view-'+t).hidden=(t!==name);
  });
  document.querySelectorAll('.tab-btn').forEach(function(b){
    b.classList.toggle('active',b.dataset.tab===name);
  });
  if(name==='config'){loadConfig();loadWifiNets();}
  if(name==='logs')initLogs();
}

// ── Send command
function sc(obj,btn){
  if(!ws||ws.readyState!==1){if(btn)flash(btn,false);return}
  ws.send(JSON.stringify(obj));
  if(btn)flash(btn,true);
}
function flash(b,ok){
  b.classList.add(ok?'fok':'ferr');
  setTimeout(function(){b.classList.remove('fok','ferr')},1000);
}

// ── Sync controls
function syncCtrl(d){
  var lb=document.getElementById('led-badge');
  if(d.led_manual){lb.className='badge b-on';lb.textContent='MANUAL ON'}
  else if(d.light_on){lb.className='badge b-on';lb.textContent='AUTO ON'}
  else{lb.className='badge b-off';lb.textContent='OFF'}
  if(d.led_pct!=null){
    document.getElementById('led-sl').value=d.led_pct;
    document.getElementById('led-sv').textContent=d.led_pct+'%';
    document.getElementById('cfg-led').value=d.led_pct;
    document.getElementById('cfg-led-v').textContent=d.led_pct+'%';
  }
  var fb=document.getElementById('fan-badge');
  fb.className='badge '+(d.fan_auto?'b-auto':'b-manual');
  fb.textContent=d.fan_auto?'AUTO':'MANUAL';
  document.getElementById('fan-auto-btn').textContent='Auto: '+(d.fan_auto?'ON':'OFF');
  if(d.fan_pct!=null){
    document.getElementById('fan-sl').value=d.fan_pct;
    document.getElementById('fan-sv').textContent=d.fan_pct+'%';
  }
  var ib=document.getElementById('irr-badge');
  ib.className='badge '+(d.auto_irr?'b-auto':'b-off');
  ib.textContent=d.auto_irr?'AUTO ON':'AUTO OFF';
  document.getElementById('tank').textContent=d.tank_ok?'\uD83D\uDFE2 Tanque: OK':'\uD83D\uDD34 Sin agua';
  if(d.pump_calibrated!==undefined){
    var ps=document.getElementById('pump-st');
    if(ps)ps.textContent=d.pump_calibrated?'\u2705 Calibrada':'\u26A0\uFE0F Sin calibrar';
    setPumpTag(d.pump_calibrated);
  }
}

function toggleFanAuto(btn){
  sc({cmd:'ventauto',args:fanAuto?'off':'on'},btn);
}
function toggleAutoIrr(btn){
  sc({cmd:'autoriego',args:autoIrr?'off':'on'},btn);
}
function calPump(btn){
  if(!confirm('Iniciar calibracion de bomba 5 segundos?'))return;
  sc({cmd:'calibrar',args:''},btn);
}

// ── Camera
function camConnect(){
  var u=document.getElementById('cam-url').value.trim();
  if(!u)return;
  localStorage.setItem('gh_cam_url',u);
  var img=document.getElementById('cam-feed');
  img.src=u;img.style.display='block';
  document.getElementById('cam-ph').style.display='none';
}
function camDisconnect(){
  document.getElementById('cam-feed').src='';
  document.getElementById('cam-feed').style.display='none';
  document.getElementById('cam-ph').style.display='flex';
}
function camErr(){
  document.getElementById('cam-ph').style.display='flex';
}

// ── Events
var EVI={RIEGO:'&#128167;',LUZ_ON:'&#128161;',LUZ_OFF:'&#127761;',ALERTA_ON:'&#9888;&#65039;',ALERTA_OFF:'&#9989;'};
function loadEvents(){
  fetch('/api/logs').then(function(r){return r.json()}).then(function(idx){
    var months=Object.keys(idx);
    if(!months.length)return Promise.reject('no months');
    var files=idx[months[months.length-1]];
    if(!files||!files.length)return Promise.reject('no files');
    return fetch('/api/logfile?path='+encodeURIComponent(files[files.length-1]));
  }).then(function(r){return r.text()}).then(function(csv){
    var rows=csv.split('\n').filter(function(l){return /RIEGO|LUZ_ON|LUZ_OFF|ALERTA_ON|ALERTA_OFF/.test(l)}).slice(-6);
    var ul=document.getElementById('ev-list');
    ul.innerHTML='';
    if(!rows.length){ul.innerHTML='<li style="color:var(--text2);font-size:.76rem">Sin eventos recientes</li>';return}
    rows.forEach(function(row){
      var p=row.split(',');
      var tp=(p[1]||'').trim().toUpperCase();
      var tm=(p[0]||'').split(' ')[1]||(p[0]||'');
      var dt=p.slice(2).join(',').trim().substring(0,35);
      var ic=EVI[tp]||'&bull;';
      var li=document.createElement('li');
      li.className='ev-row';
      li.innerHTML='<span>'+ic+'</span><span class="ev-badge ev-'+tp+'">'+tp+'</span><span class="ev-time">'+tm+'</span><span class="ev-detail">'+dt+'</span>';
      ul.appendChild(li);
    });
  }).catch(function(){
    document.getElementById('ev-list').innerHTML='<li style="color:var(--text2);font-size:.76rem">Sin datos de SD</li>';
  });
}

// ── Logs tab
function initLogs(){
  fetch('/api/logs').then(function(r){return r.json()}).then(function(idx){
    logIdx=idx;
    var sel=document.getElementById('log-month');
    sel.innerHTML='<option>-- Mes --</option>';
    Object.keys(idx).forEach(function(m){
      var o=document.createElement('option');o.value=m;o.textContent=m;sel.appendChild(o);
    });
  }).catch(function(){});
}
function loadMonth(){
  var m=document.getElementById('log-month').value;
  var files=logIdx[m]||[];
  var sel=document.getElementById('log-file');
  sel.innerHTML='<option>-- Archivo --</option>';
  files.forEach(function(f){
    var o=document.createElement('option');o.value=f;o.textContent=f.split('/').pop();sel.appendChild(o);
  });
}
function loadLogFile(){
  var path=document.getElementById('log-file').value;
  if(!path||path.startsWith('--'))return;
  var dl=document.getElementById('log-dl');
  dl.href='/api/logfile?path='+encodeURIComponent(path)+'&dl=1';
  dl.hidden=false;
  fetch('/api/logfile?path='+encodeURIComponent(path)).then(function(r){return r.text()}).then(function(csv){
    var lines=csv.split('\n').filter(function(l){return l.trim()});
    if(!lines.length){document.getElementById('log-wrap').innerHTML='<p style="color:var(--text2)">Archivo vacio.</p>';return}
    var hdr=lines[0].split(',');
    var html='<table><thead><tr>'+hdr.map(function(h){return'<th>'+h.trim()+'</th>'}).join('')+'</tr></thead><tbody>';
    lines.slice(1).forEach(function(row){
      var cells=row.split(',');
      var tp=(cells[1]||'').trim().toUpperCase();
      html+='<tr>'+cells.map(function(c,i){
        return i===1?'<td class="lt-'+tp+'">'+c.trim()+'</td>':'<td>'+c.trim()+'</td>';
      }).join('')+'</tr>';
    });
    html+='</tbody></table>';
    document.getElementById('log-wrap').innerHTML=html;
  }).catch(function(){});
}

// ── Config
function loadConfig(){
  fetch('/api/config').then(function(r){return r.json()}).then(function(c){
    sv('cfg-stage',c.stage);
    sv('cfg-pot',c.pot_l);
    sv('cfg-led',c.led_pct);document.getElementById('cfg-led-v').textContent=c.led_pct+'%';
    sv('ml-pl',c.ml_pl);sv('ml-veg',c.ml_veg);sv('ml-pre',c.ml_pre);sv('ml-flo',c.ml_flo);sv('ml-fin',c.ml_fin);
    sv('cfg-luz-pl',c.luz_pl);document.getElementById('cfg-luz-pl-v').textContent=c.luz_pl+'h';
    sv('cfg-luz-veg',c.luz_veg);document.getElementById('cfg-luz-veg-v').textContent=c.luz_veg+'h';
    sv('cfg-tmax',c.temp_max);sv('cfg-hmin',c.hum_min);sv('cfg-hmax',c.hum_max);sv('cfg-mq',c.mq_max);
    sv('cfg-smin',c.soil_min_pct);sv('cfg-smax',c.soil_max_pct);
    sv('cfg-adry',c.soil_dry_adc);sv('cfg-awet',c.soil_wet_adc);
    sv('cfg-tz',c.tz_offset);
    // Sincronizar umbrales de visualizacion desde el dispositivo
    if(c.temp_max)V.tWarn=c.temp_max;
    if(c.hum_min)V.rhL=c.hum_min;
    if(c.hum_max)V.rhH=c.hum_max;
    if(c.soil_min_pct)V.slL=c.soil_min_pct;
    if(c.mq_max)V.mqW=c.mq_max;
    // Actualizar badge de etapa en la seccion Planta
    updateStageTag(c.stage);
    // Actualizar chip de bomba y estado
    setPumpTag(c.pump_calibrated);
    var ps=document.getElementById('pump-st');
    if(ps)ps.textContent=c.pump_calibrated?'\u2705 Calibrada':'\u26A0\uFE0F Sin calibrar';
    if(c.bot_name){
      sv('tg-name',c.bot_name);
      var pl=document.getElementById('tg-prev-lnk');
      pl.href='https://t.me/'+c.bot_name.replace('@','');
      pl.textContent='t.me/'+c.bot_name.replace('@','');
      document.getElementById('tg-prev').style.display='block';
      var hi=document.getElementById('bot-name-hdr');
      if(hi&&!hi._dirty)hi.value=c.bot_name;
      updateBotLink(c.bot_name);
    }
    updateGdriveStatus(c.gdrive_enabled, c.gdrive_url);
  }).catch(function(){});
}
function sv(id,val){var e=document.getElementById(id);if(e&&val!=null)e.value=val}

function saveAlertas(){
  sc({cmd:'tempmax',args:document.getElementById('cfg-tmax').value});
  sc({cmd:'hummin', args:document.getElementById('cfg-hmin').value});
  sc({cmd:'hummax', args:document.getElementById('cfg-hmax').value});
  sc({cmd:'airemax',args:document.getElementById('cfg-mq').value});
}
function saveSuelo(btn){
  sc({cmd:'calsuelo',args:document.getElementById('cfg-adry').value+' '+document.getElementById('cfg-awet').value});
  if(btn)flash(btn,true);
}
function saveTelegram(){
  var tok=document.getElementById('tg-tok').value.trim();
  var nm=document.getElementById('tg-name').value.trim();
  if(!tok&&!nm)return;
  var body={};if(tok)body.token=tok;if(nm)body.name=nm;
  fetch('/api/telegram',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json()}).then(function(res){
      if(res.ok){
        if(nm){
          var pl=document.getElementById('tg-prev-lnk');
          pl.href='https://t.me/'+nm.replace('@','');pl.textContent='t.me/'+nm.replace('@','');
          document.getElementById('tg-prev').style.display='block';
          updateBotLink(nm);
          var hi=document.getElementById('bot-name-hdr');
          if(hi){hi.value=nm;hi._dirty=false;}
        }
        alert('Telegram guardado.');
      }
    }).catch(function(){alert('Error al guardar Telegram.')});
}

// ── Google Drive
function updateGdriveStatus(enabled, url){
  var tag=document.getElementById('gdrive-tag');
  var note=document.getElementById('gdrive-active-note');
  if(!tag)return;
  if(enabled){
    tag.textContent='Activo';
    tag.style.background='rgba(52,168,83,.18)';tag.style.color='#34a853';tag.style.borderColor='#34a85366';
    if(note)note.style.display='block';
    sv('gdrive-url', url||'');
  } else {
    tag.textContent='Inactivo';
    tag.style.background='rgba(100,100,100,.12)';tag.style.color='var(--text2)';tag.style.borderColor='rgba(100,100,100,.2)';
    if(note)note.style.display='none';
  }
}
function saveGdrive(off, btn){
  var url = off==='off' ? '' : (document.getElementById('gdrive-url').value||'').trim();
  if(!off && !url){alert('Ingresa una URL valida.');return;}
  if(url && !url.startsWith('http')){alert('La URL debe comenzar con https://');return;}
  fetch('/api/gdrive',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({url:url})})
    .then(function(r){return r.json();}).then(function(res){
      if(res.ok){
        updateGdriveStatus(url.length>0, url);
        if(btn)flash(btn,true);
      }
    }).catch(function(){if(btn)flash(btn,false);});
}

// ── WiFi network management
function escH(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function loadWifiNets(){
  fetch('/api/wifi').then(function(r){return r.json();}).then(function(nets){
    var list=document.getElementById('wifi-list');
    var tag=document.getElementById('wifi-count-tag');
    if(!list)return;
    tag.textContent=nets.length+' / 5';
    if(!nets.length){
      list.innerHTML='<p style="font-size:.75rem;color:var(--text2);margin:0 0 4px">No hay redes guardadas.</p>';
      return;
    }
    list.innerHTML=nets.map(function(n){
      var safe=n.ssid.replace(/\\/g,'\\\\').replace(/'/g,"\\'");
      return '<div class="wifi-row" id="wrow-'+n.idx+'">'
        +'<span class="wifi-ssid" onclick="wifiStartEdit('+n.idx+',\''+safe+'\')" title="Editar">'+escH(n.ssid)+'</span>'
        +'<div class="wifi-acts">'
        +'<button class="btn btn-o" style="padding:3px 9px;font-size:.72rem" onclick="wifiStartEdit('+n.idx+',\''+safe+'\')">&#9998;</button>'
        +'<button class="btn btn-d" style="padding:3px 9px;font-size:.72rem" onclick="wifiDel('+n.idx+')">&#10005;</button>'
        +'</div></div>';
    }).join('');
  }).catch(function(){});
}
function wifiSave(){
  var ssid=document.getElementById('wifi-ssid-inp').value.trim();
  var pass=document.getElementById('wifi-pass-inp').value;
  var idx=parseInt(document.getElementById('wifi-edit-idx').value);
  if(!ssid){alert('Ingresa el SSID de la red.');return;}
  var url=idx>=0?'/api/wifi/edit':'/api/wifi/add';
  var body=idx>=0?{idx:idx,ssid:ssid,pass:pass}:{ssid:ssid,pass:pass};
  fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json();})
    .then(function(d){
      if(d.ok){wifiCancelEdit();loadWifiNets();}
      else alert('Error: '+(d.error||'desconocido'));
    }).catch(function(){alert('Error de conexion.');});
}
function wifiDel(idx){
  if(!confirm('Eliminar esta red guardada?'))return;
  fetch('/api/wifi/del',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({idx:idx})})
    .then(function(r){return r.json();})
    .then(function(d){if(d.ok)loadWifiNets();});
}
function wifiStartEdit(idx,ssid){
  document.getElementById('wifi-edit-idx').value=idx;
  document.getElementById('wifi-ssid-inp').value=ssid;
  document.getElementById('wifi-pass-inp').value='';
  document.getElementById('wifi-form-title').textContent='\u270F\uFE0F Editar red';
  document.getElementById('wifi-cancel-btn').style.display='';
  document.getElementById('wifi-ssid-inp').focus();
}
function wifiCancelEdit(){
  document.getElementById('wifi-edit-idx').value=-1;
  document.getElementById('wifi-ssid-inp').value='';
  document.getElementById('wifi-pass-inp').value='';
  document.getElementById('wifi-form-title').textContent='\u2795 Agregar red';
  document.getElementById('wifi-cancel-btn').style.display='none';
}

function updateBotLink(name){
  var clean=name?name.replace('@',''):'';
  var inp=document.getElementById('bot-name-hdr');
  var lnk=document.getElementById('tg-link');
  var edt=document.getElementById('tg-edit-btn');
  if(!lnk)return;
  if(clean){
    lnk.href='https://t.me/'+clean;
    lnk.innerHTML='&#9992; @'+clean;
    lnk.style.display='flex';
    if(inp)inp.style.display='none';
    if(edt)edt.style.display='inline-block';
  } else {
    lnk.style.display='none';
    if(inp)inp.style.display='';
    if(edt)edt.style.display='none';
  }
}
function editBotNameHdr(){
  var inp=document.getElementById('bot-name-hdr');
  var lnk=document.getElementById('tg-link');
  var edt=document.getElementById('tg-edit-btn');
  if(inp){inp.style.display='';inp.focus();}
  if(lnk)lnk.style.display='none';
  if(edt)edt.style.display='none';
}
function onBotNameInput(val){
  var inp=document.getElementById('bot-name-hdr');
  if(inp)inp._dirty=true;
}
function saveBotNameHdr(val){
  var nm=val.trim();
  if(!nm)return;
  var body={name:nm};
  fetch('/api/telegram',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json()}).then(function(res){
      if(res.ok){
        var inp=document.getElementById('bot-name-hdr');
        if(inp)inp._dirty=false;
        updateBotLink(nm);
        // sincronizar con campo de config si esta abierto
        sv('tg-name',nm);
        var pl=document.getElementById('tg-prev-lnk');
        if(pl){pl.href='https://t.me/'+nm.replace('@','');pl.textContent='t.me/'+nm.replace('@','');}
        document.getElementById('tg-prev').style.display='block';
      }
    }).catch(function(){});
}
function togglePwd(id,btn){
  var i=document.getElementById(id);
  i.type=i.type==='password'?'text':'password';
  btn.textContent=i.type==='password'?'\u{1F441}':'\uD83D\uDE48';
}

// ── Config helpers
function updateStageTag(s){
  var t=document.getElementById('cfg-stage-tag');
  if(!t)return;
  var map={pl:'Plantula',veg:'Vegetativo',pre:'Pre-flor',flo:'Floracion',fin:'Final'};
  t.textContent=map[s]||s||'---';
}
function setPumpTag(cal){
  ['cfg-pump-tag','cfg-pump-tag2'].forEach(function(id){
    var t=document.getElementById(id);
    if(!t)return;
    if(cal){
      t.textContent='Calibrada';
      t.style.cssText='background:rgba(57,255,20,.12);color:var(--neon);border-color:#39ff1440';
    } else {
      t.textContent='Sin calibrar';
      t.style.cssText='background:rgba(255,18,79,.12);color:var(--c4);border-color:#ff124f44';
    }
  });
}
function unlockAdc(btn){
  var dry=document.getElementById('cfg-adry');
  var wet=document.getElementById('cfg-awet');
  var row=document.getElementById('adc-save-row');
  var locked=dry.readOnly;
  dry.readOnly=!locked;wet.readOnly=!locked;
  dry.classList.toggle('unlocked',locked);wet.classList.toggle('unlocked',locked);
  row.style.display=locked?'flex':'none';
  btn.textContent=locked?'\uD83D\uDD12 Bloquear':'\uD83D\uDD13 Editar';
}

// ── Fan RPM sparkline
function pushRpm(v,ts){
  var t=ts||Date.now();
  rpmHist.push({t:t,v:v});
  var cutoff=t-SPK_WIN;
  while(rpmHist.length>0&&rpmHist[0].t<cutoff)rpmHist.shift();
  var lbl=document.getElementById('fan-rpm-lbl');
  if(lbl)lbl.textContent=v+' RPM';
  drawRpmChart();
}
function drawRpmChart(){
  var svg=document.getElementById('rpm-chart');
  if(!svg||rpmHist.length<1)return;
  var VW=600,VH=280,ML=72,MR=14,MT=16,MB=40;
  var PW=VW-ML-MR,PH=VH-MT-MB;
  var col='#00e5ff';
  var now=Date.now();
  var tMin=now-SPK_WIN;
  var vals=rpmHist.map(function(p){return p.v;});
  var mn=0,mx=Math.max.apply(null,vals);
  if(mx<100)mx=100;
  function tx(t){return (ML+(t-tMin)/SPK_WIN*PW).toFixed(1);}
  function ty(v){return (MT+PH-((v-mn)/(mx-mn))*PH).toFixed(1);}
  var s='';
  for(var m=2;m<=10;m+=2){
    var gx=(ML+(1-m/10)*PW).toFixed(1);
    s+='<line x1="'+gx+'" y1="'+MT+'" x2="'+gx+'" y2="'+(MT+PH)+'" stroke="#ffffff15" stroke-width="0.8" stroke-dasharray="3,4"/>';
  }
  s+='<line x1="'+ML+'" y1="'+MT+'" x2="'+ML+'" y2="'+(MT+PH)+'" stroke="#ffffff30" stroke-width="1"/>';
  s+='<line x1="'+ML+'" y1="'+(MT+PH)+'" x2="'+(ML+PW)+'" y2="'+(MT+PH)+'" stroke="#ffffff30" stroke-width="1"/>';
  [{m:10,l:'-10m'},{m:5,l:'-5m'},{m:0,l:'ahora'}].forEach(function(lx){
    var xt=(ML+(1-lx.m/10)*PW).toFixed(1);
    s+='<line x1="'+xt+'" y1="'+(MT+PH)+'" x2="'+xt+'" y2="'+(MT+PH+4)+'" stroke="#ffffff35" stroke-width="1"/>';
    s+='<text x="'+xt+'" y="'+(VH-3)+'" text-anchor="middle" fill="#9b59b6" font-size="15" font-family="Courier New">'+lx.l+'</text>';
  });
  [mn,Math.round((mn+mx)/2),mx].forEach(function(v){
    var yt=ty(v);
    s+='<text x="'+(ML-4)+'" y="'+yt+'" text-anchor="end" dominant-baseline="middle" fill="#9b59b6" font-size="15" font-family="Courier New">'+v+'</text>';
  });
  if(rpmHist.length>=2){
    var pts=rpmHist.map(function(p){return tx(p.t)+','+ty(p.v);}).join(' ');
    var bot=(MT+PH).toFixed(1);
    s+='<polygon points="'+tx(rpmHist[0].t)+','+bot+' '+pts+' '+tx(rpmHist[rpmHist.length-1].t)+','+bot+'" fill="'+col+'" opacity="0.15"/>';
    s+='<polyline points="'+pts+'" fill="none" stroke="'+col+'" stroke-width="3.5" stroke-linejoin="round"/>';
  }
  var lp=rpmHist[rpmHist.length-1];
  s+='<circle cx="'+tx(lp.t)+'" cy="'+ty(lp.v)+'" r="6" fill="'+col+'"/>';
  svg.innerHTML=s;
}

// ── Sensor sparklines
function toggleSpk(key,lbl){
  var wrap=document.getElementById('spk-wrap-'+key);
  if(!wrap)return;
  var arrow=lbl.querySelector('.spk-arrow');
  var opening=wrap.style.display==='none';
  wrap.style.display=opening?'block':'none';
  if(arrow)arrow.classList.toggle('open',opening);
  if(opening)drawSpk(key);
}
function pushSpk(key,val,ts){
  var h=spkData[key];
  if(!h)return;
  var t=ts||Date.now();
  h.push({t:t,v:val});
  var cutoff=t-SPK_WIN;
  while(h.length>0&&h[0].t<cutoff)h.shift();
  drawSpk(key);
}
function drawSpk(key){
  var svg=document.getElementById('spk-'+key);
  var h=spkData[key];
  if(!svg||!h||h.length<1)return;
  var VW=600,VH=280,ML=72,MR=14,MT=16,MB=40;
  var PW=VW-ML-MR,PH=VH-MT-MB;
  var col=spkColor[key]||'#7a04eb';
  var now=Date.now();
  var tMin=now-SPK_WIN;
  var vals=h.map(function(p){return p.v;});
  var mn=Math.min.apply(null,vals);
  var mx=Math.max.apply(null,vals);
  if(mx===mn){mn=Math.max(0,mn-1);mx=mx+1;}
  var vRange=mx-mn;
  function tx(t){return (ML+(t-tMin)/SPK_WIN*PW).toFixed(1);}
  function ty(v){return (MT+PH-((v-mn)/vRange)*PH).toFixed(1);}
  function fmtV(v){if(v%1===0)return ''+v;if(Math.abs(v)>=10)return v.toFixed(0);return v.toFixed(1);}
  var s='';
  // vertical grid at 2-min intervals (10-min window)
  for(var m=2;m<=10;m+=2){
    var gx=(ML+(1-m/10)*PW).toFixed(1);
    s+='<line x1="'+gx+'" y1="'+MT+'" x2="'+gx+'" y2="'+(MT+PH)+'" stroke="#ffffff15" stroke-width="0.8" stroke-dasharray="3,4"/>';
  }
  // horizontal grid at midpoint
  var gy=ty(mn+vRange/2);
  s+='<line x1="'+ML+'" y1="'+gy+'" x2="'+(ML+PW)+'" y2="'+gy+'" stroke="#ffffff12" stroke-width="0.8" stroke-dasharray="3,5"/>';
  // axes
  s+='<line x1="'+ML+'" y1="'+MT+'" x2="'+ML+'" y2="'+(MT+PH)+'" stroke="#ffffff30" stroke-width="1"/>';
  s+='<line x1="'+ML+'" y1="'+(MT+PH)+'" x2="'+(ML+PW)+'" y2="'+(MT+PH)+'" stroke="#ffffff30" stroke-width="1"/>';
  // X axis labels
  [{m:10,l:'-10m'},{m:5,l:'-5m'},{m:0,l:'ahora'}].forEach(function(lx){
    var xt=(ML+(1-lx.m/10)*PW).toFixed(1);
    s+='<line x1="'+xt+'" y1="'+(MT+PH)+'" x2="'+xt+'" y2="'+(MT+PH+4)+'" stroke="#ffffff35" stroke-width="1"/>';
    s+='<text x="'+xt+'" y="'+(VH-3)+'" text-anchor="middle" fill="#9b59b6" font-size="15" font-family="Courier New">'+lx.l+'</text>';
  });
  // Y axis labels: min, mid, max
  [mn,mn+vRange/2,mx].forEach(function(v){
    var yt=ty(v);
    s+='<text x="'+(ML-4)+'" y="'+yt+'" text-anchor="end" dominant-baseline="middle" fill="#9b59b6" font-size="15" font-family="Courier New">'+fmtV(v)+'</text>';
  });
  // data area and line
  if(h.length>=2){
    var pts=h.map(function(p){return tx(p.t)+','+ty(p.v);}).join(' ');
    var bot=(MT+PH).toFixed(1);
    s+='<polygon points="'+tx(h[0].t)+','+bot+' '+pts+' '+tx(h[h.length-1].t)+','+bot+'" fill="'+col+'" opacity="0.15"/>';
    s+='<polyline points="'+pts+'" fill="none" stroke="'+col+'" stroke-width="3.5" stroke-linejoin="round"/>';
  }
  // dot at latest reading
  var lp=h[h.length-1];
  s+='<circle cx="'+tx(lp.t)+'" cy="'+ty(lp.v)+'" r="6" fill="'+col+'"/>';
  svg.innerHTML=s;
}

// ── Config save helpers
function savePlanta(btn){
  sc({cmd:'maceta',args:document.getElementById('cfg-pot').value});
  sc({cmd:'led',args:document.getElementById('cfg-led').value});
  sc({cmd:'ml',args:'pl '+document.getElementById('ml-pl').value});
  sc({cmd:'ml',args:'veg '+document.getElementById('ml-veg').value});
  sc({cmd:'ml',args:'pre '+document.getElementById('ml-pre').value});
  sc({cmd:'ml',args:'flo '+document.getElementById('ml-flo').value});
  sc({cmd:'ml',args:'fin '+document.getElementById('ml-fin').value});
  sc({cmd:'luz',args:'pl '+document.getElementById('cfg-luz-pl').value});
  sc({cmd:'luz',args:'veg '+document.getElementById('cfg-luz-veg').value});
  if(btn)flash(btn,true);
}
function saveUmbralesSuelo(btn){
  sc({cmd:'suelomin',args:document.getElementById('cfg-smin').value});
  sc({cmd:'suelomax',args:document.getElementById('cfg-smax').value});
  if(btn)flash(btn,true);
}

// ── Init
(function(){
  var cu=localStorage.getItem('gh_cam_url');
  if(cu){document.getElementById('cam-url').value=cu;camConnect()}
  wsConn();
  loadEvents();
}());
</script>
</body>
</html>
)rawliteral";
