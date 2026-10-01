'use strict';

const SOURCE_PATH = '/source.glb';
const DESTINATION_PATH = '/destination.glb';
const ERROR_NAMES = ['ok', 'bad arguments', 'cannot open input', 'malformed GLB', 'not found',
  'cannot write output', 'internal error'];
const MAP_PRESETS = [
  { file: 'ual-to-mixamo.map', label: 'UAL (UE5 Mannequin) → Mixamo' },
  { file: 'mixamo-identity.map', label: 'Mixamo → Mixamo (identity)' },
];

const ui = {};
for (const element of document.querySelectorAll('[id]')) {
  ui[element.id.replace(/-([a-z])/g, (_, letter) => letter.toUpperCase())] = element;
}

let Module = null;
let sourceLoaded = false;
let destinationLoaded = false;
let configureTimer = null;
let previewIndex = -1;
let scrubbing = false;

function isEditable(target) {
  return target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA'
    || target.tagName === 'SELECT' || target.isContentEditable);
}

for (const type of ['keydown', 'keypress', 'keyup']) {
  window.addEventListener(type, (event) => {
    if (isEditable(event.target)) {
      event.stopImmediatePropagation();
    }
  }, true);
}

ui.canvas.addEventListener('contextmenu', (event) => event.preventDefault());

function setStatus(text, isError) {
  ui.status.textContent = text;
  ui.status.classList.toggle('error', Boolean(isError));
}

function errorName(code) {
  return ERROR_NAMES[code] || `error ${code}`;
}

function readFileBytes(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(new Uint8Array(reader.result));
    reader.onerror = () => reject(reader.error);
    reader.readAsArrayBuffer(file);
  });
}

function readFileText(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result);
    reader.onerror = () => reject(reader.error);
    reader.readAsText(file);
  });
}

function selectedIndices() {
  return [...ui.animationList.querySelectorAll('input[type=checkbox]:checked')]
    .map((box) => Number(box.dataset.index));
}

function updateButtons() {
  const ready = Module && sourceLoaded && destinationLoaded && Module._web_is_ready() === 1;
  const count = Module ? Module._web_animation_count() : 0;
  ui.exportAll.disabled = !ready || count === 0;
  ui.exportSelected.disabled = !ready || selectedIndices().length === 0;
  ui.alignment.textContent = ready ? Module.UTF8ToString(Module._web_alignment_text()) : '';
}

function renderAnimationList() {
  const count = Module._web_animation_count();
  ui.animationList.textContent = '';
  ui.animationCount.textContent = String(count);
  ui.selectAll.checked = false;
  if (count === 0) {
    const empty = document.createElement('li');
    empty.className = 'empty';
    empty.textContent = sourceLoaded ? 'The source GLB has no animations.' : 'Open a source GLB to list its animations.';
    ui.animationList.appendChild(empty);
    return;
  }
  for (let index = 0; index < count; index++) {
    const item = document.createElement('li');
    const label = document.createElement('label');
    const box = document.createElement('input');
    const name = document.createElement('span');
    const duration = document.createElement('span');
    box.type = 'checkbox';
    box.dataset.index = String(index);
    box.addEventListener('change', updateButtons);
    name.className = 'name';
    name.textContent = Module.UTF8ToString(Module._web_animation_name(index));
    name.addEventListener('click', (event) => {
      event.preventDefault();
      preview(index);
    });
    duration.className = 'duration';
    duration.textContent = `${Module._web_animation_duration(index).toFixed(2)} s`;
    label.append(box, name, duration);
    item.appendChild(label);
    ui.animationList.appendChild(item);
  }
}

function configure() {
  if (!Module || !sourceLoaded || !destinationLoaded) {
    updateButtons();
    return;
  }
  const fps = Number(ui.optFps.value) > 0 ? Number(ui.optFps.value) : 0;
  const code = Module.ccall('web_configure', 'number',
    ['string', 'number', 'number', 'number', 'number'],
    [ui.mapText.value, ui.optFrameAlign.checked ? 1 : 0, ui.optRestAlign.checked ? 1 : 0,
      ui.optInPlace.checked ? 1 : 0, fps]);
  if (code === 0) {
    setStatus('ready');
  }
  updateButtons();
  refreshPreview();
}

