// Team-Tree remote control: publishes the retained state message that all
// trees follow and shows which trees are online.
'use strict';

const CONFIG = {
  url: 'wss://g18b7118.ala.eu-central-1.emqxsl.com:8084/mqtt',
  username: 'remote',
  topicState: 'team-tree/state',
  topicStatusPrefix: 'team-tree/status/',
  numLeds: 12,
  sendDelayMs: 300,
  // Disconnect when the app is in the background so idle tabs do not eat
  // into the broker's free session minutes.
  backgroundDisconnectMs: 15000,
};

const MODES = [
  { id: 'off', label: 'Aus', colors: false, speed: false },
  { id: 'solid', label: 'Statisch', colors: true, speed: false, hint: 'Die Farben werden reihum auf die LEDs verteilt.' },
  { id: 'breathe', label: 'Atmen', colors: true, speed: true, hint: 'Pulsiert langsam und wechselt dabei durch die Farben.' },
  { id: 'fade', label: 'Wechsel', colors: true, speed: true, hint: 'Blendet weich von Farbe zu Farbe über.' },
  { id: 'rainbow', label: 'Regenbogen', colors: false, speed: true },
  { id: 'chase', label: 'Lauflicht', colors: true, speed: true, hint: 'Erste Farbe = Hintergrund, die weiteren laufen nacheinander um den Baum.' },
  { id: 'twinkle', label: 'Funkeln', colors: true, speed: true, hint: 'Grundfarben mit Funkeln. Bei einer Farbe funkelt es warmweiß.' },
];

const PRESETS = [
  { label: 'Weihnachten', state: { mode: 'twinkle', colors: ['#ff0000', '#00c020', '#ffb060'], speed: 5 } },
  { label: 'Kerzenschein', state: { mode: 'twinkle', colors: ['#ff8a20'], speed: 3 } },
  { label: 'Winter', state: { mode: 'twinkle', colors: ['#2050ff', '#ffffff', '#80c0ff'], speed: 4 } },
  { label: 'Goldlauf', state: { mode: 'chase', colors: ['#003000', '#ff0000', '#ffd700'], speed: 6 } },
  { label: 'Kamin', state: { mode: 'fade', colors: ['#ff3000', '#ff8000', '#c01000'], speed: 6 } },
  { label: 'Regenbogen', state: { mode: 'rainbow', colors: ['#ffffff'], speed: 5 } },
  { label: 'Ruhepuls', state: { mode: 'breathe', colors: ['#ffb060', '#ff4060'], speed: 3 } },
];

const DEFAULT_STATE = { mode: 'twinkle', colors: ['#ffb060', '#ff0000', '#00ff20'], speed: 5, brightness: 60 };
const MAX_COLORS = 6;

const $ = (id) => document.getElementById(id);

const store = {
  get(key) { try { return localStorage.getItem(key); } catch { return null; } },
  set(key, value) { try { localStorage.setItem(key, value); } catch { /* private mode */ } },
  remove(key) { try { localStorage.removeItem(key); } catch { /* private mode */ } },
};

let state = { ...DEFAULT_STATE, colors: [...DEFAULT_STATE.colors] };
let lastBy = null;
let lastTs = null;
let client = null;
let dirty = false;          // local change not yet published
let sendTimer = null;
let hideTimer = null;
const trees = new Map();    // id -> status object

// ---- Rendering --------------------------------------------------------------

function modeInfo(id) { return MODES.find((m) => m.id === id) || MODES[MODES.length - 1]; }

function renderModes() {
  $('modes').replaceChildren(...MODES.map((m) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'mode';
    b.setAttribute('role', 'radio');
    b.textContent = m.label;
    b.setAttribute('aria-checked', String(state.mode === m.id));
    b.addEventListener('click', () => update({ mode: m.id }));
    return b;
  }));
}

