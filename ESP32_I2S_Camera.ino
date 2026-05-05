#include "OV7670.h"
#include <WiFi.h>
#include <Wire.h>

const char* WIFI_SSID     = "Hansel 704";
const char* WIFI_PASSWORD = "28112001";

WiFiServer server(80);
OV7670* camera    = nullptr;
bool cameraReady  = false;

const int CAM_SDA  = 21;
const int CAM_SCL  = 22;
const int CAM_VS   = 32;
const int CAM_HS   = 33;
const int CAM_PCLK = 25;
const int CAM_MCLK = 27;
const int CAM_D0   = 13;
const int CAM_D1   = 17;
const int CAM_D2   = 26;
const int CAM_D3   = 15;
const int CAM_D4   = 36;
const int CAM_D5   = 37;
const int CAM_D6   = 38;
const int CAM_D7   = 39;

const int CAM_PWDN_PIN  = -1;
const int CAM_RESET_PIN = -1;

const int    FRAME_WIDTH  = 160;
const int    FRAME_HEIGHT = 120;
const size_t FRAME_BYTES  = FRAME_WIDTH * FRAME_HEIGHT * 2;

const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>OV7670 Stream</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    :root {
      --bg:        #0d0f12;
      --surface:   #13161b;
      --border:    #1e2330;
      --accent:    #00e5a0;
      --accent-dim:#00b87a;
      --text:      #e2e8f0;
      --muted:     #64748b;
      --radius:    12px;
    }

    body {
      background: var(--bg);
      color: var(--text);
      font-family: 'Inter', 'Segoe UI', system-ui, sans-serif;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 24px;
    }

    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: 28px 24px 24px;
      width: 100%;
      max-width: 560px;
      box-shadow: 0 8px 40px rgba(0,0,0,0.45);
    }

    .header {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 20px;
    }

    @keyframes pulse {
      0%, 100% { opacity: 1; box-shadow: 0 0 8px var(--accent); }
      50%       { opacity: 0.5; box-shadow: 0 0 16px var(--accent); }
    }

    .title {
      font-size: 0.95rem;
      font-weight: 600;
      letter-spacing: 0.04em;
      text-transform: uppercase;
      color: var(--text);
    }

    .badge {
      margin-left: auto;
      font-size: 0.7rem;
      font-weight: 500;
      color: var(--accent);
      background: rgba(0,229,160,0.08);
      border: 1px solid rgba(0,229,160,0.2);
      border-radius: 6px;
      padding: 3px 8px;
      letter-spacing: 0.05em;
      text-transform: uppercase;
    }

    .viewport {
      position: relative;
      background: #000;
      border-radius: 8px;
      overflow: hidden;
      border: 1px solid var(--border);
      line-height: 0;
    }

    canvas {
      width: 100%;
      height: auto;
      display: block;
      image-rendering: pixelated;
    }

    .overlay-corner {
      position: absolute;
      width: 14px;
      height: 14px;
      border-color: var(--accent);
      border-style: solid;
      opacity: 0.6;
    }
    .overlay-corner.tl { top: 8px; left: 8px;  border-width: 2px 0 0 2px; }
    .overlay-corner.tr { top: 8px; right: 8px; border-width: 2px 2px 0 0; }
    .overlay-corner.bl { bottom: 8px; left: 8px;  border-width: 0 0 2px 2px; }
    .overlay-corner.br { bottom: 8px; right: 8px; border-width: 0 2px 2px 0; }

    .footer {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-top: 14px;
      padding-top: 14px;
      border-top: 1px solid var(--border);
    }

    .meta {
      font-size: 0.72rem;
      color: var(--muted);
      letter-spacing: 0.03em;
    }

    #status {
      font-size: 0.78rem;
      font-weight: 600;
      color: var(--accent);
      font-variant-numeric: tabular-nums;
      letter-spacing: 0.04em;
    }

    #status.error {
      color: #f87171;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="header">
      <span class="title">OV7670</span>
    </div>

    <div class="viewport">
      <canvas id="cam" width="160" height="120"></canvas>
      <span class="overlay-corner tl"></span>
      <span class="overlay-corner tr"></span>
      <span class="overlay-corner bl"></span>
      <span class="overlay-corner br"></span>
    </div>

    <div class="footer">
      <span class="meta">160 × 120 px</span>
      <span id="status">Iniciando...</span>
    </div>
  </div>

  <script>
    const canvas   = document.getElementById("cam");
    const ctx      = canvas.getContext("2d");
    const img      = ctx.createImageData(160, 120);
    const statusEl = document.getElementById("status");
    let frames = 0;

    setInterval(() => {
      statusEl.textContent = frames + " FPS";
      statusEl.classList.remove("error");
      frames = 0;
    }, 1000);

    async function fetchFrame() {
      try {
        const res  = await fetch("/frame", { cache: "no-store" });
        if (!res.ok) throw new Error("HTTP " + res.status);
        const data = new Uint8Array(await res.arrayBuffer());
        for (let i = 0, j = 0; i < data.length; i += 2, j += 4) {
          const px   = (data[i] << 8) | data[i + 1];
          img.data[j]   = (px & 0xF800) >> 8;   // R
          img.data[j+1] = (px & 0x07E0) >> 3;   // G
          img.data[j+2] = (px & 0x001F) << 3;   // B
          img.data[j+3] = 255;
        }
        ctx.putImageData(img, 0, 0);
        frames++;
      } catch (e) {
        statusEl.textContent = "Erro: " + e.message;
        statusEl.classList.add("error");
        await new Promise(r => setTimeout(r, 500));
      } finally {
        requestAnimationFrame(fetchFrame);
      }
    }

    fetchFrame();
  </script>