function scheduleConfigure() {
  clearTimeout(configureTimer);
  configureTimer = setTimeout(configure, 250);
}

async function loadGlb(file, path, exportName) {
  const bytes = await readFileBytes(file);
  Module.FS.writeFile(path, bytes);
  const code = Module.ccall(exportName, 'number', ['string'], [path]);
  if (code !== 0) {
    setStatus(`${file.name}: ${errorName(code)}`, true);
  }
  return code === 0;
}

async function openSource(file) {
  setStatus(`loading ${file.name}…`);
  previewIndex = -1;
  sourceLoaded = await loadGlb(file, SOURCE_PATH, 'web_load_source');
  ui.sourceName.textContent = sourceLoaded ? file.name : 'no source GLB';
  renderAnimationList();
  if (sourceLoaded) {
    setStatus(`${file.name}: ${Module._web_animation_count()} animations`);
  }
  configure();
}

async function openDestination(file) {
  setStatus(`loading ${file.name}…`);
  destinationLoaded = await loadGlb(file, DESTINATION_PATH, 'web_load_destination');
  ui.destinationName.textContent = destinationLoaded ? file.name : 'no destination GLB';
  if (destinationLoaded) {
    setStatus(`${file.name} loaded`);
  }
  configure();
}

function markPreviewRow() {
  for (const item of ui.animationList.querySelectorAll('li')) {
    const box = item.querySelector('input[type=checkbox]');
    item.classList.toggle('active', Boolean(box) && Number(box.dataset.index) === previewIndex);
  }
}

function updatePlaybackControls() {
  const count = Module ? Module._web_frame_count() : 0;
  ui.play.disabled = count === 0;
  ui.frame.disabled = count === 0;
  ui.frame.max = String(Math.max(count - 1, 0));
  if (count === 0) {
    ui.frameLabel.textContent = 'no preview';
    ui.play.textContent = 'Play';
    return;
  }
  ui.play.textContent = Module._web_is_playing() ? 'Pause' : 'Play';
  if (!scrubbing) {
    ui.frame.value = String(Module._web_frame());
  }
  ui.frameLabel.textContent = `frame ${ui.frame.value} / ${count - 1}`;
}

function preview(index) {
  if (!Module || !Module._web_is_ready()) {
    setStatus('load both files and a valid map before previewing', true);
    return;
  }
  const frames = Module._web_preview(index);
  previewIndex = frames > 0 ? index : -1;
  if (frames < 0) {
    setStatus(`preview failed: ${errorName(-frames)}`, true);
  } else {
    setStatus(`previewing ${Module.UTF8ToString(Module._web_animation_name(index))} (${frames} frames)`);
  }
  markPreviewRow();
  updatePlaybackControls();
}

function refreshPreview() {
  if (!Module) {
    return;
  }
  if (previewIndex >= 0 && previewIndex < Module._web_animation_count() && Module._web_is_ready()) {
    Module._web_refresh_preview();
  } else {
    previewIndex = -1;
    Module._web_refresh_preview();
  }
  markPreviewRow();
  updatePlaybackControls();
}

function tickPlayback() {
  updatePlaybackControls();
  requestAnimationFrame(tickPlayback);
}

function fileNameFor(index) {
  const pointer = Module._web_animation_file_name(index);
  const name = Module.UTF8ToString(pointer);
  Module._web_free(pointer);
  return name;
}

function buildGlb(index) {
  const size = Module._web_build_glb(index);
  if (size < 0) {
    throw new Error(`${fileNameFor(index)}: ${errorName(-size)}`);
  }
  const start = Module._web_glb_data();
  const bytes = Module.HEAPU8.slice(start, start + size);
  Module._web_glb_release();
  return bytes;
}

function downloadBytes(name, bytes) {
  const url = URL.createObjectURL(new Blob([bytes], { type: 'model/gltf-binary' }));
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = name;
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}

async function pickDirectory(count) {
  if (!window.showDirectoryPicker || count < 2) {
    return null;
  }
  try {
    return await window.showDirectoryPicker({ mode: 'readwrite' });
  } catch (error) {
    return error.name === 'AbortError' ? undefined : null;
  }
}

async function saveToDirectory(directory, name, bytes) {
  const handle = await directory.getFileHandle(name, { create: true });
  const writable = await handle.createWritable();
  await writable.write(bytes);
  await writable.close();
}