function renderColors() {
  const info = modeInfo(state.mode);
  $('colorCard').classList.toggle('disabled', !info.colors);
  $('colorHint').textContent = info.colors ? info.hint || '' : 'Dieser Effekt verwendet keine eigenen Farben.';

  const items = state.colors.map((hex, idx) => {
    const wrap = document.createElement('div');
    wrap.className = 'swatch';
    wrap.style.setProperty('--c', hex);

    const label = document.createElement('label');
    label.title = `Farbe ${idx + 1} ändern`;
    const input = document.createElement('input');
    input.type = 'color';
    input.value = hex;
    input.setAttribute('aria-label', `Farbe ${idx + 1}`);
    input.addEventListener('input', () => {
      wrap.style.setProperty('--c', input.value);
      const colors = [...state.colors];
      colors[idx] = input.value;
      update({ colors }, { rerender: false });
    });
    label.append(input);
    wrap.append(label);

    if (state.colors.length > 1) {
      const rm = document.createElement('button');
      rm.type = 'button';
      rm.className = 'remove';
      rm.textContent = '×';
      rm.setAttribute('aria-label', `Farbe ${idx + 1} entfernen`);
      rm.addEventListener('click', () => update({ colors: state.colors.filter((_, k) => k !== idx) }));
      wrap.append(rm);
    }
    return wrap;
  });

  if (state.colors.length < MAX_COLORS) {
    const add = document.createElement('button');
    add.type = 'button';
    add.className = 'add';
    add.textContent = '+';
    add.setAttribute('aria-label', 'Farbe hinzufügen');
    add.addEventListener('click', () => update({ colors: [...state.colors, '#ffffff'] }));
    items.push(add);
  }
  $('colors').replaceChildren(...items);
}

function renderSliders() {
  $('speed').value = state.speed;
  $('speedOut').textContent = state.speed;
  $('brightness').value = state.brightness;
  $('brightnessOut').textContent = `${state.brightness} %`;
  const speedOn = modeInfo(state.mode).speed;
  $('speed').disabled = !speedOn;
  $('speedRow').classList.toggle('disabled', !speedOn);
}

function renderPresets() {
  $('presets').replaceChildren(...PRESETS.map((p) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'preset';
    const sw = document.createElement('span');
    sw.className = 'sw';
    const dots = p.state.mode === 'rainbow' ? ['#ff0000', '#ffd000', '#00c0ff'] : p.state.colors;
    for (const c of dots) {
      const i = document.createElement('i');
      i.style.background = c;
      sw.append(i);
    }
    b.append(sw, document.createTextNode(p.label));
    b.addEventListener('click', () => update({ ...p.state, colors: [...p.state.colors] }));
    return b;
  }));
}

function relTime(tsSec) {
  const diff = Math.max(0, Date.now() / 1000 - tsSec);
  if (diff < 60) return 'gerade eben';
  if (diff < 3600) return `vor ${Math.floor(diff / 60)} Min.`;
  if (diff < 86400) return `vor ${Math.floor(diff / 3600)} Std.`;
  return `vor ${Math.floor(diff / 86400)} Tagen`;
}

function renderLastChange() {
  const parts = [];
  if (lastBy) parts.push(`Zuletzt geändert von ${lastBy}`);
  if (lastTs) parts.push(relTime(lastTs));
  $('lastChange').textContent = parts.join(' · ') || ' ';
}

function renderTrees() {
  const list = [...trees.entries()]
    .map(([id, s]) => ({ id, ...s }))
    .sort((a, b) => (b.online - a.online) || (a.name || a.id).localeCompare(b.name || b.id, 'de'));
  const online = list.filter((t) => t.online).length;
  $('treeCount').textContent = list.length ? `· ${online} von ${list.length} online` : '';

  if (!list.length) {
    const li = document.createElement('li');
    li.className = 'empty';
    li.textContent = 'Noch keine Statusmeldungen empfangen.';
    $('trees').replaceChildren(li);
    return;
  }
  const nameCount = new Map();
  for (const t of list) nameCount.set(t.name || t.id, (nameCount.get(t.name || t.id) || 0) + 1);
  $('trees').replaceChildren(...list.map((t) => {
    const li = document.createElement('li');
    li.className = t.online ? 'online' : 'offline';
    li.title = t.id;
    const dot = document.createElement('span');
    dot.className = 'dot';
    const name = document.createElement('span');
    name.className = 'name';
    const label = t.name || t.id;
    // Same name twice (e.g. a stale entry): add the ID tail to tell them apart.
    name.textContent = nameCount.get(label) > 1 && t.name ? `${label} (…${t.id.slice(-4)})` : label;
    const meta = document.createElement('span');
    meta.className = 'meta';
    meta.textContent = t.online
      ? [t.rssi != null ? `${t.rssi} dBm` : null, t.fw ? `v${t.fw}` : null].filter(Boolean).join(' · ')
      : 'offline';
    li.append(dot, name, meta);
    return li;
  }));
}