</body>
</html>
)rawliteral";

void configureCameraControlPins()
{
  if (CAM_PWDN_PIN >= 0) {
    pinMode(CAM_PWDN_PIN, OUTPUT);
    digitalWrite(CAM_PWDN_PIN, LOW);
  }
  if (CAM_RESET_PIN >= 0) {
    pinMode(CAM_RESET_PIN, OUTPUT);
    digitalWrite(CAM_RESET_PIN, HIGH);
  }
}

void scanI2CBus()
{
  int found = 0;
  Serial.println("[I2C] Scanning bus...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("[I2C] Device at 0x");
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (!found) Serial.println("[I2C] No device found.");
}

bool connectWiFi(uint32_t timeoutMs)
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Failed.");
    return false;
  }
  Serial.print("[WIFI] Connected: http://");
  Serial.println(WiFi.localIP());
  return true;
}

void sendHttpHeader(WiFiClient& client, const char* status, const char* contentType)
{
  client.print("HTTP/1.1 "); client.println(status);
  client.print("Content-Type: "); client.println(contentType);
  client.println("Connection: close");
  client.println("Cache-Control: no-store");
  client.println();
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=====================================");
  Serial.println(" OV7670 ESP32 Stream");
  Serial.println("=====================================");

  configureCameraControlPins();

  Wire.begin(CAM_SDA, CAM_SCL);
  scanI2CBus();
  Wire.end();

  Serial.println("[CAM] Inicializando OV7670...");
  camera = new OV7670(
    OV7670::QQVGA_RGB565,
    CAM_SDA, CAM_SCL, CAM_VS, CAM_HS, CAM_MCLK, CAM_PCLK,
    CAM_D0, CAM_D1, CAM_D2, CAM_D3, CAM_D4, CAM_D5, CAM_D6, CAM_D7);

  cameraReady = (camera != nullptr && camera->frame != nullptr);
  Serial.println(cameraReady ? "[CAM] OK." : "[CAM] Falhou.");

  if (connectWiFi(20000)) {
    server.begin();
    Serial.println("[HTTP] Servidor rodando.");
  } else {
    Serial.println("[HTTP] Sem WiFi, servidor nao iniciado.");
  }
}

void loop()
{
  WiFiClient client = server.available();
  if (!client) return;

  uint32_t start = millis();
  while (client.connected() && !client.available() && (millis() - start) < 1000)
    delay(1);

  if (!client.available()) { client.stop(); return; }

  String req = client.readStringUntil('\n');
  req.trim();
  while (client.connected() && client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  if (req.startsWith("GET /frame")) {
    if (!cameraReady) {
      sendHttpHeader(client, "503 Service Unavailable", "text/plain");
      client.println("Camera not ready");
    } else {
      camera->oneFrame();
      sendHttpHeader(client, "200 OK", "application/octet-stream");
      client.write((const uint8_t*)camera->frame, FRAME_BYTES);
    }
  } else if (req.startsWith("GET / ")) {
    sendHttpHeader(client, "200 OK", "text/html; charset=utf-8");
    client.println(index_html);
  } else {
    sendHttpHeader(client, "404 Not Found", "text/plain");
    client.println("Not found");
  }

  client.stop();
}
