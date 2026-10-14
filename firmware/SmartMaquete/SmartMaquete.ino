/*
 * ============================================================
 *  Smart Maquete — Controlador de Iluminação para Maquetes
 *  v1.0
 *
 *  Hardware : ESP32-S3 (YD-ESP32-S3) + 8x MOSFET IRLB8721
 *             LM2596 5V reg + Mean Well LRS-100-12 (12V PSU)
 *  Pinos PWM : 1, 2, 42, 41, 40, 39, 38, 37
 *  Acesso    : Wi-Fi AP  SSID=SmartMaquete  PASS=maquete123
 *              http://192.168.4.1
 *
 *  Features
 *  --------
 *  • 8 canais PWM independentes (10-bit, 5 kHz)
 *  • Master dimmer global
 *  • Nomes de canal editáveis
 *  • 6 cenas salváveis / carregáveis
 *  • Persistência via Preferences (NVS)
 *  • Web UI responsiva servida inline (sem SPIFFS)
 *  • API REST JSON simples
 *
 *  Author : Nicolas / Smart Maquete
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ─── CONFIG ──────────────────────────────────────────────────
const char* AP_SSID     = "SmartMaquete";
const char* AP_PASSWORD = "maquete123";
const IPAddress AP_IP(192, 168, 4, 1);

const int   NUM_CHANNELS  = 8;
const int   CHANNEL_PINS[NUM_CHANNELS] = {1, 2, 42, 41, 40, 39, 38, 37};
const int   PWM_FREQ      = 5000;
const int   PWM_RES       = 10;       // 10-bit → 0-1023
const int   PWM_MAX       = 1023;
const int   MAX_SCENES    = 6;

// ─── ESTADO ──────────────────────────────────────────────────
int    chVal[NUM_CHANNELS]  = {0,0,0,0,0,0,0,0};
String chName[NUM_CHANNELS] = {
  "Fachada","Interior","Jardim","Piscina",
  "Garagem","Telhado","Acento 1","Acento 2"
};
int masterVal = PWM_MAX;

struct Scene {
  char   name[32];
  int    values[NUM_CHANNELS];
  bool   used;
};
Scene scenes[MAX_SCENES];

WebServer   server(80);
Preferences prefs;

// ─── PWM helpers ─────────────────────────────────────────────
void applyChannel(int ch) {
  int out = (int)((long)chVal[ch] * masterVal / PWM_MAX);
  out = constrain(out, 0, PWM_MAX);
  ledcWrite(CHANNEL_PINS[ch], out);
}

void applyAll() {
  for (int i = 0; i < NUM_CHANNELS; i++) applyChannel(i);
}

// ─── PERSISTÊNCIA ────────────────────────────────────────────
void saveChannels() {
  prefs.begin("sm_ch", false);
  for (int i = 0; i < NUM_CHANNELS; i++) {
    prefs.putInt(("v" + String(i)).c_str(), chVal[i]);
    prefs.putString(("n" + String(i)).c_str(), chName[i]);
  }
  prefs.putInt("master", masterVal);
  prefs.end();
}

void loadChannels() {
  prefs.begin("sm_ch", true);
  for (int i = 0; i < NUM_CHANNELS; i++) {
    chVal[i]  = prefs.getInt(("v" + String(i)).c_str(), 0);
    chName[i] = prefs.getString(("n" + String(i)).c_str(), chName[i]);
  }
  masterVal = prefs.getInt("master", PWM_MAX);
  prefs.end();
}

void saveScenes() {
  prefs.begin("sm_sc", false);
  for (int s = 0; s < MAX_SCENES; s++) {
    String pfx = "s" + String(s);
    prefs.putBool((pfx + "u").c_str(), scenes[s].used);
    if (scenes[s].used) {
      prefs.putString((pfx + "n").c_str(), scenes[s].name);
      for (int i = 0; i < NUM_CHANNELS; i++)
        prefs.putInt((pfx + "v" + String(i)).c_str(), scenes[s].values[i]);
    }
  }
  prefs.end();
}

void loadScenes() {
  prefs.begin("sm_sc", true);
  for (int s = 0; s < MAX_SCENES; s++) {
    String pfx = "s" + String(s);
    scenes[s].used = prefs.getBool((pfx + "u").c_str(), false);
    if (scenes[s].used) {
      String n = prefs.getString((pfx + "n").c_str(), "Cena " + String(s+1));
      strncpy(scenes[s].name, n.c_str(), 31);
      scenes[s].name[31] = 0;
      for (int i = 0; i < NUM_CHANNELS; i++)
        scenes[s].values[i] = prefs.getInt((pfx + "v" + String(i)).c_str(), 0);
    }
  }
  prefs.end();
}

// ─── JSON helpers ────────────────────────────────────────────
String stateJson() {
  String j = "{\"master\":" + String(masterVal) + ",\"channels\":[";
  for (int i = 0; i < NUM_CHANNELS; i++) {
    j += "{\"id\":" + String(i) +
         ",\"name\":\"" + chName[i] + "\"" +
         ",\"value\":" + String(chVal[i]) + "}";
    if (i < NUM_CHANNELS - 1) j += ",";
  }
  j += "],\"scenes\":[";
  for (int s = 0; s < MAX_SCENES; s++) {
    j += "{\"id\":" + String(s) +
         ",\"used\":" + (scenes[s].used ? "true" : "false") +
         ",\"name\":\"" + (scenes[s].used ? String(scenes[s].name) : "") + "\"}";
    if (s < MAX_SCENES - 1) j += ",";
  }
  j += "]}";
  return j;
}

// ─── HANDLERS REST ───────────────────────────────────────────

// GET /api/state
void handleGetState() {
  server.send(200, "application/json", stateJson());
}

// POST /api/channel  body: id=0&value=512
void handleSetChannel() {
  if (!server.hasArg("id") || !server.hasArg("value")) {
    server.send(400, "application/json", "{\"error\":\"missing args\"}");
    return;
  }
  int id  = server.arg("id").toInt();
  int val = server.arg("value").toInt();
  if (id < 0 || id >= NUM_CHANNELS) { server.send(400, "application/json", "{\"error\":\"bad id\"}"); return; }
  chVal[id] = constrain(val, 0, PWM_MAX);
  applyChannel(id);
  saveChannels();
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/master  body: value=1023
void handleSetMaster() {
  if (!server.hasArg("value")) { server.send(400, "application/json", "{\"error\":\"missing value\"}"); return; }
  masterVal = constrain(server.arg("value").toInt(), 0, PWM_MAX);
  applyAll();
  saveChannels();
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/rename  body: id=0&name=Fachada
void handleRename() {
  if (!server.hasArg("id") || !server.hasArg("name")) { server.send(400, "application/json", "{\"error\":\"missing args\"}"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= NUM_CHANNELS) { server.send(400, "application/json", "{\"error\":\"bad id\"}"); return; }
  chName[id] = server.arg("name").substring(0, 24);
  saveChannels();
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/scene/save  body: id=0&name=Dia
void handleSceneSave() {
  if (!server.hasArg("id") || !server.hasArg("name")) { server.send(400, "application/json", "{\"error\":\"missing args\"}"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= MAX_SCENES) { server.send(400, "application/json", "{\"error\":\"bad id\"}"); return; }
  scenes[id].used = true;
  String n = server.arg("name").substring(0, 31);
  strncpy(scenes[id].name, n.c_str(), 31);
  scenes[id].name[31] = 0;
  for (int i = 0; i < NUM_CHANNELS; i++) scenes[id].values[i] = chVal[i];
  saveScenes();
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/scene/load  body: id=0
void handleSceneLoad() {
  if (!server.hasArg("id")) { server.send(400, "application/json", "{\"error\":\"missing id\"}"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= MAX_SCENES || !scenes[id].used) { server.send(400, "application/json", "{\"error\":\"scene not found\"}"); return; }
  for (int i = 0; i < NUM_CHANNELS; i++) chVal[i] = scenes[id].values[i];
  applyAll();
  saveChannels();
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/scene/delete  body: id=0
void handleSceneDelete() {
  if (!server.hasArg("id")) { server.send(400, "application/json", "{\"error\":\"missing id\"}"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= MAX_SCENES) { server.send(400, "application/json", "{\"error\":\"bad id\"}"); return; }
  scenes[id].used = false;
  scenes[id].name[0] = 0;
  saveScenes();
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/alloff
void handleAllOff() {
  for (int i = 0; i < NUM_CHANNELS; i++) chVal[i] = 0;
  applyAll();
  saveChannels();
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/allon
void handleAllOn() {
  for (int i = 0; i < NUM_CHANNELS; i++) chVal[i] = PWM_MAX;
  applyAll();
  saveChannels();
  server.send(200, "application/json", "{\"ok\":true}");
}

// ─── WEB UI (inline HTML) ────────────────────────────────────
void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Maquete</title>
<style>
  :root {
    --bg:      #0d0f14;
    --surface: #161b25;
    --card:    #1e2535;
    --accent:  #f0a500;
    --accent2: #e06030;
    --text:    #e8eaf0;
    --muted:   #6b7280;
    --radius:  10px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', system-ui, sans-serif;
    min-height: 100vh;
    padding: 16px;
  }
  h1 {
    text-align: center;
    font-size: 1.5rem;
    letter-spacing: .12em;
    text-transform: uppercase;
    color: var(--accent);
    margin-bottom: 6px;
  }
  .sub { text-align:center; color:var(--muted); font-size:.8rem; margin-bottom:20px; }

  /* Master */
  .master-card {
    background: var(--surface);
    border: 1px solid var(--accent);
    border-radius: var(--radius);
    padding: 14px 18px;
    margin-bottom: 18px;
  }
  .master-card label { font-size:.85rem; color:var(--accent); letter-spacing:.08em; }
  .master-card .val  { float:right; font-weight:700; color:var(--accent); }

  /* Channels grid */
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
    gap: 14px;
    margin-bottom: 20px;
  }
  .ch-card {
    background: var(--card);
    border-radius: var(--radius);
    padding: 14px;
    border: 1px solid #2a3347;
    transition: border-color .2s;
  }
  .ch-card:hover { border-color: var(--accent); }
  .ch-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 10px;
  }
  .ch-name {
    background: transparent;
    border: none;
    border-bottom: 1px dashed var(--muted);
    color: var(--text);
    font-size: .95rem;
    font-weight: 600;
    width: 70%;
    outline: none;
    padding: 2px 4px;
  }
  .ch-name:focus { border-bottom-color: var(--accent); }
  .ch-pct { color: var(--accent); font-weight:700; font-size:.95rem; }

  /* Sliders */
  input[type=range] {
    -webkit-appearance: none;
    width: 100%;
    height: 6px;
    border-radius: 3px;
    background: #2a3347;
    outline: none;
    cursor: pointer;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 18px; height: 18px;
    border-radius: 50%;
    background: var(--accent);
    border: 2px solid var(--bg);
    transition: transform .1s;
  }
  input[type=range]::-webkit-slider-thumb:active { transform: scale(1.3); }

  /* Buttons */
  .btn-row { display:flex; gap:10px; flex-wrap:wrap; margin-bottom:20px; }
  button {
    padding: 9px 18px;
    border: none;
    border-radius: 6px;
    cursor: pointer;
    font-size: .85rem;
    font-weight: 600;
    letter-spacing: .05em;
    transition: opacity .15s, transform .1s;
  }
  button:active { transform: scale(.96); }
  .btn-gold  { background: var(--accent);  color: #000; }
  .btn-red   { background: #c0392b; color: #fff; }
  .btn-dark  { background: #2a3347; color: var(--text); }
  .btn-scene { background: #1a2a1a; color: #6fcf6f; border:1px solid #2d5a2d; }
  .btn-scene.active { background:#2d5a2d; }

  /* Scenes */
  .scenes-title {
    font-size:.85rem;
    color:var(--muted);
    letter-spacing:.1em;
    text-transform:uppercase;
    margin-bottom:10px;
  }
  .scenes-grid {
    display:grid;
    grid-template-columns:repeat(auto-fill,minmax(200px,1fr));
    gap:10px;
    margin-bottom:24px;
  }
  .scene-card {
    background:var(--card);
    border-radius:8px;
    border:1px solid #2a3347;
    padding:12px;
  }
  .scene-card input {
    width:100%;
    background:transparent;
    border:none;
    border-bottom:1px dashed var(--muted);
    color:var(--text);
    font-size:.9rem;
    outline:none;
    margin-bottom:10px;
    padding:2px 4px;
  }
  .scene-card input:focus { border-bottom-color:#6fcf6f; }
  .scene-btns { display:flex; gap:6px; flex-wrap:wrap; }
  .scene-btns button { padding:6px 12px; font-size:.78rem; }

  .status-bar {
    text-align:center;
    font-size:.78rem;
    color:var(--muted);
    margin-top:10px;
    min-height:1.2em;
  }
  .ok  { color:#6fcf6f; }
  .err { color:#eb5757; }
</style>
</head>
<body>

<h1>&#9670; Smart Maquete</h1>
<p class="sub">Controlador de Iluminação — 8 Canais PWM</p>

<!-- Master -->
<div class="master-card">
  <label>MASTER DIMMER <span class="val" id="masterPct">100%</span></label>
  <br><br>
  <input type="range" min="0" max="1023" id="masterSlider" oninput="onMaster(this.value)">
</div>

<!-- Quick actions -->
<div class="btn-row">
  <button class="btn-gold" onclick="allOn()">&#9679; Ligar Tudo</button>
  <button class="btn-red"  onclick="allOff()">&#9675; Desligar Tudo</button>
</div>

<!-- Channels -->
<div class="grid" id="grid"></div>

<!-- Scenes -->
<div class="scenes-title">&#9670; CENAS SALVAS</div>
<div class="scenes-grid" id="scenesGrid"></div>

<div class="status-bar" id="status"></div>

<script>
const MAX = 1023;
let state = null;

async function api(path, params) {
  const body = new URLSearchParams(params);
  try {
    const r = await fetch('/api/' + path, { method:'POST', body });
    return r.ok;
  } catch(e) { return false; }
}

function pct(v) { return Math.round(v / MAX * 100) + '%'; }
function status(msg, ok) {
  const el = document.getElementById('status');
  el.textContent = msg;
  el.className = 'status-bar ' + (ok === false ? 'err' : 'ok');
  setTimeout(() => { el.textContent = ''; el.className = 'status-bar'; }, 2000);
}

async function loadState() {
  try {
    const r = await fetch('/api/state');
    state = await r.json();
    render();
  } catch(e) { status('Erro ao carregar estado', false); }
}

function render() {
  // Master
  document.getElementById('masterSlider').value = state.master;
  document.getElementById('masterPct').textContent = pct(state.master);

  // Channels
  const grid = document.getElementById('grid');
  grid.innerHTML = '';
  state.channels.forEach(ch => {
    const div = document.createElement('div');
    div.className = 'ch-card';
    div.innerHTML = `
      <div class="ch-header">
        <input class="ch-name" id="name${ch.id}" value="${ch.name}"
               onblur="renameChannel(${ch.id})" onkeydown="if(event.key==='Enter')this.blur()">
        <span class="ch-pct" id="pct${ch.id}">${pct(ch.value)}</span>
      </div>
      <input type="range" min="0" max="1023" value="${ch.value}"
             oninput="onChannel(${ch.id}, this.value)">
    `;
    grid.appendChild(div);
  });

  // Scenes
  const sg = document.getElementById('scenesGrid');
  sg.innerHTML = '';
  state.scenes.forEach(sc => {
    const div = document.createElement('div');
    div.className = 'scene-card';
    div.innerHTML = `
      <input id="scName${sc.id}" value="${sc.name || ('Cena ' + (sc.id+1))}" placeholder="Nome da cena">
      <div class="scene-btns">
        <button class="btn-gold" onclick="sceneSave(${sc.id})">Salvar</button>
        ${sc.used ? `<button class="btn-scene" onclick="sceneLoad(${sc.id})">Carregar</button>` : ''}
        ${sc.used ? `<button class="btn-red" style="font-size:.72rem;padding:5px 8px" onclick="sceneDel(${sc.id})">&#10005;</button>` : ''}
      </div>
    `;
    sg.appendChild(div);
  });
}

// ── Handlers ──────────────────────────────────────────────────

let masterTimer = null;
function onMaster(v) {
  document.getElementById('masterPct').textContent = pct(v);
  clearTimeout(masterTimer);
  masterTimer = setTimeout(async () => {
    const ok = await api('master', { value: v });
    status(ok ? 'Master atualizado' : 'Erro master', ok || false);
  }, 120);
}

const chTimers = {};
function onChannel(id, v) {
  document.getElementById('pct' + id).textContent = pct(v);
  clearTimeout(chTimers[id]);
  chTimers[id] = setTimeout(async () => {
    const ok = await api('channel', { id, value: v });
    if (!ok) status('Erro canal ' + id, false);
  }, 120);
}

async function renameChannel(id) {
  const name = document.getElementById('name' + id).value.trim();
  if (!name) return;
  const ok = await api('rename', { id, name });
  status(ok ? 'Nome salvo' : 'Erro ao renomear', ok || false);
}

async function allOn()  { const ok = await api('allon',  {}); if(ok){state.channels.forEach(c=>c.value=MAX);render();} }
async function allOff() { const ok = await api('alloff', {}); if(ok){state.channels.forEach(c=>c.value=0);render();} }

async function sceneSave(id) {
  const name = document.getElementById('scName' + id).value.trim() || ('Cena ' + (id+1));
  const ok = await api('scene/save', { id, name });
  status(ok ? '\"' + name + '\" salva' : 'Erro ao salvar cena', ok || false);
  if (ok) loadState();
}

async function sceneLoad(id) {
  const ok = await api('scene/load', { id });
  status(ok ? 'Cena carregada' : 'Erro ao carregar cena', ok || false);
  if (ok) loadState();
}

async function sceneDel(id) {
  const ok = await api('scene/delete', { id });
  status(ok ? 'Cena removida' : 'Erro ao remover', ok || false);
  if (ok) loadState();
}

// ── Init ───────────────────────────────────────────────────────
loadState();
</script>
</body>
</html>
)rawhtml";
  server.send(200, "text/html", html);
}

// ─── SETUP ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Smart Maquete v1.0 ===");

  // PWM
  for (int i = 0; i < NUM_CHANNELS; i++) {
    ledcAttach(CHANNEL_PINS[i], PWM_FREQ, PWM_RES);
    ledcWrite(CHANNEL_PINS[i], 0);
  }

  // Persistência
  loadChannels();
  loadScenes();
  applyAll();

  // Wi-Fi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255,255,255,0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Rotas
  server.on("/",                 HTTP_GET,  handleRoot);
  server.on("/api/state",        HTTP_GET,  handleGetState);
  server.on("/api/channel",      HTTP_POST, handleSetChannel);
  server.on("/api/master",       HTTP_POST, handleSetMaster);
  server.on("/api/rename",       HTTP_POST, handleRename);
  server.on("/api/scene/save",   HTTP_POST, handleSceneSave);
  server.on("/api/scene/load",   HTTP_POST, handleSceneLoad);
  server.on("/api/scene/delete", HTTP_POST, handleSceneDelete);
  server.on("/api/allon",        HTTP_POST, handleAllOn);
  server.on("/api/alloff",       HTTP_POST, handleAllOff);
  server.begin();

  Serial.println("Servidor HTTP iniciado.");
  Serial.println("Acesse: http://192.168.4.1");
}

// ─── LOOP ────────────────────────────────────────────────────
void loop() {
  server.handleClient();
}
