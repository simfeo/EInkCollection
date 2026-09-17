#pragma once

const char INDEX_HTML[] PROGMEM = R"EPAPERHTML(
<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#10130f">
  <title>E-Paper C3</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0d0f0c;
      --panel: #171a15;
      --panel-2: #1e221b;
      --line: #343b2f;
      --text: #f0f2eb;
      --muted: #aab1a1;
      --accent: #d4ff58;
      --accent-dark: #26310e;
      --ok: #78dc8b;
      --bad: #ff8585;
      --shadow: 0 24px 70px rgba(0,0,0,.35);
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      min-height: 100vh;
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: var(--text);
      background:
        radial-gradient(circle at 15% 0%, rgba(212,255,88,.10), transparent 30rem),
        linear-gradient(145deg, #10130f, var(--bg) 55%);
    }

    main {
      width: min(980px, calc(100% - 28px));
      margin: 0 auto;
      padding: 30px 0 42px;
    }

    header {
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 18px;
      margin-bottom: 22px;
    }

    .eyebrow {
      margin: 0 0 7px;
      color: var(--accent);
      font-size: 12px;
      font-weight: 800;
      letter-spacing: .14em;
      text-transform: uppercase;
    }

    h1 {
      margin: 0;
      font-size: clamp(29px, 6vw, 52px);
      line-height: .98;
      letter-spacing: -.045em;
    }

    .subtitle {
      max-width: 600px;
      margin: 12px 0 0;
      color: var(--muted);
      font-size: 15px;
      line-height: 1.55;
    }

    .connection {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      flex: 0 0 auto;
      padding: 9px 12px;
      border: 1px solid var(--line);
      border-radius: 999px;
      background: rgba(23,26,21,.84);
      color: var(--muted);
      font-size: 12px;
      white-space: nowrap;
    }

    .dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #70766c;
      box-shadow: 0 0 0 4px rgba(112,118,108,.12);
    }

    .dot.online {
      background: var(--ok);
      box-shadow: 0 0 0 4px rgba(120,220,139,.13);
    }

    .grid {
      display: grid;
      grid-template-columns: minmax(0, 1.35fr) minmax(290px, .65fr);
      gap: 18px;
      align-items: start;
    }

    .card {
      border: 1px solid var(--line);
      border-radius: 20px;
      background: rgba(23,26,21,.93);
      box-shadow: var(--shadow);
      overflow: hidden;
    }

    .card-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      padding: 16px 18px;
      border-bottom: 1px solid var(--line);
    }

    .card-head h2 {
      margin: 0;
      font-size: 15px;
      letter-spacing: -.01em;
    }

    .badge {
      padding: 5px 8px;
      border-radius: 7px;
      background: var(--panel-2);
      color: var(--muted);
      font: 700 11px/1 ui-monospace, SFMono-Regular, Menlo, monospace;
    }

    .screen-wrap {
      padding: clamp(16px, 4vw, 28px);
      background:
        linear-gradient(45deg, rgba(255,255,255,.025) 25%, transparent 25%) 0 0 / 18px 18px,
        linear-gradient(-45deg, rgba(255,255,255,.025) 25%, transparent 25%) 0 0 / 18px 18px,
        #11130f;
    }

    .bezel {
      padding: clamp(9px, 2.5vw, 15px);
      border: 1px solid #565b52;
      border-radius: 13px;
      background: linear-gradient(145deg, #e9ebe2, #aeb2a8);
      box-shadow: inset 0 0 0 3px #c9ccc2, 0 18px 36px rgba(0,0,0,.42);
    }

    canvas#preview {
      display: block;
      width: 100%;
      aspect-ratio: 296 / 128;
      border: 1px solid #74786e;
      background: #fff;
      image-rendering: pixelated;
    }

    .screen-foot {
      display: flex;
      justify-content: space-between;
      gap: 10px;
      padding: 13px 18px;
      color: var(--muted);
      font-size: 12px;
    }

    .controls { padding: 17px; }

    .file-picker {
      display: grid;
      place-items: center;
      min-height: 116px;
      padding: 16px;
      border: 1px dashed #59624e;
      border-radius: 14px;
      background: var(--panel-2);
      text-align: center;
      cursor: pointer;
      transition: .18s ease;
    }

    .file-picker:hover,
    .file-picker.drag {
      border-color: var(--accent);
      background: var(--accent-dark);
      transform: translateY(-1px);
    }

    .file-picker input { display: none; }
    .file-title { font-weight: 800; }
    .file-help { margin-top: 6px; color: var(--muted); font-size: 12px; line-height: 1.4; }
    .file-name { margin-top: 8px; color: var(--accent); font-size: 12px; overflow-wrap: anywhere; }

    .control-list {
      display: grid;
      gap: 14px;
      margin-top: 17px;
    }

    label.row,
    .range-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      color: var(--muted);
      font-size: 13px;
    }

    select {
      min-width: 144px;
      padding: 9px 30px 9px 10px;
      border: 1px solid var(--line);
      border-radius: 9px;
      color: var(--text);
      background: #10130f;
      font: inherit;
    }

    .check {
      appearance: none;
      width: 42px;
      height: 24px;
      margin: 0;
      border: 1px solid #4b5147;
      border-radius: 999px;
      background: #10130f;
      cursor: pointer;
      transition: .18s ease;
    }

    .check::after {
      content: "";
      display: block;
      width: 18px;
      height: 18px;
      margin: 2px;
      border-radius: 50%;
      background: #777e72;
      transition: .18s ease;
    }

    .check:checked { border-color: var(--accent); background: var(--accent-dark); }
    .check:checked::after { transform: translateX(18px); background: var(--accent); }

    input[type="range"] {
      width: 100%;
      margin: 8px 0 0;
      accent-color: var(--accent);
    }

    button {
      width: 100%;
      margin-top: 18px;
      padding: 13px 16px;
      border: 0;
      border-radius: 12px;
      color: #15180f;
      background: var(--accent);
      font: 850 14px/1.2 inherit;
      cursor: pointer;
      box-shadow: 0 8px 24px rgba(212,255,88,.13);
      transition: .18s ease;
    }

    button:hover:not(:disabled) { transform: translateY(-1px); filter: brightness(1.05); }
    button:disabled { cursor: not-allowed; opacity: .42; box-shadow: none; }

    .status {
      min-height: 42px;
      margin-top: 12px;
      padding: 10px 12px;
      border-radius: 10px;
      color: var(--muted);
      background: #11130f;
      font-size: 12px;
      line-height: 1.45;
    }

    .status.ok { color: var(--ok); }
    .status.bad { color: var(--bad); }

    .note {
      margin: 16px 2px 0;
      color: var(--muted);
      font-size: 12px;
      line-height: 1.55;
    }

    @media (max-width: 760px) {
      header { align-items: stretch; flex-direction: column; }
      .connection { align-self: flex-start; }
      .grid { grid-template-columns: 1fr; }
      main { padding-top: 20px; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <div>
        <p class="eyebrow">ESP32-C3 · 2.9″ E-Paper</p>
        <h1>Загрузчик экрана</h1>
        <p class="subtitle">Выберите изображение, настройте чёрно-белое преобразование и отправьте готовый кадр на дисплей.</p>
      </div>
      <div class="connection"><span class="dot" id="dot"></span><span id="connectionText">Проверка связи…</span></div>
    </header>

    <section class="grid">
      <article class="card">
        <div class="card-head">
          <h2>Предпросмотр результата</h2>
          <span class="badge">296 × 128 · 1 bit</span>
        </div>
        <div class="screen-wrap">
          <div class="bezel">
            <canvas id="preview" width="296" height="128" aria-label="Предпросмотр e-paper"></canvas>
          </div>
        </div>
        <div class="screen-foot">
          <span id="previewLabel">Изображение не выбрано</span>
          <span>4 736 байт</span>
        </div>
      </article>

      <aside class="card">
        <div class="card-head"><h2>Настройки кадра</h2></div>
        <div class="controls">
          <label class="file-picker" id="dropZone">
            <input id="file" type="file" accept="image/png,image/jpeg,image/webp,image/gif,image/bmp">
            <span>
              <span class="file-title">Выбрать картинку</span>
              <span class="file-help">JPEG, PNG, WebP, BMP или GIF<br>можно перетащить сюда</span>
              <span class="file-name" id="fileName"></span>
            </span>
          </label>

          <div class="control-list">
            <label class="row">
              <span>Масштабирование</span>
              <select id="fit">
                <option value="contain">Вписать целиком</option>
                <option value="cover">Заполнить экран</option>
              </select>
            </label>

            <label class="row">
              <span>Повернуть на 90°</span>
              <input class="check" id="rotate" type="checkbox">
            </label>

            <label class="row">
              <span>Дизеринг</span>
              <select id="dither">
                <option value="atkinson">Аткинсон — контрастный</option>
                <option value="fs">Флойд-Стейнберг — точный тон</option>
                <option value="sierra2">Сьерра-2</option>
                <option value="jjn">Джарвис — мягкий</option>
                <option value="bayer2">Байер 2×2 — грубая сетка</option>
                <option value="bayer4">Байер 4×4</option>
                <option value="bayer8">Байер 8×8 — мелкая сетка</option>
                <option value="none">Без дизеринга — порог</option>
              </select>
            </label>

            <label class="row">
              <span>Автоуровни</span>
              <input class="check" id="autoLevels" type="checkbox" checked>
            </label>

            <label class="row">
              <span>Инвертировать</span>
              <input class="check" id="invert" type="checkbox">
            </label>

            <div>
              <div class="range-head"><span>Яркость</span><strong id="brightnessValue">0</strong></div>
              <input id="brightness" type="range" min="-100" max="100" value="0">
            </div>

            <div>
              <div class="range-head"><span>Контраст</span><strong id="contrastValue">1.00</strong></div>
              <input id="contrast" type="range" min="50" max="200" value="100">
            </div>

            <div>
              <div class="range-head"><span>Резкость</span><strong id="sharpenValue">0.6</strong></div>
              <input id="sharpen" type="range" min="0" max="200" value="60">
            </div>

            <div id="thresholdRow" hidden>
              <div class="range-head"><span>Порог чёрного</span><strong id="thresholdValue">128</strong></div>
              <input id="threshold" type="range" min="20" max="235" value="128">
            </div>
          </div>

          <button id="upload" type="button" disabled>Показать на e-paper</button>
          <div class="status" id="status">Ожидаю изображение.</div>
        </div>
      </aside>
    </section>

    <p class="note">Обработка выполняется локально в браузере. В ESP32 передаётся только монохромный кадр. При обновлении e-paper несколько раз мигает — это нормальный полный цикл очистки. Анимированный GIF превращается в один статичный кадр.</p>
  </main>

  <canvas id="work" width="296" height="128" hidden></canvas>

  <script>
    (() => {
      "use strict";

      const WIDTH = 296;
      const HEIGHT = 128;
      const ROW_BYTES = 37;
      const BITMAP_BYTES = ROW_BYTES * HEIGHT;

      const byId = (id) => document.getElementById(id);
      const ui = {
        preview: byId("preview"),
        work: byId("work"),
        file: byId("file"),
        fileName: byId("fileName"),
        dropZone: byId("dropZone"),
        fit: byId("fit"),
        rotate: byId("rotate"),
        dither: byId("dither"),
        autoLevels: byId("autoLevels"),
        invert: byId("invert"),
        brightness: byId("brightness"),
        brightnessValue: byId("brightnessValue"),
        contrast: byId("contrast"),
        contrastValue: byId("contrastValue"),
        sharpen: byId("sharpen"),
        sharpenValue: byId("sharpenValue"),
        thresholdRow: byId("thresholdRow"),
        threshold: byId("threshold"),
        thresholdValue: byId("thresholdValue"),
        upload: byId("upload"),
        status: byId("status"),
        previewLabel: byId("previewLabel"),
        dot: byId("dot"),
        connectionText: byId("connectionText")
      };

      const previewContext = ui.preview.getContext("2d", { alpha: false });
      const workContext = ui.work.getContext("2d", { willReadFrequently: true });

      let sourceImage = null;
      let sourceUrl = "";
      let packedBitmap = null;
      let uploading = false;

      function setMessage(text, tone = "") {
        ui.status.textContent = text;
        ui.status.className = `status ${tone}`.trim();
      }

      function showConnection(online, clients = null) {
        ui.dot.classList.toggle("online", online);
        if (!online) {
          ui.connectionText.textContent = "Нет связи с ESP32";
        } else if (clients === null) {
          ui.connectionText.textContent = "ESP32 подключена";
        } else {
          ui.connectionText.textContent = `ESP32 подключена · клиентов: ${clients}`;
        }
      }

      function setUploading(value) {
        uploading = value;
        ui.upload.disabled = value || !packedBitmap;
        ui.upload.textContent = value ? "Отправляю…" : "Показать на e-paper";
      }

      function drawEmptyPreview() {
        previewContext.fillStyle = "#ffffff";
        previewContext.fillRect(0, 0, WIDTH, HEIGHT);
        previewContext.fillStyle = "#a6aaa2";
        previewContext.font = "12px system-ui, sans-serif";
        previewContext.textAlign = "center";
        previewContext.textBaseline = "middle";
        previewContext.fillText("Выберите изображение", WIDTH / 2, HEIGHT / 2);
      }

      function drawPackedBitmap(bytes) {
        if (bytes.length !== BITMAP_BYTES) {
          throw new Error("Некорректный размер сохранённого кадра");
        }

        const output = previewContext.createImageData(WIDTH, HEIGHT);
        for (let y = 0; y < HEIGHT; y++) {
          for (let x = 0; x < WIDTH; x++) {
            const black = (bytes[y * ROW_BYTES + (x >> 3)] & (0x80 >> (x & 7))) !== 0;
            const color = black ? 0 : 255;
            const pixel = (y * WIDTH + x) * 4;
            output.data[pixel] = color;
            output.data[pixel + 1] = color;
            output.data[pixel + 2] = color;
            output.data[pixel + 3] = 255;
          }
        }

        previewContext.putImageData(output, 0, 0);
        packedBitmap = new Uint8Array(bytes);
        ui.upload.disabled = uploading;
      }

      // Error diffusion kernels as [dx, dy, weight] plus a common divisor.
      const KERNELS = {
        fs: {
          divisor: 16,
          taps: [[1, 0, 7], [-1, 1, 3], [0, 1, 5], [1, 1, 1]]
        },
        atkinson: {
          divisor: 8,
          taps: [[1, 0, 1], [2, 0, 1], [-1, 1, 1], [0, 1, 1], [1, 1, 1], [0, 2, 1]]
        },
        sierra2: {
          divisor: 16,
          taps: [[1, 0, 4], [2, 0, 3], [-2, 1, 1], [-1, 1, 2], [0, 1, 3], [1, 1, 2], [2, 1, 1]]
        },
        jjn: {
          divisor: 48,
          taps: [
            [1, 0, 7], [2, 0, 5],
            [-2, 1, 3], [-1, 1, 5], [0, 1, 7], [1, 1, 5], [2, 1, 3],
            [-2, 2, 1], [-1, 2, 3], [0, 2, 5], [1, 2, 3], [2, 2, 1]
          ]
        }
      };

      // Ordered Bayer matrix of the given power-of-two size, scaled to 0..255.
      // Bits go least significant first: that is the canonical construction.
      // Walking them the other way also yields a plausible-looking matrix, but
      // it drops every low threshold of the tile into one quadrant and the
      // image dithers in visible blocks instead of an even grid.
      function makeBayer(size) {
        const bits = Math.log2(size);
        const matrix = new Float32Array(size * size);
        for (let y = 0; y < size; y++) {
          for (let x = 0; x < size; x++) {
            let value = 0;
            for (let bit = 0; bit < bits; bit++) {
              const xc = (x >> bit) & 1;
              const yc = (y >> bit) & 1;
              value = (value << 2) | ((xc ^ yc) << 1) | yc;
            }
            matrix[y * size + x] = (value + 0.5) / (size * size) * 255;
          }
        }
        return matrix;
      }

      const BAYER = {
        bayer2: { size: 2, matrix: makeBayer(2) },
        bayer4: { size: 4, matrix: makeBayer(4) },
        bayer8: { size: 8, matrix: makeBayer(8) }
      };

      function stretchLevels(values) {
        const histogram = new Uint32Array(256);
        for (let i = 0; i < values.length; i++) {
          histogram[Math.max(0, Math.min(255, Math.round(values[i])))]++;
        }

        // Ignore the darkest and brightest 0.5% so single specks cannot set the range.
        const cut = values.length * 0.005;
        let low = 0;
        let high = 255;
        for (let i = 0, acc = 0; i < 256; i++) {
          acc += histogram[i];
          if (acc > cut) { low = i; break; }
        }
        for (let i = 255, acc = 0; i >= 0; i--) {
          acc += histogram[i];
          if (acc > cut) { high = i; break; }
        }

        if (high - low < 32) return;

        const scale = 255 / (high - low);
        for (let i = 0; i < values.length; i++) {
          values[i] = (values[i] - low) * scale;
        }
      }

      function unsharpMask(values, amount) {
        if (amount <= 0) return;

        const blurred = new Float32Array(values.length);
        const rowSum = new Float32Array(values.length);

        for (let y = 0; y < HEIGHT; y++) {
          for (let x = 0; x < WIDTH; x++) {
            const index = y * WIDTH + x;
            const left = x > 0 ? values[index - 1] : values[index];
            const right = x < WIDTH - 1 ? values[index + 1] : values[index];
            rowSum[index] = (left + values[index] + right) / 3;
          }
        }

        for (let y = 0; y < HEIGHT; y++) {
          for (let x = 0; x < WIDTH; x++) {
            const index = y * WIDTH + x;
            const up = y > 0 ? rowSum[index - WIDTH] : rowSum[index];
            const down = y < HEIGHT - 1 ? rowSum[index + WIDTH] : rowSum[index];
            blurred[index] = (up + rowSum[index] + down) / 3;
          }
        }

        for (let i = 0; i < values.length; i++) {
          values[i] += amount * (values[i] - blurred[i]);
        }
      }

      function convertWorkCanvas() {
        const input = workContext.getImageData(0, 0, WIDTH, HEIGHT);
        const values = new Float32Array(WIDTH * HEIGHT);
        const mode = ui.dither.value;
        const invert = ui.invert.checked;
        const brightness = Number(ui.brightness.value);
        const contrast = Number(ui.contrast.value) / 100;
        const threshold = mode === "none" ? Number(ui.threshold.value) : 128;

        for (let i = 0; i < values.length; i++) {
          const pixel = i * 4;
          let gray = input.data[pixel] * 0.299
            + input.data[pixel + 1] * 0.587
            + input.data[pixel + 2] * 0.114;
          if (invert) gray = 255 - gray;
          values[i] = gray;
        }

        if (ui.autoLevels.checked) stretchLevels(values);

        for (let i = 0; i < values.length; i++) {
          values[i] = 128 + (values[i] - 128) * contrast + brightness;
        }

        unsharpMask(values, Number(ui.sharpen.value) / 100);

        const output = previewContext.createImageData(WIDTH, HEIGHT);
        const bytes = new Uint8Array(BITMAP_BYTES);
        const kernel = KERNELS[mode] || null;
        const ordered = BAYER[mode] || null;

        for (let y = 0; y < HEIGHT; y++) {
          // Serpentine scan: alternating direction hides the diagonal worming
          // that plain left-to-right error diffusion leaves in flat areas.
          const reversed = kernel !== null && (y & 1) === 1;
          for (let step = 0; step < WIDTH; step++) {
            const x = reversed ? WIDTH - 1 - step : step;
            const index = y * WIDTH + x;

            let limit = threshold;
            if (ordered) {
              limit = ordered.matrix[(y % ordered.size) * ordered.size + (x % ordered.size)];
            }

            const oldValue = values[index];
            const black = oldValue < limit;
            const newValue = black ? 0 : 255;

            if (black) {
              bytes[y * ROW_BYTES + (x >> 3)] |= 0x80 >> (x & 7);
            }

            const pixel = index * 4;
            output.data[pixel] = newValue;
            output.data[pixel + 1] = newValue;
            output.data[pixel + 2] = newValue;
            output.data[pixel + 3] = 255;

            if (kernel) {
              // Error stays unclamped, otherwise highlights and shadows swallow
              // it and collapse into flat blobs instead of dithered texture.
              const error = oldValue - newValue;
              for (let t = 0; t < kernel.taps.length; t++) {
                const tap = kernel.taps[t];
                const tx = x + (reversed ? -tap[0] : tap[0]);
                const ty = y + tap[1];
                if (tx < 0 || tx >= WIDTH || ty >= HEIGHT) continue;
                values[ty * WIDTH + tx] += error * tap[2] / kernel.divisor;
              }
            }
          }
        }

        previewContext.putImageData(output, 0, 0);
        packedBitmap = bytes;
        ui.upload.disabled = uploading;
        ui.previewLabel.textContent = "Готово к отправке";
      }

      let shrunkSource = null;
      let shrunkScale = 0;

      // Canvas downscaling in one huge step drops detail, so halve repeatedly
      // until the image is within 2x of the size we actually need.
      function shrinkSource(scale) {
        if (shrunkSource && shrunkScale === scale) return shrunkSource;

        const targetWidth = sourceImage.naturalWidth * scale;
        let width = sourceImage.naturalWidth;
        let height = sourceImage.naturalHeight;
        let current = sourceImage;

        while (width / 2 > targetWidth && width > 2 && height > 2) {
          const nextWidth = Math.max(1, Math.round(width / 2));
          const nextHeight = Math.max(1, Math.round(height / 2));
          const step = document.createElement("canvas");
          step.width = nextWidth;
          step.height = nextHeight;
          const stepContext = step.getContext("2d");
          stepContext.imageSmoothingEnabled = true;
          stepContext.imageSmoothingQuality = "high";
          stepContext.drawImage(current, 0, 0, nextWidth, nextHeight);
          current = step;
          width = nextWidth;
          height = nextHeight;
        }

        shrunkSource = current;
        shrunkScale = scale;
        return current;
      }

      function renderSource() {
        if (!sourceImage) return;

        workContext.save();
        workContext.setTransform(1, 0, 0, 1, 0, 0);
        workContext.fillStyle = "#ffffff";
        workContext.fillRect(0, 0, WIDTH, HEIGHT);
        workContext.imageSmoothingEnabled = true;
        workContext.imageSmoothingQuality = "high";

        const rotated = ui.rotate.checked;
        const orientedWidth = rotated ? sourceImage.naturalHeight : sourceImage.naturalWidth;
        const orientedHeight = rotated ? sourceImage.naturalWidth : sourceImage.naturalHeight;
        const containScale = Math.min(WIDTH / orientedWidth, HEIGHT / orientedHeight);
        const coverScale = Math.max(WIDTH / orientedWidth, HEIGHT / orientedHeight);
        const scale = ui.fit.value === "cover" ? coverScale : containScale;
        const drawWidth = sourceImage.naturalWidth * scale;
        const drawHeight = sourceImage.naturalHeight * scale;

        workContext.translate(WIDTH / 2, HEIGHT / 2);
        if (rotated) workContext.rotate(Math.PI / 2);
        workContext.drawImage(shrinkSource(scale), -drawWidth / 2, -drawHeight / 2, drawWidth, drawHeight);
        workContext.restore();

        convertWorkCanvas();
      }

      async function useFile(file) {
        if (!file) return;
        if (!file.type.startsWith("image/")) {
          setMessage("Это не графический файл.", "bad");
          return;
        }

        const image = new Image();
        const nextUrl = URL.createObjectURL(file);

        try {
          await new Promise((resolve, reject) => {
            image.onload = resolve;
            image.onerror = () => reject(new Error("Браузер не смог открыть изображение"));
            image.src = nextUrl;
          });

          if (sourceUrl) URL.revokeObjectURL(sourceUrl);
          sourceUrl = nextUrl;
          sourceImage = image;
          shrunkSource = null;
          shrunkScale = 0;
          ui.fileName.textContent = file.name;
          ui.rotate.checked = false;
          renderSource();
          setMessage("Настройте предпросмотр и отправьте кадр.");
        } catch (error) {
          URL.revokeObjectURL(nextUrl);
          setMessage(error.message, "bad");
        }
      }

      async function readJson(response) {
        try {
          return await response.json();
        } catch (_) {
          return {};
        }
      }

      function pause(ms) {
        return new Promise((resolve) => setTimeout(resolve, ms));
      }

      async function waitForDisplay() {
        for (let attempt = 0; attempt < 30; attempt++) {
          await pause(750);
          try {
            const response = await fetch(`/status?t=${Date.now()}`, { cache: "no-store" });
            if (!response.ok) continue;
            const status = await response.json();
            showConnection(true, status.clients);
            if (!status.busy) return true;
          } catch (_) {
            // During a full e-paper update the single-threaded HTTP server pauses.
          }
        }
        return false;
      }

      async function uploadBitmap() {
        if (!packedBitmap || uploading) return;

        setUploading(true);
        setMessage("Передаю 4 736 байт в ESP32…");

        try {
          const form = new FormData();
          form.append(
            "image",
            new Blob([packedBitmap], { type: "application/octet-stream" }),
            "screen.bin"
          );

          const response = await fetch("/upload", { method: "POST", body: form });
          const result = await readJson(response);
          if (!response.ok || !result.ok) {
            throw new Error(result.error || `Ошибка HTTP ${response.status}`);
          }

          setMessage("Кадр принят. Экран выполняет полный цикл обновления…", "ok");
          const complete = await waitForDisplay();
          setMessage(
            complete
              ? "Готово — изображение сохранено и показано на экране."
              : "Изображение сохранено. Обновление экрана может ещё продолжаться.",
            "ok"
          );
          ui.previewLabel.textContent = "Текущее изображение";
        } catch (error) {
          showConnection(false);
          setMessage(`Не удалось загрузить: ${error.message}`, "bad");
        } finally {
          setUploading(false);
        }
      }

      async function loadDeviceState() {
        try {
          const response = await fetch(`/status?t=${Date.now()}`, { cache: "no-store" });
          if (!response.ok) throw new Error();
          const status = await response.json();
          showConnection(true, status.clients);

          if (status.hasImage) {
            const imageResponse = await fetch(`/current?t=${Date.now()}`, { cache: "no-store" });
            if (!imageResponse.ok) throw new Error();
            const bytes = new Uint8Array(await imageResponse.arrayBuffer());
            drawPackedBitmap(bytes);
            ui.previewLabel.textContent = "Текущее изображение";
            setMessage("Загружен кадр, сохранённый в ESP32.");
          } else {
            setMessage("Выберите изображение для первого обновления.");
          }
        } catch (_) {
          showConnection(false);
          setMessage("Нет связи. Подключитесь к Wi-Fi «E-Paper-C3» и откройте 192.168.4.1.", "bad");
        }
      }

      ui.file.addEventListener("change", () => useFile(ui.file.files[0]));
      ui.fit.addEventListener("change", renderSource);
      ui.rotate.addEventListener("change", renderSource);
      function syncThresholdRow() {
        ui.thresholdRow.hidden = ui.dither.value !== "none";
      }

      ui.dither.addEventListener("change", () => {
        syncThresholdRow();
        renderSource();
      });
      ui.autoLevels.addEventListener("change", renderSource);
      ui.invert.addEventListener("change", renderSource);
      ui.brightness.addEventListener("input", () => {
        ui.brightnessValue.textContent = ui.brightness.value;
        renderSource();
      });
      ui.contrast.addEventListener("input", () => {
        ui.contrastValue.textContent = (Number(ui.contrast.value) / 100).toFixed(2);
        renderSource();
      });
      ui.sharpen.addEventListener("input", () => {
        ui.sharpenValue.textContent = (Number(ui.sharpen.value) / 100).toFixed(1);
        renderSource();
      });
      ui.threshold.addEventListener("input", () => {
        ui.thresholdValue.textContent = ui.threshold.value;
        renderSource();
      });
      syncThresholdRow();
      ui.upload.addEventListener("click", uploadBitmap);

      for (const eventName of ["dragenter", "dragover"]) {
        ui.dropZone.addEventListener(eventName, (event) => {
          event.preventDefault();
          ui.dropZone.classList.add("drag");
        });
      }

      for (const eventName of ["dragleave", "drop"]) {
        ui.dropZone.addEventListener(eventName, (event) => {
          event.preventDefault();
          ui.dropZone.classList.remove("drag");
        });
      }

      ui.dropZone.addEventListener("drop", (event) => useFile(event.dataTransfer.files[0]));

      drawEmptyPreview();
      loadDeviceState();
    })();
  </script>
</body>
</html>
)EPAPERHTML";