async function exportTracks(indices) {
  const directory = await pickDirectory(indices.length);
  if (directory === undefined) {
    return;
  }
  let written = 0;
  try {
    for (const index of indices) {
      const name = fileNameFor(index);
      setStatus(`converting ${name}…`);
      const bytes = buildGlb(index);
      if (directory) {
        await saveToDirectory(directory, name, bytes);
      } else {
        downloadBytes(name, bytes);
        await new Promise((resolve) => setTimeout(resolve, 150));
      }
      written++;
    }
    setStatus(`exported ${written} file${written === 1 ? '' : 's'}`);
  } catch (error) {
    setStatus(`export failed after ${written} file(s): ${error.message}`, true);
  }
}

async function loadMapPreset(file) {
  const response = await fetch(`mappings/${file}`);
  ui.mapText.value = response.ok ? await response.text() : '';
  scheduleConfigure();
}

function resizeCanvas() {
  if (Module) {
    Module._web_resize(ui.canvasBox.clientWidth, ui.canvasBox.clientHeight);
  }
}

function wireEvents() {
  ui.sourceFile.addEventListener('change', () => {
    if (ui.sourceFile.files[0]) {
      openSource(ui.sourceFile.files[0]);
    }
  });
  ui.destinationFile.addEventListener('change', () => {
    if (ui.destinationFile.files[0]) {
      openDestination(ui.destinationFile.files[0]);
    }
  });
  ui.selectAll.addEventListener('change', () => {
    for (const box of ui.animationList.querySelectorAll('input[type=checkbox]')) {
      box.checked = ui.selectAll.checked;
    }
    updateButtons();
  });
  ui.exportSelected.addEventListener('click', () => exportTracks(selectedIndices()));
  ui.exportAll.addEventListener('click', () => {
    exportTracks([...Array(Module._web_animation_count()).keys()]);
  });
  ui.mapPreset.addEventListener('change', () => loadMapPreset(ui.mapPreset.value));
  ui.mapFile.addEventListener('change', async () => {
    if (ui.mapFile.files[0]) {
      ui.mapText.value = await readFileText(ui.mapFile.files[0]);
      scheduleConfigure();
    }
  });
  ui.mapText.addEventListener('input', scheduleConfigure);
  for (const option of [ui.optInPlace, ui.optFrameAlign, ui.optRestAlign, ui.optFps]) {
    option.addEventListener('change', scheduleConfigure);
  }
  ui.showBones.addEventListener('change', () => Module._web_set_show_bones(ui.showBones.checked ? 1 : 0));
  ui.play.addEventListener('click', () => {
    Module._web_set_playing(Module._web_is_playing() ? 0 : 1);
    updatePlaybackControls();
  });
  ui.frame.addEventListener('pointerdown', () => { scrubbing = true; });
  ui.frame.addEventListener('pointerup', () => { scrubbing = false; });
  ui.frame.addEventListener('input', () => {
    Module._web_set_playing(0);
    Module._web_set_frame(Number(ui.frame.value));
    updatePlaybackControls();
  });
  ui.resetCamera.addEventListener('click', () => Module._web_reset_camera());
  new ResizeObserver(resizeCanvas).observe(ui.canvasBox);
}

function fillPresets() {
  for (const preset of MAP_PRESETS) {
    const option = document.createElement('option');
    option.value = preset.file;
    option.textContent = preset.label;
    ui.mapPreset.appendChild(option);
  }
}

async function start() {
  fillPresets();
  renderAnimationListPlaceholder();
  Module = await createAnimRetarget({
    canvas: ui.canvas,
    print: (line) => console.log(line),
    printErr: (line) => {
      console.error(line);
      setStatus(line, /error|warning/i.test(line));
    },
  });
  wireEvents();
  resizeCanvas();
  tickPlayback();
  await loadMapPreset(MAP_PRESETS[0].file);
  setStatus('open a source GLB and a destination GLB');
}

function renderAnimationListPlaceholder() {
  const empty = document.createElement('li');
  empty.className = 'empty';
  empty.textContent = 'Open a source GLB to list its animations.';
  ui.animationList.appendChild(empty);
}

start().catch((error) => setStatus(`failed to start: ${error.message}`, true));
