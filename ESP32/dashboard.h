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
#tg-link{
  display:none;align-items:center;gap:4px;
  background:rgba(0,172,238,.12);border:1px solid #00acee44;
  color:#00acee;padding:4px 9px;border-radius:5px;font-size:.75rem;
}
#tg-link:hover{background:rgba(0,172,238,.22)}
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
.gauge-label{font-size:.76rem;color:var(--text2)}
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
</style>
</head>
<body>

<!-- HEADER -->
<header>
  <h1>&#9670; Greenhouse</h1>
  <nav>
    <button class="tab-btn active" data-tab="dashboard" onclick="showTab('dashboard')">Dashboard</button>
    <button class="tab-btn" data-tab="logs" onclick="showTab('logs')">Logs</button>
    <button class="tab-btn" data-tab="config" onclick="showTab('config')">Config</button>
  </nav>
  <a id="tg-link" href="#" target="_blank" rel="noopener">&#129302; Bot</a>
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
      <div class="gauge-label">&#127777; Temperatura</div>
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
      <div class="gauge-label">&#128167; Humedad</div>
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
      <div class="gauge-label">&#127807; Suelo</div>
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
      <div class="gauge-label">&#127787; Aire MQ</div>
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
      <div class="gauge-label">&#127807; Suelo RAW</div>
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
      <div class="gauge-label">&#127744; Fan RPM</div>
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
        <svg id="rpm-chart" viewBox="0 0 200 36" preserveAspectRatio="none"
          style="width:100%;height:36px;background:rgba(0,229,255,.05);border-radius:4px;display:block"></svg>
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

    <!-- Bomba -->
    <div class="card">
      <div class="card-title">&#9881; Bomba</div>
      <div id="pump-st" style="font-size:.76rem;margin-bottom:7px">&#11036; Sin datos</div>
      <div class="ctrl-row">
        <button class="btn btn-d" onclick="calPump(this)">Calibrar 5s</button>
      </div>
      <div class="irow" style="margin-top:7px">
        <input type="number" id="caudal-ml" min="1" max="9999" placeholder="mL recolectados">
        <button class="btn btn-o" onclick="sc({cmd:'caudal',args:document.getElementById('caudal-ml').value},this)">Guardar</button>
      </div>
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
        <label>Dias entre riegos</label>
        <input type="number" id="cfg-pause" min="1" max="5" value="2">
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

</div><!-- /view-config -->

</main>

<script>
'use strict';
// ── State
var ws, tsBase=0, tsAt=0, fanAuto=true, autoIrr=true;
var logIdx={};
// Umbrales de visualizacion: se sincronizan desde /api/config al abrir Config
var V={tWarn:32,rhL:40,rhH:70,slL:25,mqW:500};
// Fan RPM history
var rpmHist=[], RPM_MAX_PTS=40;

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
    var t=document.getElementById('tg-link');
    t.href='https://t.me/'+d.bot_name.replace('@','');
    t.style.display='flex';
  }
  gauge('temp',d.temp_c,0,50,V.tWarn-6,V.tWarn);
  gauge('rh',d.rh_pct,0,100,V.rhL,V.rhH);
  gauge('soil',d.soil_pct,0,100,V.slL,80);
  gauge('mq',d.mq_raw,0,4095,V.mqW*0.7,V.mqW);
  // Soil raw ADC (menor = mas humedo; sin alerta de color, rango 0-4095)
  gauge('soil-raw',d.soil_adc,0,4095,9999,9999);
  // Fan RPM
  if(d.fan_rpm!==undefined){pushRpm(d.fan_rpm)}
  gauge('fan-rpm',d.fan_rpm,0,3000,1500,2500);
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
  arc.style.stroke=(val>=wH)?'#ff124f':(val>=wL)?'#ff8c00':'#39ff14';
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

// ── Tabs
function showTab(name){
  ['dashboard','logs','config'].forEach(function(t){
    document.getElementById('view-'+t).hidden=(t!==name);
  });
  document.querySelectorAll('.tab-btn').forEach(function(b){
    b.classList.toggle('active',b.dataset.tab===name);
  });
  if(name==='config')loadConfig();
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
    ps.textContent=d.pump_calibrated?'\u2705 Calibrada':'\u26A0\uFE0F Sin calibrar';
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
    sv('cfg-pot',c.pot_l);sv('cfg-pause',c.pause_days);
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
    // Actualizar chip de bomba en la seccion Suelo y Bomba
    setPumpTag(c.pump_calibrated);
    document.getElementById('pump-st').textContent=c.pump_calibrated?'\u2705 Calibrada':'\u26A0\uFE0F Sin calibrar';
    // Actualizar badge de etapa en la seccion Planta
    updateStageTag(c.stage);
    if(c.bot_name){
      sv('tg-name',c.bot_name);
      var pl=document.getElementById('tg-prev-lnk');
      pl.href='https://t.me/'+c.bot_name.replace('@','');
      pl.textContent='t.me/'+c.bot_name.replace('@','');
      document.getElementById('tg-prev').style.display='block';
    }
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
          var tl=document.getElementById('tg-link');tl.href=pl.href;tl.style.display='flex';
        }
        alert('Telegram guardado.');
      }
    }).catch(function(){alert('Error al guardar Telegram.')});
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
  var t=document.getElementById('cfg-pump-tag');
  if(!t)return;
  if(cal){
    t.textContent='Calibrada';
    t.style.cssText='background:rgba(57,255,20,.12);color:var(--neon);border-color:#39ff1440';
  } else {
    t.textContent='Sin calibrar';
    t.style.cssText='background:rgba(255,18,79,.12);color:var(--c4);border-color:#ff124f44';
  }
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
function pushRpm(v){
  rpmHist.push(v);
  if(rpmHist.length>RPM_MAX_PTS)rpmHist.shift();
  var lbl=document.getElementById('fan-rpm-lbl');
  if(lbl)lbl.textContent=v+' RPM';
  drawRpmChart();
}
function drawRpmChart(){
  var svg=document.getElementById('rpm-chart');
  if(!svg||rpmHist.length<2)return;
  var W=200,H=36;
  var mx=Math.max.apply(null,rpmHist);
  if(mx<100)mx=100;
  var pts=rpmHist.map(function(r,i){
    var x=((i/(RPM_MAX_PTS-1))*W).toFixed(1);
    var y=(H-(r/mx)*(H-4)-2).toFixed(1);
    return x+','+y;
  }).join(' ');
  svg.innerHTML='<polyline points="'+pts+'" fill="none" stroke="#00e5ff" stroke-width="1.5" stroke-linejoin="round"/>';
}

// ── Config save helpers
function savePlanta(btn){
  sc({cmd:'maceta',args:document.getElementById('cfg-pot').value});
  sc({cmd:'pausariego',args:document.getElementById('cfg-pause').value});
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