function renderAll() {
  renderModes();
  renderColors();
  renderSliders();
  renderLastChange();
}

// ---- Tree preview -------------------------------------------------------------

const canvas = $('tree');
const ctx = canvas.getContext('2d');
const W = 240;
const H = 300;

// LEDs spiral up the tree from the bottom (index 0) to the top.
const LED_XY = Array.from({ length: CONFIG.numLeds }, (_, i) => {
  const t = i / (CONFIG.numLeds - 1);
  const angle = t * 2.6 * 2 * Math.PI + 0.6;
  const halfWidth = 78 * (1 - t) + 10;
  return {
    x: W / 2 + Math.sin(angle) * halfWidth * 0.85,
    y: 250 - t * 190,
    depth: (Math.cos(angle) + 1) / 2,  // 1 = front, 0 = back
  };
});
// Draw order: back to front.
const LED_ORDER = LED_XY.map((_, i) => i).sort((a, b) => LED_XY[a].depth - LED_XY[b].depth);

function setupCanvas() {
  const dpr = Math.min(window.devicePixelRatio || 1, 3);
  canvas.width = W * dpr;
  canvas.height = H * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

function treePath() {
  // Three stacked tiers, like a classic printed tree.
  const p = new Path2D();
  const tiers = [
    { top: 30, bottom: 120, half: 52 },
    { top: 80, bottom: 190, half: 78 },
    { top: 140, bottom: 262, half: 104 },
  ];
  for (const t of tiers) {
    p.moveTo(W / 2, t.top);
    p.quadraticCurveTo(W / 2 + t.half * 0.45, (t.top + t.bottom) / 2, W / 2 + t.half, t.bottom);
    p.quadraticCurveTo(W / 2, t.bottom + 10, W / 2 - t.half, t.bottom);
    p.quadraticCurveTo(W / 2 - t.half * 0.45, (t.top + t.bottom) / 2, W / 2, t.top);
  }
  return p;
}
const TREE = treePath();

function drawFrame(now) {
  const leds = TreeEffects.render(state, CONFIG.numLeds, now);
  const b = state.brightness;
  const gain = b === 0 ? 0 : 0.3 + 0.7 * (b / 100);
  const lit = leds.map((c) => c.map((x) => Math.round(x * gain)));

  // Average light tints the white "printed" body, like diffused LED light.
  const avg = [0, 1, 2].map((k) => lit.reduce((s, c) => s + c[k], 0) / lit.length);

  ctx.clearRect(0, 0, W, H);

  // Trunk
  ctx.fillStyle = '#2a3346';
  ctx.fillRect(W / 2 - 12, 262, 24, 22);

  // Body
  ctx.save();
  ctx.fillStyle = '#dfe5ef';
  ctx.globalAlpha = 0.16;
  ctx.fill(TREE);
  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = 'lighter';
  ctx.fillStyle = `rgba(${avg[0] | 0},${avg[1] | 0},${avg[2] | 0},0.35)`;
  ctx.fill(TREE);
  ctx.restore();

  // LEDs, back to front, additive glow
  ctx.save();
  ctx.globalCompositeOperation = 'lighter';
  for (const i of LED_ORDER) {
    const { x, y, depth } = LED_XY[i];
    const [r, g, bl] = lit[i];
    if (r + g + bl === 0) continue;
    const size = 0.6 + 0.4 * depth;
    const alpha = 0.45 + 0.55 * depth;
    const radius = 26 * size;
    const grad = ctx.createRadialGradient(x, y, 0, x, y, radius);
    grad.addColorStop(0, `rgba(${r},${g},${bl},${alpha})`);
    grad.addColorStop(0.25, `rgba(${r},${g},${bl},${alpha * 0.45})`);
    grad.addColorStop(1, `rgba(${r},${g},${bl},0)`);
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = `rgba(${Math.min(255, r + 90)},${Math.min(255, g + 90)},${Math.min(255, bl + 90)},${alpha})`;
    ctx.beginPath();
    ctx.arc(x, y, 3.2 * size, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();

  // Star
  ctx.save();
  ctx.translate(W / 2, 24);
  ctx.beginPath();
  for (let k = 0; k < 10; k++) {
    const rad = k % 2 ? 4.5 : 11;
    const a = (k * Math.PI) / 5 - Math.PI / 2;
    ctx.lineTo(Math.cos(a) * rad, Math.sin(a) * rad);
  }
  ctx.closePath();
  ctx.fillStyle = '#f2c66d';
  ctx.shadowColor = '#f2c66d';
  ctx.shadowBlur = 12;
  ctx.fill();
  ctx.restore();
}

let lastFrame = 0;
function loop(t) {
  requestAnimationFrame(loop);
  if (document.hidden || t - lastFrame < 33) return;  // ~30 fps is plenty
  lastFrame = t;
  drawFrame(Date.now());
}

// ---- State changes -------------------------------------------------------------

function update(patch, { rerender = true } = {}) {
  state = { ...state, ...patch };
  if (rerender) renderAll();
  else renderSliders();
  dirty = true;
  clearTimeout(sendTimer);
  sendTimer = setTimeout(send, CONFIG.sendDelayMs);
}

function send() {
  clearTimeout(sendTimer);
  if (!dirty) return;
  if (!client || !client.connected) {
    toast('Nicht verbunden – Änderung nicht gesendet');
    return;
  }
  const name = store.get('tt-name') || '';
  const ts = Math.floor(Date.now() / 1000);
  const payload = {
    mode: state.mode,
    colors: state.colors,
    speed: state.speed,
    brightness: state.brightness,
    by: name,
    ts,
  };
  client.publish(CONFIG.topicState, JSON.stringify(payload), { qos: 1, retain: true }, (err) => {
    if (err) toast('Senden fehlgeschlagen');
  });
  dirty = false;
  lastBy = name;
  lastTs = ts;
  renderLastChange();
}

function applyRemoteState(json) {
  let s;
  try { s = JSON.parse(json); } catch { return; }
  if (dirty) return;  // the user is editing; their change wins
  const colors = Array.isArray(s.colors)
    ? s.colors.filter((c) => /^#?[0-9a-f]{6}$/i.test(c)).map((c) => (c[0] === '#' ? c : `#${c}`).toLowerCase()).slice(0, MAX_COLORS)
    : [];
  const next = {
    mode: MODES.some((m) => m.id === s.mode) ? s.mode : 'twinkle',
    colors: colors.length ? colors : [...DEFAULT_STATE.colors],
    speed: Math.min(10, Math.max(1, Number(s.speed) || 5)),
    brightness: Math.min(100, Math.max(0, Number.isFinite(Number(s.brightness)) ? Number(s.brightness) : 60)),
  };
  lastBy = s.by || null;
  lastTs = Number(s.ts) || null;
  // Our own echo: skip re-rendering so an open colour picker is not destroyed.
  const same = JSON.stringify(next) === JSON.stringify({
    mode: state.mode, colors: state.colors, speed: state.speed, brightness: state.brightness,
  });
  state = next;
  if (same) renderLastChange();
  else renderAll();
}

function applyStatus(id, json) {
  if (!json) { trees.delete(id); renderTrees(); return; }  // retained status cleared
  let s;
  try { s = JSON.parse(json); } catch { return; }
  // Trees always send "online". A retained "{}" (the EMQX console cannot
  // publish empty payloads) marks a stale entry as removed.
  if (!s || typeof s !== 'object' || !('online' in s)) { trees.delete(id); renderTrees(); return; }
  trees.set(id, { ...trees.get(id), ...s, online: !!s.online });
  renderTrees();
}

// ---- MQTT -------------------------------------------------------------------------

function setConn(stateName, text) {
  $('conn').dataset.state = stateName;
  $('connText').textContent = text;
  $('app').setAttribute('aria-busy', String(stateName !== 'online'));
}

function connect() {
  const password = store.get('tt-pass');
  if (!password) { showLogin(); return; }
  if (client) client.end(true);

  setConn('connecting', 'Verbinde …');
  const c = mqtt.connect(CONFIG.url, {
    username: CONFIG.username,
    password,
    clientId: `remote-${Math.random().toString(16).slice(2, 10)}`,
    clean: true,
    keepalive: 60,
    reconnectPeriod: 5000,
    connectTimeout: 10000,
    protocolVersion: 4,
  });
  client = c;

  c.on('connect', () => {
    setConn('online', 'Verbunden');
    closeLogin();
    trees.clear();
    c.subscribe([CONFIG.topicState, `${CONFIG.topicStatusPrefix}+`], { qos: 1 });
  });
  c.on('reconnect', () => setConn('connecting', 'Verbinde …'));
  c.on('offline', () => setConn('offline', 'Offline'));
  c.on('close', () => { if (client === c && !c.connected) setConn('offline', 'Offline'); });
  c.on('error', (err) => {
    // 4 = bad username/password, 5 = not authorized (MQTT 3.1.1)
    if (err && (err.code === 4 || err.code === 5)) {
      c.end(true);
      client = null;
      store.remove('tt-pass');
      setConn('offline', 'Offline');
      showLogin('Das Passwort stimmt nicht.');
    }
  });
  c.on('message', (topic, payload) => {
    const text = payload.toString();
    if (topic === CONFIG.topicState) applyRemoteState(text);
    else if (topic.startsWith(CONFIG.topicStatusPrefix)) applyStatus(topic.slice(CONFIG.topicStatusPrefix.length), text);
  });
}

function disconnect() {
  if (dirty) send();
  if (client) client.end(false);
  client = null;
  setConn('offline', 'Pausiert');
}

// ---- Login / misc -------------------------------------------------------------------

function showLogin(error = '') {
  $('loginName').value = store.get('tt-name') || '';
  $('loginPass').value = '';
  $('loginError').textContent = error;
  if (!$('login').open) $('login').showModal();
  ($('loginName').value ? $('loginPass') : $('loginName')).focus();
}

function closeLogin() { if ($('login').open) $('login').close(); }

function renderWhoami() {
  const name = store.get('tt-name');
  $('whoami').textContent = name ? `Angemeldet als ${name}` : '';
}

let toastTimer = null;
function toast(text) {
  const el = $('toast');
  el.textContent = text;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 2200);
}

function init() {
  setupCanvas();
  renderPresets();
  renderAll();
  renderTrees();
  renderWhoami();
  requestAnimationFrame(loop);
  setInterval(renderLastChange, 30000);

  $('speed').addEventListener('input', (e) => update({ speed: Number(e.target.value) }, { rerender: false }));
  $('brightness').addEventListener('input', (e) => update({ brightness: Number(e.target.value) }, { rerender: false }));

  $('loginForm').addEventListener('submit', (e) => {
    e.preventDefault();
    const name = $('loginName').value.trim();
    const pass = $('loginPass').value;
    if (!name || !pass) return;
    store.set('tt-name', name);
    store.set('tt-pass', pass);
    renderWhoami();
    $('loginError').textContent = '';
    connect();
  });
  $('login').addEventListener('cancel', (e) => e.preventDefault());  // login is required

  $('logout').addEventListener('click', () => {
    disconnect();
    store.remove('tt-pass');
    showLogin();
  });
  $('conn').addEventListener('click', () => {
    if (!client || !client.connected) connect();
  });

  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      hideTimer = setTimeout(disconnect, CONFIG.backgroundDisconnectMs);
    } else {
      clearTimeout(hideTimer);
      if (!client && store.get('tt-pass')) connect();
    }
  });

  if (typeof mqtt === 'undefined') {
    setConn('offline', 'Offline');
    toast('MQTT-Bibliothek konnte nicht geladen werden');
    return;
  }
  connect();

  if ('serviceWorker' in navigator && location.protocol === 'https:') {
    navigator.serviceWorker.register('sw.js').catch(() => {});
  }
}

init();
