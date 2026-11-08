#pragma once

#include <Arduino.h>

// Captive Portal HTML (for AP mode provisioning)
const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
  <title>CastPad Setup</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
    body { background: #0f172a; color: #f8fafc; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 100vh; padding: 20px; }
    .card { background: #1e293b; border-radius: 16px; border: 1px solid #334155; padding: 28px; width: 100%; max-width: 400px; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5); }
    h1 { font-size: 24px; font-weight: 700; color: #38bdf8; margin-bottom: 8px; text-align: center; }
    p { font-size: 14px; color: #94a3b8; margin-bottom: 24px; text-align: center; }
    .form-group { margin-bottom: 18px; }
    label { display: block; font-size: 13px; font-weight: 600; color: #cbd5e1; margin-bottom: 6px; }
    select, input { width: 100%; padding: 12px 14px; border-radius: 10px; border: 1px solid #475569; background: #0f172a; color: #f8fafc; font-size: 15px; outline: none; transition: border-color 0.2s; }
    select:focus, input:focus { border-color: #38bdf8; }
    button { width: 100%; padding: 14px; border-radius: 10px; border: none; background: #0284c7; color: white; font-size: 16px; font-weight: 600; cursor: pointer; transition: background 0.2s; margin-top: 8px; }
    button:active { background: #0369a1; }
    .status { margin-top: 14px; text-align: center; font-size: 13px; color: #38bdf8; min-height: 20px; }
    .scan-btn { background: #334155; margin-bottom: 12px; padding: 8px; font-size: 13px; }
  </style>
</head>
<body>
  <div class="card">
    <h1>CastPad</h1>
    <p>Connect to your home Wi-Fi network</p>

    <div class="form-group">
      <label for="ssid">Wi-Fi Network</label>
      <select id="ssid">
        <option value="">Scanning networks...</option>
      </select>
    </div>
    <button class="scan-btn" onclick="scanWifi()">Rescan Networks</button>

    <div class="form-group">
      <label for="manual_ssid">Or Enter SSID Manually</label>
      <input type="text" id="manual_ssid" placeholder="Hidden or manual SSID">
    </div>

    <div class="form-group">
      <label for="pass">Wi-Fi Password</label>
      <input type="password" id="pass" placeholder="Enter password">
    </div>

    <div class="form-group">
      <label for="host">Hostname / Device Name</label>
      <input type="text" id="host" value="CastPad" placeholder="CastPad" maxlength="32">
    </div>

    <button onclick="saveConfig()">Save & Connect</button>
    <button class="scan-btn" onclick="useStandaloneAp()" style="margin-top: 10px; background: rgba(56, 189, 248, 0.2); border: 1px solid rgba(56, 189, 248, 0.4); color: #38bdf8;">Run in Standalone AP Mode (Direct Connect)</button>
    <div class="status" id="status"></div>

    <hr style="border: none; border-top: 1px solid #334155; margin: 24px 0 16px;">
    <div style="font-size: 13px; color: #94a3b8; text-align: center; margin-bottom: 10px;">Or update firmware directly (.bin)</div>
    <input type="file" id="ota_file" accept=".bin" style="margin-bottom: 8px; font-size: 13px;">
    <button class="scan-btn" onclick="uploadOta()" style="background: #0284c7;">Upload Firmware</button>
    <div class="status" id="ota_status"></div>
  </div>

  <script>
    function scanWifi() {
      const select = document.getElementById('ssid');
      select.innerHTML = '<option value="">Scanning...</option>';
      fetch('/scan')
        .then(r => r.json())
        .then(networks => {
          select.innerHTML = '<option value="">-- Select a Network --</option>';
          if (networks.length === 0) {
            select.innerHTML = '<option value="">No networks found</option>';
            return;
          }
          networks.forEach(n => {
            const opt = document.createElement('option');
            opt.value = n.ssid;
            opt.textContent = `${n.ssid} (${n.rssi} dBm)`;
            select.appendChild(opt);
          });
        })
        .catch(() => {
          select.innerHTML = '<option value="">Error scanning</option>';
        });
    }

    function saveConfig() {
      const selectSsid = document.getElementById('ssid').value;
      const manualSsid = document.getElementById('manual_ssid').value.trim();
      const ssid = manualSsid || selectSsid;
      const pass = document.getElementById('pass').value;
      const rawHost = document.getElementById('host').value.trim();
      let host = rawHost.replace(/[^a-zA-Z0-9-]/g, '').replace(/^-+|-+$/g, '').slice(0, 32);
      if (!host) host = 'CastPad';
      const status = document.getElementById('status');

      if (!ssid) {
        status.textContent = 'Please select or enter an SSID.';
        return;
      }

      status.textContent = 'Saving & rebooting...';

      fetch('/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, pass, host })
      })
      .then(r => r.text())
      .then(msg => {
        status.textContent = 'Config saved! Board is rebooting. Connect to your Wi-Fi and open http://' + host + '.local';
      })
      .catch(() => {
        status.textContent = 'Rebooting... Open http://' + host + '.local on your Wi-Fi.';
      });
    }

    function useStandaloneAp() {
      const rawHost = document.getElementById('host').value.trim();
      let host = rawHost.replace(/[^a-zA-Z0-9-]/g, '').replace(/^-+|-+$/g, '').slice(0, 32);
      if (!host) host = 'CastPad';
      const status = document.getElementById('status');
      status.textContent = 'Enabling AP Mode & rebooting...';
      fetch('/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ap_mode: true, host })
      })
      .then(() => {
        status.textContent = 'AP Mode enabled! Connect to Wi-Fi "' + host + '" and open http://192.168.4.1';
      })
      .catch(() => {
        status.textContent = 'Rebooting... Connect to Wi-Fi "' + host + '" and open http://192.168.4.1';
      });
    }

    function uploadOta() {
      const fileInput = document.getElementById('ota_file');
      const status = document.getElementById('ota_status');
      if (!fileInput.files || !fileInput.files[0]) {
        status.textContent = 'Please select a .bin file.';
        return;
      }
      const file = fileInput.files[0];
      status.textContent = 'Uploading firmware...';
      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/update', true);
      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          status.textContent = `Uploading: ${pct}%`;
        }
      };
      xhr.onload = () => {
        if (xhr.status === 200) {
          status.textContent = 'Update successful! Board is rebooting...';
        } else {
          status.textContent = 'Update failed: ' + xhr.responseText;
        }
      };
      xhr.onerror = () => { status.textContent = 'Upload network error.'; };
      // Must be multipart/form-data: the /update endpoint uses the upload handler
      const formData = new FormData();
      formData.append('firmware', file);
      xhr.send(formData);
    }

    window.onload = scanWifi;
  </script>
</body>
</html>
)rawliteral";

// Web App Manifest (PWA)
const char MANIFEST_JSON[] PROGMEM = R"rawliteral({
  "name": "CastPad",
  "short_name": "CastPad",
  "description": "CastPad BLE Remote & Trackpad",
  "start_url": "/",
  "display": "standalone",
  "orientation": "portrait",
  "background_color": "#0b0f19",
  "theme_color": "#0b0f19",
  "icons": [
    {
      "src": "/icon.svg",
      "sizes": "192x192 512x512",
      "type": "image/svg+xml",
      "purpose": "any maskable"
    }
  ]
})rawliteral";

// App Icon (SVG)
const char ICON_SVG[] PROGMEM = R"rawliteral(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 192" width="192" height="192">
  <rect width="192" height="192" rx="44" fill="#0b0f19"/>
  <rect x="42" y="22" width="108" height="148" rx="28" fill="#1e293b" stroke="#38bdf8" stroke-width="5"/>
  <circle cx="96" cy="54" r="14" fill="#38bdf8"/>
  <circle cx="96" cy="118" r="26" fill="#0f172a" stroke="#38bdf8" stroke-width="4"/>
  <circle cx="96" cy="118" r="10" fill="#38bdf8"/>
  <line x1="68" y1="84" x2="84" y2="84" stroke="#94a3b8" stroke-width="3" stroke-linecap="round"/>
  <line x1="108" y1="84" x2="124" y2="84" stroke="#94a3b8" stroke-width="3" stroke-linecap="round"/>
</svg>)rawliteral";

// Main iPhone 15 Optimized Application HTML
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <meta name="apple-mobile-web-app-title" content="CastPad">
  <meta name="application-name" content="CastPad">
  <meta name="theme-color" content="#0b0f19">
  <link rel="manifest" href="/manifest.json">
  <link rel="icon" type="image/svg+xml" href="/icon.svg">
  <link rel="apple-touch-icon" href="/icon.svg">
  <title>CastPad</title>
  <style>
    :root {
      --bg: #0b0f19;
      --card-bg: rgba(22, 27, 39, 0.85);
      --glass: rgba(255, 255, 255, 0.05);
      --border: rgba(6, 182, 212, 0.35);
      --text: #f3f4f6;
      --text-muted: #9ca3af;
      --primary: #06b6d4;
      --primary-glow: rgba(6, 182, 212, 0.35);
      --btn-bg: #0891b2;
      --btn-border: rgba(6, 182, 212, 0.65);
      --red: #ef4444;
      --green: #22c55e;
      --yellow: #eab308;
      --blue: #3b82f6;
      --btn-gap: clamp(8px, 2.2vw, 10px);
      --row-gap: clamp(8px, 1.6vh, 12px);
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      -webkit-tap-highlight-color: transparent;
      user-select: none;
      -webkit-user-select: none;
      touch-action: manipulation;
    }

    body {
      background: #080b12;
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "SF Pro Display", "SF Pro Text", Helvetica, Arial, sans-serif;
      height: 100vh;
      height: -webkit-fill-available;
      height: 100dvh;
      overflow: hidden;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      margin: 0;
      padding: 0;
    }

    /* iPhone 15 frame container: constrained & simulated frame on larger screens */
    .phone-frame {
      width: 100%;
      height: 100%;
      max-width: 414px;
      max-height: 874px;
      background: var(--bg);
      display: flex;
      flex-direction: column;
      position: relative;
      overflow: hidden;
      padding-top: max(10px, env(safe-area-inset-top));
      padding-bottom: max(12px, env(safe-area-inset-bottom));
      padding-left: max(14px, env(safe-area-inset-left));
      padding-right: max(14px, env(safe-area-inset-right));
    }

    @media (min-width: 480px) and (min-height: 600px) {
      body {
        padding: 20px;
        background: radial-gradient(circle at 50% 20%, #151d2f 0%, #060910 100%);
      }
      .phone-frame {
        border-radius: 46px;
        border: 2px solid var(--border);
        box-shadow: 0 28px 75px rgba(0, 0, 0, 0.8), 0 0 0 1px rgba(255, 255, 255, 0.06);
      }
    }

    /* Header & Navigation Bar */
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 4px 2px 8px 2px;
      width: 100%;
      flex-shrink: 0;
    }

    .brand {
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .brand h1 {
      font-size: 19px;
      font-weight: 700;
      letter-spacing: -0.3px;
      background: linear-gradient(135deg, var(--primary), #818cf8);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }

    .badge {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      padding: 4px 9px;
      border-radius: 9999px;
      font-size: 11px;
      font-weight: 600;
      background: rgba(239, 68, 68, 0.15);
      color: #f87171;
      border: 1px solid rgba(239, 68, 68, 0.3);
      transition: all 0.15s ease;
    }

    .badge.connected {
      background: rgba(34, 197, 94, 0.15);
      color: #4ade80;
      border-color: rgba(34, 197, 94, 0.3);
    }

    .badge.radio-off {
      background: rgba(148, 163, 184, 0.15);
      color: #94a3b8;
      border-color: rgba(148, 163, 184, 0.3);
    }

    .badge.tx-flash {
      box-shadow: 0 0 16px var(--primary-glow);
      border-color: var(--primary);
      transform: scale(1.06);
    }

    .dot {
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: currentColor;
      transition: transform 0.15s ease, box-shadow 0.15s ease;
    }

    .badge.tx-flash .dot {
      transform: scale(1.5);
      box-shadow: 0 0 8px currentColor;
    }

    /* Touch Ripple Animation */
    .touch-ripple {
      position: absolute;
      border-radius: 50%;
      background: radial-gradient(circle, rgba(255, 255, 255, 0.65) 0%, rgba(56, 189, 248, 0.45) 60%, transparent 100%);
      transform: scale(0);
      animation: rippleAnim 0.42s cubic-bezier(0.22, 1, 0.36, 1) forwards;
      pointer-events: none;
      z-index: 10;
    }

    @keyframes rippleAnim {
      to {
        transform: scale(2.6);
        opacity: 0;
      }
    }

    /* Distinct Icon-Only Top Tabs */
    .tabs {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: var(--btn-gap);
      background: rgba(15, 23, 42, 0.85);
      border-radius: 16px;
      padding: 5px;
      border: 1px solid var(--border);
      margin-bottom: var(--row-gap);
      width: 100%;
      flex-shrink: 0;
    }

    .tab-btn {
      position: relative;
      overflow: hidden;
      height: 42px;
      display: flex;
      align-items: center;
      justify-content: center;
      color: var(--text-muted);
      border: 1px solid var(--border);
      background: rgba(30, 41, 59, 0.45);
      border-radius: 12px;
      cursor: pointer;
      transition: all 0.14s ease;
    }

    .tab-btn svg {
      width: 22px;
      height: 22px;
      transition: transform 0.14s ease;
    }

    /* Distinct per-tab subtle accents & active states */
    .tab-btn.tab-remote { color: var(--primary); }
    .tab-btn.tab-remote.active {
      color: #fff;
      background: linear-gradient(135deg, var(--primary-glow), rgba(2, 132, 199, 0.22));
      border-color: var(--primary);
      box-shadow: 0 0 14px var(--primary-glow), inset 0 1px 0 rgba(255, 255, 255, 0.15);
    }

    .tab-btn.tab-ql { color: #6ee7b7; }
    .tab-btn.tab-ql.active {
      color: #fff;
      background: linear-gradient(135deg, rgba(16, 185, 129, 0.32), rgba(5, 150, 105, 0.22));
      border-color: rgba(52, 211, 153, 0.75);
      box-shadow: 0 0 14px rgba(52, 211, 153, 0.35), inset 0 1px 0 rgba(255, 255, 255, 0.15);
    }

    .tab-btn.tab-mouse { color: #c4b5fd; }
    .tab-btn.tab-mouse.active {
      color: #fff;
      background: linear-gradient(135deg, rgba(139, 92, 246, 0.34), rgba(109, 40, 217, 0.22));
      border-color: rgba(167, 139, 250, 0.75);
      box-shadow: 0 0 14px rgba(167, 139, 250, 0.35), inset 0 1px 0 rgba(255, 255, 255, 0.15);
    }

    .tab-btn.tab-settings { color: #fdba74; }
    .tab-btn.tab-settings.active {
      color: #fff;
      background: linear-gradient(135deg, rgba(249, 115, 22, 0.34), rgba(234, 88, 12, 0.22));
      border-color: rgba(251, 146, 60, 0.75);
      box-shadow: 0 0 14px rgba(251, 146, 60, 0.35), inset 0 1px 0 rgba(255, 255, 255, 0.15);
    }

    .tab-btn.active svg {
      transform: scale(1.1);
    }

    .tab-btn:active,
    .tab-btn.btn-pressed {
      transform: scale(0.93);
      color: #fff;
    }

    /* Tab Contents */
    .tab-content {
      display: none;
      flex: 1;
      flex-direction: column;
      overflow: hidden;
      min-height: 0;
    }

    .tab-content.active {
      display: flex;
    }

    #tab-remote {
      width: 100%;
      height: 100%;
      display: none;
      flex-direction: column;
      justify-content: space-between;
      gap: var(--row-gap);
      overflow: hidden;
    }

    #tab-remote.active {
      display: flex;
    }

    #tab-settings {
      flex: 1 1 0;
      min-height: 0; /* allow flex item to shrink below content height so it can scroll */
      overflow-y: auto !important;
      -webkit-overflow-scrolling: touch !important;
      touch-action: pan-y !important;
      overscroll-behavior-y: contain;
    }

    #tab-settings.active {
      display: block !important;
    }

    /* Responsive Square 3x3 D-PAD with Corner Aux Buttons */
    .dpad-container {
      flex: 1 1 auto;
      display: flex;
      align-items: center;
      justify-content: center;
      width: 100%;
      margin: 0;
      min-height: 0;
    }

    .dpad {
      position: relative;
      width: 100%;
      aspect-ratio: 1 / 1;
      max-height: 100%;
      max-width: 100%;
      display: grid;
      grid-template-columns: 1fr 1.15fr 1fr;
      grid-template-rows: 1fr 1.15fr 1fr;
      gap: var(--btn-gap);
      padding: clamp(8px, 2.2vw, 12px);
      background: linear-gradient(145deg, #162032 0%, #0d1322 100%);
      border-radius: clamp(20px, 4vw, 28px);
      border: 2px solid var(--border);
      box-shadow: inset 0 2px 10px rgba(0,0,0,0.65), 0 12px 28px rgba(0,0,0,0.45);
    }

    .dpad-btn {
      position: relative;
      overflow: hidden;
      background: linear-gradient(145deg, #243247, #1b2638);
      border: 1px solid var(--border);
      color: #f1f5f9;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      box-shadow: 0 4px 10px rgba(0,0,0,0.35), inset 0 1px 0 rgba(255,255,255,0.08);
      transition: all 0.08s ease;
    }

    .dpad-btn svg {
      width: clamp(20px, 5.2vw, 28px);
      height: clamp(20px, 5.2vw, 28px);
    }

    .dpad-up { border-radius: 20px 20px 12px 12px; }
    .dpad-down { border-radius: 12px 12px 20px 20px; }
    .dpad-left { border-radius: 20px 12px 12px 20px; }
    .dpad-right { border-radius: 12px 20px 20px 12px; }

    .dpad-btn:active,
    .dpad-btn.btn-pressed {
      background: var(--primary);
      color: #0f172a;
      border-color: #ffffff;
      transform: scale(0.93);
      box-shadow: 0 0 22px var(--primary-glow), inset 0 0 8px rgba(255, 255, 255, 0.5);
      z-index: 2;
    }

    /* Corner buttons on the squarish D-Pad: Custom 1..4 */
    .dpad-aux {
      position: relative;
      overflow: hidden;
      background: rgba(17, 24, 39, 0.45);
      border: 1px solid var(--border);
      border-radius: clamp(12px, 2.5vw, 18px);
      color: #cbd5e1;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 2px;
      padding: 4px;
      cursor: pointer;
      box-shadow: inset 0 1px 0 rgba(255,255,255,0.03), 0 3px 8px rgba(0,0,0,0.2);
      transition: all 0.12s ease;
    }

    .dpad-aux.configured {
      background: rgba(17, 24, 39, 0.78);
      border-color: var(--border);
      box-shadow: inset 0 1px 0 rgba(255,255,255,0.05), 0 3px 8px rgba(0,0,0,0.3);
    }

    .dpad-aux:active,
    .dpad-aux.btn-pressed {
      background: var(--primary);
      color: #0f172a;
      border-color: #ffffff;
      transform: scale(0.93);
      box-shadow: 0 0 22px var(--primary-glow), inset 0 0 8px rgba(255, 255, 255, 0.5);
      z-index: 2;
    }

    .dpad-aux-label {
      font-size: clamp(12px, 3.2vw, 15px);
      font-weight: 800;
      letter-spacing: 0.5px;
      line-height: 1;
      text-align: center;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      max-width: 90%;
    }

    .dpad-center {
      position: relative;
      overflow: hidden;
      width: 100%;
      height: 100%;
      border-radius: clamp(14px, 3vw, 22px);
      background: linear-gradient(135deg, #334155, #1e293b);
      border: 2px solid var(--primary);
      color: var(--primary);
      font-size: clamp(15px, 3.8vw, 18px);
      font-weight: 800;
      letter-spacing: 0.5px;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      box-shadow: 0 4px 12px rgba(0,0,0,0.55);
      transition: all 0.08s ease;
    }

    .dpad-center:active,
    .dpad-center.btn-pressed {
      background: var(--primary);
      color: #0f172a;
      border-color: #ffffff;
      transform: scale(0.93);
      box-shadow: 0 0 22px var(--primary-glow), inset 0 0 8px rgba(255, 255, 255, 0.5);
      z-index: 3;
    }

    /* Aux / Navigation & Media Rows (3 buttons per row) */
    .nav-row {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: var(--btn-gap);
      width: 100%;
      flex-shrink: 0;
    }

    .nav-btn {
      position: relative;
      overflow: hidden;
      width: 100%;
      height: clamp(50px, 6.8vh, 64px);
      min-height: 48px;
      padding: clamp(6px, 1vh, 8px) 4px;
      background: rgba(30, 41, 59, 0.75);
      border: 1px solid var(--border);
      border-radius: clamp(12px, 2.8vw, 16px);
      color: #e2e8f0;
      font-size: clamp(11px, 2.7vw, 13px);
      font-weight: 600;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: clamp(3px, 0.6vh, 5px);
      white-space: nowrap;
      cursor: pointer;
      transition: all 0.08s ease;
    }

    .nav-btn svg {
      width: clamp(18px, 4.4vw, 22px);
      height: clamp(18px, 4.4vw, 22px);
      flex-shrink: 0;
    }

    .nav-btn:active,
    .nav-btn.btn-pressed {
      background: var(--primary);
      color: #0f172a;
      border-color: #ffffff;
      transform: scale(0.93);
      box-shadow: 0 0 22px var(--primary-glow), inset 0 0 8px rgba(255, 255, 255, 0.5);
    }

    /* 4 Color Buttons (No dots) */
    .color-row {
      flex: 0 0 auto;
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: var(--btn-gap);
      width: 100%;
      padding-top: 0;
      padding-bottom: 2px;
    }

    .color-btn {
      position: relative;
      overflow: hidden;
      width: 100%;
      height: clamp(48px, 6.5vh, 60px);
      min-height: 44px;
      padding: clamp(6px, 1vh, 8px) 4px;
      border-radius: clamp(12px, 2.8vw, 16px);
      border: 2px solid transparent;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 2px;
      cursor: pointer;
      box-shadow: 0 4px 12px rgba(0,0,0,0.3);
      transition: all 0.08s ease;
    }

    .color-btn:active,
    .color-btn.btn-pressed {
      transform: scale(0.91);
    }

    .color-label {
      font-size: clamp(10px, 2.5vw, 12px);
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.4px;
      color: #f8fafc;
      max-width: 100%;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .btn-red { background: rgba(239, 68, 68, 0.25); border-color: var(--border); }
    .btn-red:active, .btn-red.btn-pressed {
      background: rgba(239, 68, 68, 0.55);
      border-color: #fca5a5;
      box-shadow: 0 0 20px rgba(239, 68, 68, 0.85), inset 0 0 8px rgba(255, 255, 255, 0.35);
    }

    .btn-green { background: rgba(34, 197, 94, 0.25); border-color: var(--border); }
    .btn-green:active, .btn-green.btn-pressed {
      background: rgba(34, 197, 94, 0.55);
      border-color: #86efac;
      box-shadow: 0 0 20px rgba(34, 197, 94, 0.85), inset 0 0 8px rgba(255, 255, 255, 0.35);
    }

    .btn-yellow { background: rgba(234, 179, 8, 0.25); border-color: var(--border); }
    .btn-yellow:active, .btn-yellow.btn-pressed {
      background: rgba(234, 179, 8, 0.55);
      border-color: #fde047;
      box-shadow: 0 0 20px rgba(234, 179, 8, 0.85), inset 0 0 8px rgba(255, 255, 255, 0.35);
    }

    .btn-blue { background: rgba(59, 130, 246, 0.25); border-color: var(--border); }
    .btn-blue:active, .btn-blue.btn-pressed {
      background: rgba(59, 130, 246, 0.55);
      border-color: #93c5fd;
      box-shadow: 0 0 20px rgba(59, 130, 246, 0.85), inset 0 0 8px rgba(255, 255, 255, 0.35);
    }

    @media (max-height: 700px) {
      header { padding: 4px 2px 6px 2px; }
      .tabs { padding: 4px; margin-bottom: 6px; }
      .tab-btn { height: 38px; }
    }

    @media (max-height: 540px) {
      body {
        overflow-y: auto !important;
      }
      .phone-frame {
        max-height: none;
        height: auto;
      }
      #tab-remote {
        overflow-y: auto !important;
        -webkit-overflow-scrolling: touch;
      }
    }

    /* QUICK LINKS TAB (Paging, Glassmorphism, Accents) */
    #tab-quicklinks {
      width: 100%;
      height: 100%;
    }

    .ql-wrapper {
      flex: 1;
      display: flex;
      flex-direction: column;
      min-height: 0;
      gap: var(--row-gap);
      height: 100%;
    }

    .ql-grid {
      flex: 1;
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      grid-template-rows: repeat(4, 1fr);
      gap: var(--btn-gap);
      padding: 2px 0;
      min-height: 0;
      transition: opacity 0.12s ease, transform 0.12s ease;
    }

    .ql-grid.page-transitioning {
      opacity: 0.35;
      transform: scale(0.97);
    }

    .ql-btn {
      position: relative;
      overflow: hidden;
      width: 100%;
      height: 100%;
      background: linear-gradient(135deg, rgba(30, 41, 59, 0.72), rgba(15, 23, 42, 0.78));
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border: 1.5px solid var(--border);
      border-radius: clamp(14px, 3.2vw, 18px);
      color: #f1f5f9;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: clamp(8px, 2vw, 14px) 6px;
      cursor: pointer;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 1px 0 rgba(255,255,255,0.06);
      transition: transform 0.08s ease, box-shadow 0.08s ease, border-color 0.15s ease;
    }

    .ql-btn.unconfigured {
      background: rgba(15, 23, 42, 0.28);
      border: 1.5px solid var(--border);
      opacity: 0.45;
      box-shadow: none;
      cursor: default;
    }

    .ql-btn.unconfigured .ql-slot-num {
      font-size: 13px;
      font-weight: 600;
      color: rgba(255, 255, 255, 0.2);
    }

    /* Unified Button Color Accents: custom color overrides border & glow; 'theme' uses active theme */
    .ql-btn:not(.unconfigured)[data-color="theme"],
    .color-btn[data-color="theme"],
    .dpad-aux[data-color="theme"] {
      border-color: var(--border) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px var(--primary-glow);
    }
    .ql-btn:not(.unconfigured)[data-color="sky"],
    .color-btn[data-color="sky"],
    .dpad-aux[data-color="sky"] {
      border-color: rgba(56, 189, 248, 0.75) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px rgba(56, 189, 248, 0.15) !important;
    }
    .ql-btn:not(.unconfigured)[data-color="green"],
    .color-btn[data-color="green"],
    .dpad-aux[data-color="green"] {
      border-color: rgba(34, 197, 94, 0.75) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px rgba(52, 211, 153, 0.15) !important;
    }
    .ql-btn:not(.unconfigured)[data-color="amber"],
    .color-btn[data-color="amber"],
    .dpad-aux[data-color="amber"] {
      border-color: rgba(245, 158, 11, 0.75) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px rgba(251, 191, 36, 0.15) !important;
    }
    .ql-btn:not(.unconfigured)[data-color="red"],
    .color-btn[data-color="red"],
    .dpad-aux[data-color="red"] {
      border-color: rgba(239, 68, 68, 0.75) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px rgba(251, 113, 133, 0.15) !important;
    }
    .ql-btn:not(.unconfigured)[data-color="indigo"],
    .color-btn[data-color="indigo"],
    .dpad-aux[data-color="indigo"] {
      border-color: rgba(99, 102, 241, 0.75) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px rgba(129, 140, 248, 0.15) !important;
    }
    .ql-btn:not(.unconfigured)[data-color="purple"],
    .color-btn[data-color="purple"],
    .dpad-aux[data-color="purple"] {
      border-color: rgba(168, 85, 247, 0.75) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px rgba(167, 139, 250, 0.15) !important;
    }
    .ql-btn:not(.unconfigured)[data-color="pink"],
    .color-btn[data-color="pink"],
    .dpad-aux[data-color="pink"] {
      border-color: rgba(244, 63, 94, 0.75) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px rgba(244, 114, 182, 0.15) !important;
    }
    .ql-btn:not(.unconfigured)[data-color="slate"],
    .color-btn[data-color="slate"],
    .dpad-aux[data-color="slate"] {
      border-color: rgba(148, 163, 184, 0.75) !important;
      box-shadow: 0 4px 14px rgba(0,0,0,0.32), inset 0 0 12px rgba(148, 163, 184, 0.15) !important;
    }

    .ql-btn:active,
    .ql-btn.btn-pressed {
      transform: scale(0.93);
      box-shadow: 0 0 22px var(--primary-glow), inset 0 0 8px rgba(255, 255, 255, 0.5);
    }

    .btn-emoji {
      font-size: clamp(16px, 4.2vw, 22px);
      line-height: 1;
      display: block;
      margin-bottom: 2px;
      user-select: none;
    }
    .btn-emoji.large {
      font-size: clamp(22px, 5.8vw, 30px);
      margin-bottom: 0;
    }
    .btn-text-label {
      font-size: clamp(10px, 2.5vw, 13px);
      font-weight: 700;
      text-align: center;
      line-height: 1.15;
      max-width: 95%;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      display: block;
    }

    .ql-btn-label {
      font-size: clamp(12px, 3.2vw, 15px);
      font-weight: 700;
      text-align: center;
      line-height: 1.2;
      max-width: 100%;
      overflow: hidden;
      text-overflow: ellipsis;
      display: -webkit-box;
      -webkit-line-clamp: 2;
      -webkit-box-orient: vertical;
      word-break: break-word;
    }

    /* Never display channel number or sequence on the button face */
    .ql-btn-badge {
      display: none !important;
    }


    /* Emoji Suggestions */
    .emoji-chip {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: 30px;
      height: 30px;
      border-radius: 8px;
      background: rgba(255,255,255,0.06);
      border: 1px solid rgba(255,255,255,0.1);
      cursor: pointer;
      font-size: 16px;
      transition: transform 0.1s ease, background 0.1s ease;
      user-select: none;
    }
    .emoji-chip:hover {
      background: rgba(255,255,255,0.15);
      transform: scale(1.15);
    }

    /* Sequence Builder Styles */
    .seq-step-row {
      display: flex;
      align-items: center;
      gap: 8px;
      background: rgba(15, 23, 42, 0.7);
      border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: 10px;
      padding: 8px 10px;
      margin-bottom: 8px;
    }
    .seq-step-num {
      font-size: 11px;
      font-weight: 800;
      color: var(--primary);
      width: 22px;
    }
    .seq-step-ctrls {
      display: flex;
      gap: 4px;
    }
    .seq-icon-btn {
      background: rgba(255,255,255,0.06);
      border: 1px solid rgba(255,255,255,0.1);
      color: #94a3b8;
      border-radius: 6px;
      padding: 4px 6px;
      font-size: 11px;
      cursor: pointer;
    }
    .seq-icon-btn:hover {
      color: #ffffff;
      background: rgba(255,255,255,0.15);
    }
    .seq-del-btn {
      color: #ef4444;
    }
    .seq-del-btn:hover {
      background: rgba(239,68,68,0.2);
      color: #fca5a5;
    }
    .seq-flow-pill {
      display: inline-flex;
      align-items: center;
      gap: 4px;
      background: rgba(255,255,255,0.08);
      border: 1px solid rgba(255,255,255,0.12);
      border-radius: 6px;
      padding: 2px 6px;
      font-size: 11px;
      font-weight: 700;
      color: #f1f5f9;
    }
    .seq-flow-arrow {
      color: var(--primary);
      font-size: 10px;
    }

    /* Pagination Dots - Significantly Bigger & Thumb-Friendly */
    .ql-pagination {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 16px;
      padding: 10px 0 4px 0;
      user-select: none;
      min-height: 42px;
    }

    .ql-dot {
      width: 14px;
      height: 14px;
      border-radius: 50%;
      background: rgba(255, 255, 255, 0.28);
      cursor: pointer;
      position: relative;
      transition: all 0.22s cubic-bezier(0.4, 0, 0.2, 1);
    }

    .ql-dot::before {
      content: '';
      position: absolute;
      top: -12px;
      bottom: -12px;
      left: -12px;
      right: -12px;
    }

    .ql-dot.active {
      width: 36px;
      border-radius: 9999px;
      background: var(--primary);
      box-shadow: 0 0 14px var(--primary-glow);
    }

    /* Modal Color Swatch Picker */
    .color-swatch-picker {
      display: flex;
      align-items: center;
      gap: 8px;
      margin: 8px 0 14px 0;
      flex-wrap: wrap;
    }

    .color-swatch-pill {
      width: 28px;
      height: 28px;
      border-radius: 50%;
      cursor: pointer;
      border: 2px solid transparent;
      transition: transform 0.15s ease, border-color 0.15s ease;
      box-shadow: 0 2px 6px rgba(0,0,0,0.3);
    }

    .color-swatch-pill.selected {
      border-color: #ffffff;
      transform: scale(1.15);
      box-shadow: 0 0 10px rgba(255, 255, 255, 0.6);
    }

    /* MOUSE TAB */
    #tab-mouse {
      width: 100%;
      height: 100%;
    }

    .mouse-wrapper {
      flex: 1;
      display: flex;
      flex-direction: column;
      gap: var(--row-gap);
      min-height: 0;
      width: 100%;
      height: 100%;
    }

    .trackpad-container {
      flex: 1;
      display: flex;
      background: #111827;
      border: 1px solid var(--border);
      border-radius: 18px;
      position: relative;
      touch-action: none;
      box-shadow: inset 0 2px 10px rgba(0,0,0,0.5);
      overflow: hidden;
      min-height: 0;
    }

    .trackpad {
      position: relative;
      overflow: hidden;
      flex: 1;
      height: 100%;
      touch-action: none;
      display: flex;
      align-items: center;
      justify-content: center;
      background-image: radial-gradient(rgba(255, 255, 255, 0.08) 1px, transparent 1px);
      background-size: 24px 24px;
    }

    .trackpad-hint {
      color: rgba(255,255,255,0.25);
      font-size: 13px;
      font-weight: 500;
      pointer-events: none;
    }

    .scroll-strip {
      width: 48px;
      height: 100%;
      background: rgba(30, 41, 59, 0.4);
      border-left: 1px solid var(--border);
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 16px;
      color: rgba(255,255,255,0.3);
      font-size: 11px;
      touch-action: none;
      transition: all 0.1s ease;
    }

    .scroll-strip:active,
    .scroll-strip.active {
      background: var(--primary-glow);
      color: var(--primary);
      box-shadow: inset 0 0 14px var(--primary-glow);
    }

    .mouse-buttons-row {
      display: flex;
      gap: var(--btn-gap);
      height: clamp(50px, 6.8vh, 64px);
      min-height: 46px;
      flex-shrink: 0;
    }

    .mouse-btn {
      position: relative;
      overflow: hidden;
      flex: 1;
      background: #1e293b;
      border: 1px solid var(--border);
      border-radius: clamp(12px, 2.8vw, 16px);
      color: #e2e8f0;
      font-size: 15px;
      font-weight: 600;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.08s ease;
    }

    .mouse-btn:active,
    .mouse-btn.btn-pressed {
      background: var(--primary);
      color: #0f172a;
      border-color: #ffffff;
      transform: scale(0.93);
      box-shadow: 0 0 22px var(--primary-glow), inset 0 0 8px rgba(255, 255, 255, 0.5);
    }

    /* SETTINGS TAB */
    .settings-wrapper {
      padding-right: 4px;
      display: flex;
      flex-direction: column;
      gap: 16px;
      padding-bottom: 90px;
      touch-action: pan-y !important;
    }

    .settings-group {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 18px;
    }

    .settings-group h2 {
      font-size: 15px;
      font-weight: 700;
      color: var(--primary);
      margin-bottom: 12px;
      display: flex;
      align-items: center;
      gap: 6px;
    }

    .setting-item {
      margin-bottom: 14px;
    }

    .setting-item:last-child {
      margin-bottom: 0;
    }

    .setting-item label {
      display: flex;
      justify-content: space-between;
      font-size: 13px;
      color: #cbd5e1;
      font-weight: 500;
      margin-bottom: 6px;
    }

    .setting-input, select {
      width: 100%;
      padding: 10px 12px;
      border-radius: 10px;
      background: #0f172a;
      border: 1px solid #334155;
      color: #fff;
      font-size: 14px;
      outline: none;
    }

    .primary-btn {
      position: relative;
      overflow: hidden;
      background: var(--btn-bg);
      color: #ffffff;
      border: 1.5px solid var(--btn-border);
      padding: 12px;
      border-radius: 12px;
      font-size: 15px;
      font-weight: 600;
      width: 100%;
      cursor: pointer;
      box-shadow: 0 2px 8px rgba(0, 0, 0, 0.25);
      transition: all 0.08s ease;
    }

    .primary-btn:active,
    .primary-btn.btn-pressed {
      background: var(--primary);
      color: #0f172a;
      border-color: #ffffff;
      transform: scale(0.96);
      box-shadow: 0 0 18px var(--primary-glow);
    }

    .danger-btn {
      position: relative;
      overflow: hidden;
      background: rgba(239, 68, 68, 0.15);
      border: 1px solid rgba(239, 68, 68, 0.4);
      color: #f87171;
      padding: 12px;
      border-radius: 12px;
      font-size: 14px;
      font-weight: 600;
      width: 100%;
      cursor: pointer;
      margin-top: 10px;
      transition: all 0.08s ease;
    }

    .danger-btn:active,
    .danger-btn.btn-pressed {
      background: rgba(239, 68, 68, 0.45);
      border-color: #fca5a5;
      color: #fff;
      transform: scale(0.96);
      box-shadow: 0 0 18px rgba(239, 68, 68, 0.75);
    }

    /* Toggle Switch */
    .switch {
      position: relative;
      display: inline-block;
      width: 48px;
      height: 26px;
      flex-shrink: 0;
    }
    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0; left: 0; right: 0; bottom: 0;
      background-color: #334155;
      transition: .2s;
      border-radius: 26px;
      border: 1px solid rgba(255, 255, 255, 0.12);
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 20px;
      width: 20px;
      left: 2px;
      bottom: 2px;
      background-color: #f1f5f9;
      transition: .2s;
      border-radius: 50%;
    }
    input:checked + .slider {
      background-color: var(--primary);
    }
    input:checked + .slider:before {
      transform: translateX(22px);
      background-color: #0f172a;
    }

    /* Progress bar for OTA */
    .progress-bar {
      width: 100%;
      height: 10px;
      background: #0f172a;
      border-radius: 5px;
      overflow: hidden;
      margin-top: 10px;
      border: 1px solid #334155;
      display: none;
    }

    .progress-fill {
      height: 100%;
      background: linear-gradient(90deg, var(--primary), #818cf8);
      width: 0%;
      transition: width 0.2s;
    }

    .ota-status {
      font-size: 12px;
      color: var(--primary);
      margin-top: 8px;
      min-height: 16px;
      text-align: center;
    }

    /* Floating Centered Modals */
    .modal-overlay {
      position: fixed;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: rgba(0, 0, 0, 0.72);
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
      display: none;
      align-items: center;
      justify-content: center;
      padding: 16px;
      z-index: 100;
    }

    .modal-overlay.open {
      display: flex;
    }

    .modal-sheet {
      width: 88%;
      max-width: 350px;
      background: linear-gradient(145deg, #1e293b, #151f30);
      border: 1.5px solid var(--border);
      border-radius: 22px;
      padding: 22px 20px;
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.75), 0 0 0 1px rgba(255, 255, 255, 0.08);
      animation: modalPop 0.18s cubic-bezier(0.16, 1, 0.3, 1);
      max-height: 85vh;
      overflow-y: auto;
      -webkit-overflow-scrolling: touch;
    }

    @keyframes modalPop {
      from {
        opacity: 0;
        transform: scale(0.92);
      }
      to {
        opacity: 1;
        transform: scale(1);
      }
    }

    .modal-header {
      display: flex;
      align-items: center;
      margin-bottom: 14px;
    }

    .modal-header h3 {
      font-size: 20px;
      font-weight: 700;
      letter-spacing: -0.3px;
      color: #f8fafc;
    }

    .modal-sheet input {
      width: 100%;
      padding: 14px;
      border-radius: 12px;
      border: 1px solid #475569;
      background: #0f172a;
      color: #fff;
      font-size: 16px;
      margin-bottom: 12px;
      outline: none;
    }

    /* Enhanced Keyboard Modal */
    #kbModal {
      align-items: flex-start;
      padding-top: max(14px, env(safe-area-inset-top));
    }
    @media (min-height: 760px) and (min-width: 768px) {
      #kbModal {
        align-items: center;
      }
    }
    .kb-sheet {
      max-width: 380px !important;
      width: 94% !important;
      padding: 16px !important;
    }
    .kb-mode-switch {
      display: inline-flex;
      background: rgba(15, 23, 42, 0.75);
      border: 1px solid var(--border);
      border-radius: 999px;
      padding: 2px;
      gap: 2px;
    }
    .kb-mode-pill {
      border: none;
      background: transparent;
      color: #94a3b8;
      font-size: 12px;
      font-weight: 700;
      padding: 5px 10px;
      border-radius: 999px;
      cursor: pointer;
      transition: all 0.15s ease;
    }
    .kb-mode-pill.active {
      background: var(--btn-bg);
      color: #ffffff;
      box-shadow: 0 0 10px var(--primary-glow);
    }
    .kb-input-wrap {
      position: relative;
      display: flex;
      align-items: center;
      margin-bottom: 6px;
    }
    .kb-input-wrap input {
      margin-bottom: 0 !important;
      padding: 11px 36px 11px 12px !important;
      font-size: 16px !important;
      border: 1.5px solid var(--border) !important;
      transition: border-color 0.15s ease, box-shadow 0.15s ease;
    }
    .kb-input-wrap input:focus {
      border-color: var(--primary) !important;
      box-shadow: 0 0 12px var(--primary-glow);
    }
    .kb-input-clear {
      position: absolute;
      right: 8px;
      width: 24px;
      height: 24px;
      border-radius: 50%;
      border: none;
      background: rgba(148, 163, 184, 0.2);
      color: #cbd5e1;
      font-size: 12px;
      display: none;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      padding: 0;
    }
    .kb-hint {
      font-size: 11px;
      color: #94a3b8;
      margin-bottom: 10px;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .kb-live-dot {
      width: 7px;
      height: 7px;
      border-radius: 50%;
      background: #22c55e;
      box-shadow: 0 0 8px #22c55e;
      display: inline-block;
      animation: pulse 1.4s infinite;
    }
    .kb-tools-row {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 6px;
      margin-bottom: 8px;
    }
    .kb-tool-btn {
      background: rgba(30, 41, 59, 0.85);
      border: 1px solid var(--border);
      color: #f1f5f9;
      border-radius: 10px;
      padding: 9px 4px;
      font-size: 12px;
      font-weight: 600;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 4px;
      user-select: none;
      -webkit-user-select: none;
      transition: all 0.08s ease;
    }
    .kb-tool-btn:active {
      background: var(--primary);
      color: #0f172a;
      transform: scale(0.95);
    }
    .kb-tool-btn.kb-danger {
      color: #fca5a5;
      border-color: rgba(239, 68, 68, 0.35);
      background: rgba(239, 68, 68, 0.12);
    }
    .kb-tool-btn.kb-primary {
      background: var(--btn-bg);
      border-color: var(--btn-border);
      color: #ffffff;
    }
    .kb-chips-row {
      display: flex;
      gap: 5px;
      overflow-x: auto;
      padding-bottom: 4px;
      margin-bottom: 8px;
      scrollbar-width: none;
      -webkit-overflow-scrolling: touch;
    }
    .kb-chips-row::-webkit-scrollbar {
      display: none;
    }
    .kb-chip {
      flex-shrink: 0;
      background: rgba(15, 23, 42, 0.75);
      border: 1px solid rgba(148, 163, 184, 0.28);
      color: #cbd5e1;
      border-radius: 8px;
      padding: 5px 9px;
      font-size: 12px;
      font-family: monospace;
      font-weight: 600;
      cursor: pointer;
      user-select: none;
      transition: all 0.08s ease;
    }
    .kb-chip:active {
      background: var(--primary);
      color: #0f172a;
      border-color: var(--primary);
      transform: scale(0.94);
    }
    .kb-nav-row {
      display: grid;
      grid-template-columns: 1.1fr 1fr 1fr 1fr 1fr 1.1fr;
      gap: 5px;
      padding-top: 8px;
      border-top: 1px solid rgba(255, 255, 255, 0.08);
    }
    .kb-nav-btn {
      background: rgba(15, 23, 42, 0.65);
      border: 1px solid var(--border);
      color: #cbd5e1;
      border-radius: 9px;
      padding: 8px 0;
      font-size: 12px;
      font-weight: 700;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      user-select: none;
      transition: all 0.08s ease;
    }
    .kb-nav-btn:active {
      background: var(--primary);
      color: #0f172a;
      transform: scale(0.95);
    }
    .kb-nav-btn.kb-ok {
      background: var(--btn-bg);
      border-color: var(--btn-border);
      color: #fff;
    }
    html[data-theme-mode="light"] .kb-mode-switch {
      background: #f1f5f9;
    }
    html[data-theme-mode="light"] .kb-tool-btn,
    html[data-theme-mode="light"] .kb-chip,
    html[data-theme-mode="light"] .kb-nav-btn {
      background: #f8fafc;
      color: #1e293b;
      border-color: var(--border);
    }
    html[data-theme-mode="light"] .kb-tool-btn.kb-danger {
      background: rgba(239, 68, 68, 0.1);
      color: #dc2626;
      border-color: rgba(239, 68, 68, 0.35);
    }
    html[data-theme-mode="light"] .kb-tool-btn.kb-primary,
    html[data-theme-mode="light"] .kb-nav-btn.kb-ok {
      background: var(--btn-bg);
      color: #ffffff;
    }

    /* Toast Notification */
    .toast {
      position: fixed;
      bottom: calc(24px + env(safe-area-inset-bottom));
      left: 50%;
      transform: translateX(-50%) translateY(30px);
      background: rgba(15, 23, 42, 0.95);
      border: 1px solid var(--accent);
      color: #f8fafc;
      padding: 10px 20px;
      border-radius: 9999px;
      font-size: 14px;
      font-weight: 600;
      box-shadow: 0 10px 25px rgba(0,0,0,0.6);
      opacity: 0;
      pointer-events: none;
      transition: transform 0.25s ease, opacity 0.25s ease;
      z-index: 120;
    }
    .toast.show {
      opacity: 1;
      transform: translateX(-50%) translateY(0);
    }

    /* Long-press visual cue */
    .long-pressing {
      transform: scale(0.92) !important;
      box-shadow: 0 0 16px var(--primary-glow) !important;
      border-color: var(--primary) !important;
      transition: transform 0.5s cubic-bezier(0.2, 0.8, 0.2, 1), box-shadow 0.5s ease, border-color 0.5s ease !important;
    }

    /* Button Config Modal */
    .btn-cfg-swatch {
      display: inline-block;
      width: 14px;
      height: 14px;
      border-radius: 50%;
      margin-right: 8px;
      vertical-align: middle;
      border: 1px solid rgba(255,255,255,0.3);
    }
    .modal-btn-row {
      display: flex;
      gap: 8px;
      margin-top: 14px;
    }
    .modal-btn-row button {
      flex: 1;
      margin: 0;
    }
    .btn-secondary {
      background: #334155 !important;
      border: 1px solid var(--border) !important;
      color: #f8fafc !important;
      padding: 12px;
      border-radius: 12px;
      font-size: 15px;
      font-weight: 600;
      cursor: pointer;
    }
    .btn-secondary:active {
      transform: scale(0.96);
    }
    .btn-ghost-danger {
      background: transparent !important;
      border: 1px solid #ef4444 !important;
      color: #ef4444 !important;
      padding: 12px;
      border-radius: 12px;
      font-size: 15px;
      font-weight: 600;
      cursor: pointer;
    }
    .btn-ghost-danger:active {
      background: rgba(239, 68, 68, 0.2) !important;
      transform: scale(0.96);
    }

    /* Appearance Mode Segmented Toggle */
    .mode-toggle-group {
      display: flex;
      gap: 8px;
      margin-top: 6px;
    }
    .mode-toggle-btn {
      flex: 1;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 10px 14px;
      border-radius: 12px;
      font-size: 14px;
      font-weight: 600;
      background: rgba(30, 41, 59, 0.6);
      border: 1.5px solid var(--border);
      color: var(--text-muted);
      cursor: pointer;
      transition: all 0.15s ease;
    }
    .mode-toggle-btn.active {
      background: var(--btn-bg);
      border-color: var(--btn-border);
      color: #ffffff;
      box-shadow: 0 0 12px var(--primary-glow);
    }

    /* Full Light Mode Overrides */
    html[data-theme-mode="light"] {
      --bg: #f8fafc;
      --card-bg: rgba(255, 255, 255, 0.95);
      --glass: rgba(0, 0, 0, 0.04);
      --text: #0f172a;
      --text-muted: #64748b;
    }

    html[data-theme-mode="light"] body {
      background: #e2e8f0;
      color: var(--text);
    }

    @media (min-width: 480px) and (min-height: 600px) {
      html[data-theme-mode="light"] body {
        background: radial-gradient(circle at 50% 20%, #f1f5f9 0%, #cbd5e1 100%);
      }
      html[data-theme-mode="light"] .phone-frame {
        border: 2px solid var(--border);
        box-shadow: 0 28px 75px rgba(0, 0, 0, 0.15), 0 0 0 1px rgba(0, 0, 0, 0.05);
      }
    }

    html[data-theme-mode="light"] .tabs {
      background: rgba(255, 255, 255, 0.9);
      border: 1px solid var(--border);
      box-shadow: 0 2px 10px rgba(0, 0, 0, 0.04);
    }

    html[data-theme-mode="light"] .tab-btn {
      background: rgba(241, 245, 249, 0.85);
      border: 1px solid var(--border);
      color: #64748b;
    }

    html[data-theme-mode="light"] .tab-btn.active {
      color: #0f172a;
    }

    html[data-theme-mode="light"] .dpad {
      background: radial-gradient(circle at 50% 50%, #ffffff 0%, #e2e8f0 100%);
      border: 2px solid var(--border);
      box-shadow: 0 16px 36px rgba(0, 0, 0, 0.1), inset 0 2px 0 rgba(255, 255, 255, 0.9);
    }

    html[data-theme-mode="light"] .dpad-btn {
      background: #ffffff;
      border: 1px solid var(--border);
      color: #1e293b;
      box-shadow: 0 3px 8px rgba(0, 0, 0, 0.08);
    }

    html[data-theme-mode="light"] .dpad-aux {
      background: rgba(241, 245, 249, 0.7);
      border: 1px solid var(--border);
      color: #64748b;
      box-shadow: 0 2px 5px rgba(0, 0, 0, 0.04);
    }

    html[data-theme-mode="light"] .dpad-aux.configured {
      background: #ffffff;
      border-color: var(--border);
      color: #0f172a;
      box-shadow: 0 3px 8px rgba(0, 0, 0, 0.08);
    }

    html[data-theme-mode="light"] .dpad-center {
      background: linear-gradient(135deg, #ffffff, #f1f5f9);
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
    }

    html[data-theme-mode="light"] .nav-btn {
      background: #ffffff;
      border: 1px solid var(--border);
      color: #1e293b;
      box-shadow: 0 2px 6px rgba(0, 0, 0, 0.05);
    }

    html[data-theme-mode="light"] .trackpad-container {
      background: #ffffff;
      border: 1px solid var(--border);
      box-shadow: inset 0 2px 8px rgba(0, 0, 0, 0.06);
    }

    html[data-theme-mode="light"] .trackpad {
      background-image: radial-gradient(rgba(0, 0, 0, 0.12) 1px, transparent 1px);
    }

    html[data-theme-mode="light"] .trackpad-hint {
      color: rgba(0, 0, 0, 0.35);
    }

    html[data-theme-mode="light"] .scroll-strip {
      background: rgba(241, 245, 249, 0.85);
      border-left: 1px solid var(--border);
      color: rgba(0, 0, 0, 0.4);
    }

    html[data-theme-mode="light"] .mouse-btn {
      background: #ffffff;
      border: 1px solid var(--border);
      color: #0f172a;
      box-shadow: 0 2px 6px rgba(0, 0, 0, 0.05);
    }

    html[data-theme-mode="light"] .ql-btn {
      background: #ffffff;
      border-color: var(--border);
      border-top-color: var(--border);
      color: #0f172a;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.06);
    }

    html[data-theme-mode="light"] .ql-btn.unconfigured {
      background: rgba(241, 245, 249, 0.6);
      border-color: var(--border);
    }

    html[data-theme-mode="light"] .ql-btn.unconfigured .ql-slot-num {
      color: rgba(0, 0, 0, 0.3);
    }

    html[data-theme-mode="light"] .ql-btn-badge {
      color: #64748b;
    }

    html[data-theme-mode="light"] .ql-dot {
      background: #cbd5e1;
    }

    html[data-theme-mode="light"] .settings-item label,
    html[data-theme-mode="light"] .setting-item label {
      color: #475569;
    }

    html[data-theme-mode="light"] .setting-input,
    html[data-theme-mode="light"] select,
    html[data-theme-mode="light"] .settings-item input[type="text"],
    html[data-theme-mode="light"] .modal-sheet input {
      background: #f8fafc;
      color: #0f172a;
      border: 1px solid var(--border);
    }

    html[data-theme-mode="light"] .modal-sheet {
      background: #ffffff;
      border: 1.5px solid var(--border);
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.2), 0 0 0 1px rgba(0, 0, 0, 0.05);
    }

    html[data-theme-mode="light"] .modal-header h3 {
      color: #0f172a;
    }

    html[data-theme-mode="light"] .btn-secondary {
      background: #f1f5f9;
      color: #334155;
      border: 1px solid var(--border);
    }

    html[data-theme-mode="light"] .danger-btn {
      background: rgba(239, 68, 68, 0.1);
      border: 1.5px solid rgba(239, 68, 68, 0.4);
      color: #dc2626;
    }

    html[data-theme-mode="light"] .toast {
      background: rgba(255, 255, 255, 0.96);
      color: #0f172a;
      border: 1px solid var(--primary);
      box-shadow: 0 10px 25px rgba(0, 0, 0, 0.15);
    }

    html[data-theme-mode="light"] .modal-about,
    html[data-theme-mode="light"] .modal-help {
      color: #334155;
    }

    html[data-theme-mode="light"] .modal-about strong,
    html[data-theme-mode="light"] .modal-help strong {
      color: #0f172a;
    }

    html[data-theme-mode="light"] .modal-about code,
    html[data-theme-mode="light"] .modal-help code {
      background: #f1f5f9;
      color: #0f172a;
      border-color: #cbd5e1;
    }

    html[data-theme-mode="light"] .mode-toggle-btn {
      background: #f1f5f9;
      border-color: var(--border);
      color: #64748b;
    }

    html[data-theme-mode="light"] .mode-toggle-btn.active {
      background: var(--btn-bg);
      border-color: var(--btn-border);
      color: #ffffff;
      box-shadow: 0 0 12px var(--primary-glow);
    }
    /* Floating Desktop Tools (Quick Edit & PC Keyboard Passthrough) */
    .desktop-tools {
      display: none;
      flex-direction: row;
      align-items: center;
      justify-content: center;
      gap: 12px;
      margin-bottom: 14px;
      z-index: 90;
      flex-shrink: 0;
    }
    @media (min-width: 768px) {
      .desktop-tools {
        display: flex;
      }
      .phone-frame {
        max-height: min(874px, calc(100dvh - 84px));
      }
    }
    .desktop-quick-edit-btn {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 0 16px;
      height: 38px;
      border-radius: 9999px;
      font-size: 13px;
      font-weight: 700;
      background: var(--card-bg);
      border: 1.5px solid var(--border);
      color: var(--primary);
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.45);
      cursor: pointer;
      transition: all 0.15s ease;
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      box-sizing: border-box;
    }
    .desktop-quick-edit-btn:hover {
      transform: translateY(-2px);
      border-color: var(--primary);
      box-shadow: 0 10px 28px var(--primary-glow);
    }
    .desktop-quick-edit-btn:active {
      transform: translateY(0) scale(0.96);
    }
    .desktop-kb-card {
      display: inline-flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      padding: 0 14px;
      height: 38px;
      border-radius: 9999px;
      background: var(--card-bg);
      border: 1.5px solid var(--border);
      color: #94a3b8;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.45);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      transition: all 0.15s ease;
      user-select: none;
      box-sizing: border-box;
    }
    .desktop-kb-card.active {
      color: var(--primary);
      border-color: var(--primary);
      box-shadow: 0 0 16px var(--primary-glow);
    }

    /* Batch Edit Sheet (Desktop Modal) */
    .batch-edit-sheet {
      width: 95% !important;
      max-width: 860px !important;
      height: 84vh !important;
      max-height: 84vh !important;
      display: flex;
      flex-direction: column;
    }
    .batch-edit-table-wrap {
      flex: 1;
      overflow-y: auto;
      border: 1.5px solid var(--border);
      border-radius: 14px;
      background: rgba(15, 23, 42, 0.45);
      margin-top: 4px;
    }
    html[data-theme-mode="light"] .batch-edit-table-wrap {
      background: rgba(248, 250, 252, 0.8);
    }
    .batch-edit-table {
      width: 100%;
      border-collapse: collapse;
      font-size: 13px;
    }
    .batch-edit-table th {
      position: sticky;
      top: 0;
      background: var(--card-bg);
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
      padding: 10px 14px;
      text-align: left;
      font-weight: 700;
      color: var(--text-muted);
      border-bottom: 1.5px solid var(--border);
      z-index: 5;
    }
    .batch-edit-table td {
      padding: 8px 12px;
      border-bottom: 1px solid var(--border);
      vertical-align: middle;
    }
    .batch-edit-table tr:hover {
      background: rgba(255, 255, 255, 0.04);
    }
    html[data-theme-mode="light"] .batch-edit-table tr:hover {
      background: rgba(0, 0, 0, 0.03);
    }
    .batch-input {
      width: 100%;
      padding: 8px 12px;
      border-radius: 8px;
      border: 1.5px solid var(--border);
      background: rgba(15, 23, 42, 0.6);
      color: var(--text);
      font-size: 13px;
      font-weight: 500;
      outline: none;
    }
    html[data-theme-mode="light"] .batch-input {
      background: #ffffff;
    }
    .batch-input:focus {
      border-color: var(--primary);
    }
    .batch-color-select {
      width: 100%;
      padding: 8px 10px;
      border-radius: 8px;
      border: 1.5px solid var(--border);
      background: rgba(15, 23, 42, 0.6);
      color: var(--text);
      font-size: 12px;
      outline: none;
      cursor: pointer;
    }
    html[data-theme-mode="light"] .batch-color-select {
      background: #ffffff;
    }
    .batch-clear-btn {
      background: transparent;
      border: 1px solid rgba(239, 68, 68, 0.4);
      color: #ef4444;
      padding: 6px 10px;
      border-radius: 6px;
      font-size: 12px;
      font-weight: 600;
      cursor: pointer;
    }
    .batch-clear-btn:hover {
      background: rgba(239, 68, 68, 0.15);
    }
  </style>
</head>
<body>
  <!-- Floating Desktop Tools (Quick Edit & PC Keyboard Passthrough) -->
  <div class="desktop-tools">
    <button id="btnDesktopQuickEdit" class="desktop-quick-edit-btn" onclick="openBatchQuickEditModal()" title="Batch edit all Quick Links">
      <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
        <path d="M12 20h9"/>
        <path d="M16.5 3.5a2.121 2.121 0 0 1 3 3L7 19l-4 1 1-4L16.5 3.5z"/>
      </svg>
      <span>Quick Edit</span>
    </button>
    <div class="desktop-kb-card" id="desktopKbCard" title="Control your TV using your computer's keyboard (Arrow keys, Space, Enter, Backspace, Esc)">
      <div style="display:flex; align-items:center; gap:6px;">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <rect x="2" y="4" width="20" height="16" rx="2.5" />
          <path d="M6 8h.01M10 8h.01M14 8h.01M18 8h.01M6 12h.01M10 12h.01M14 12h.01M18 12h.01M7 16h10" />
        </svg>
        <span style="font-size:12px; font-weight:700;">PC Keys</span>
      </div>
      <label class="switch" style="transform: scale(0.68); margin: 0;">
        <input type="checkbox" id="desktopKbToggle" onchange="toggleKeyboardPassthrough(this.checked)">
        <span class="slider"></span>
      </label>
    </div>
  </div>

  <div class="phone-frame">

  <!-- Top App Header -->
  <header>
    <div class="brand" onclick="refreshApp()" style="cursor: pointer;" title="Tap to refresh">
      <h1>CastPad</h1>
    </div>
    <div class="badge" id="bleBadge">
      <span class="dot"></span>
      <span id="bleText">BLE: Connecting...</span>
    </div>
  </header>

  <!-- Navigation Tabs (Icon-only: Settings, Quick Links, Mouse, Remote) -->
  <div class="tabs">
    <button class="tab-btn tab-settings" onclick="switchTab('settings', this)" title="Settings" aria-label="Settings">
      <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <circle cx="12" cy="12" r="3" />
        <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z" />
      </svg>
    </button>
    <button class="tab-btn tab-ql" onclick="switchTab('quicklinks', this)" title="Quick Links" aria-label="Quick Links">
      <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <rect x="3" y="3" width="7" height="7" rx="1.5" />
        <rect x="14" y="3" width="7" height="7" rx="1.5" />
        <rect x="3" y="14" width="7" height="7" rx="1.5" />
        <rect x="14" y="14" width="7" height="7" rx="1.5" />
      </svg>
    </button>
    <button class="tab-btn tab-mouse" onclick="switchTab('mouse', this)" title="Mouse &amp; Trackpad" aria-label="Mouse">
      <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <rect x="6" y="2" width="12" height="20" rx="6" />
        <line x1="12" y1="6" x2="12" y2="10" />
      </svg>
    </button>
    <button class="tab-btn tab-remote active" onclick="switchTab('remote', this)" title="Remote" aria-label="Remote">
      <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <rect x="6" y="2" width="12" height="20" rx="3" />
        <circle cx="12" cy="9" r="2.5" />
        <path d="M10 15h.01M14 15h.01M10 18h.01M14 18h.01" />
      </svg>
    </button>
  </div>

  <!-- TAB 1: REMOTE -->
  <div class="tab-content active" id="tab-remote">
    <!-- Navigation Row Above D-Pad: Back, Home, Keyboard (Anchored to top, matching Mouse Tab) -->
    <div class="nav-row">
      <button class="nav-btn" onclick="sendAction('BACK')">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="15 18 9 12 15 6"/></svg>
        Back
      </button>
      <button class="nav-btn" onclick="sendAction('HOME')">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/></svg>
        Home
      </button>
      <button class="nav-btn" onclick="openKeyboardModal()" title="Keyboard Input">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <rect x="2" y="4" width="20" height="16" rx="2.5" />
          <path d="M6 8h.01M10 8h.01M14 8h.01M18 8h.01M6 12h.01M10 12h.01M14 12h.01M18 12h.01M7 16h10" />
        </svg>
        Keyboard
      </button>
    </div>

    <!-- Directional Pad (Square 3x3 Grid with Info/EPG & CH+/CH-) -->
    <div class="dpad-container">
      <div class="dpad">
        <!-- Row 1: Custom 1 (above Left), Up, Custom 2 (above Right) -->
        <button class="dpad-aux" id="aux-btn-custom1" onclick="sendColorBtn('custom1')" title="Custom 1">
          <span class="dpad-aux-label" id="lbl-custom1"></span>
        </button>
        <button class="dpad-btn dpad-up" onclick="sendAction('UP')" aria-label="Up">
          <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.6"><polyline points="18 15 12 9 6 15"/></svg>
        </button>
        <button class="dpad-aux" id="aux-btn-custom2" onclick="sendColorBtn('custom2')" title="Custom 2">
          <span class="dpad-aux-label" id="lbl-custom2"></span>
        </button>

        <!-- Row 2: Left, OK, Right -->
        <button class="dpad-btn dpad-left" onclick="sendAction('LEFT')" aria-label="Left">
          <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.6"><polyline points="15 18 9 12 15 6"/></svg>
        </button>
        <button class="dpad-center" onclick="sendAction('ENTER')">OK</button>
        <button class="dpad-btn dpad-right" onclick="sendAction('RIGHT')" aria-label="Right">
          <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.6"><polyline points="9 18 15 12 9 6"/></svg>
        </button>

        <!-- Row 3: Custom 3 (below Left), Down, Custom 4 (below Right) -->
        <button class="dpad-aux" id="aux-btn-custom3" onclick="sendColorBtn('custom3')" title="Custom 3">
          <span class="dpad-aux-label" id="lbl-custom3"></span>
        </button>
        <button class="dpad-btn dpad-down" onclick="sendAction('DOWN')" aria-label="Down">
          <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.6"><polyline points="6 9 12 15 18 9"/></svg>
        </button>
        <button class="dpad-aux" id="aux-btn-custom4" onclick="sendColorBtn('custom4')" title="Custom 4">
          <span class="dpad-aux-label" id="lbl-custom4"></span>
        </button>
      </div>
    </div>

    <!-- Media Row Below D-Pad: Rewind, Play/Pause, Fast Forward -->
    <div class="nav-row">
      <button class="nav-btn" onclick="sendAction('REWIND')">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linejoin="round"><polygon points="11 19 2 12 11 5 11 19"/><polygon points="22 19 13 12 22 5 22 19"/></svg>
        Rewind
      </button>
      <button class="nav-btn" onclick="sendAction('PLAY_PAUSE')">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linejoin="round"><polygon points="5 5 14 12 5 19 5 5"/><line x1="17" y1="5" x2="17" y2="19"/><line x1="21" y1="5" x2="21" y2="19"/></svg>
        Play/Pause
      </button>
      <button class="nav-btn" onclick="sendAction('FAST_FORWARD')">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linejoin="round"><polygon points="13 19 22 12 13 5 13 19"/><polygon points="2 19 11 12 2 5 2 19"/></svg>
        Fast Fwd
      </button>
    </div>

    <!-- 4 Colored Buttons (No dots) -->
    <div class="color-row">
      <button class="color-btn btn-red" onclick="sendColorBtn('red')">
        <span class="color-label" id="lbl-red"></span>
      </button>
      <button class="color-btn btn-green" onclick="sendColorBtn('green')">
        <span class="color-label" id="lbl-green"></span>
      </button>
      <button class="color-btn btn-yellow" onclick="sendColorBtn('yellow')">
        <span class="color-label" id="lbl-yellow"></span>
      </button>
      <button class="color-btn btn-blue" onclick="sendColorBtn('blue')">
        <span class="color-label" id="lbl-blue"></span>
      </button>
    </div>
  </div>

  <!-- TAB 2: QUICK LINKS (3 wide x 4 high Grid with Paging) -->
  <div class="tab-content" id="tab-quicklinks">
    <div class="ql-wrapper">
      <div class="ql-grid" id="qlGrid">
        <button class="ql-btn" id="ql-btn-0" onclick="onQuickLinkClick(0)"><span class="ql-btn-label" id="ql-lbl-0"></span><span class="ql-btn-badge" id="ql-badge-0"></span></button>
        <button class="ql-btn" id="ql-btn-1" onclick="onQuickLinkClick(1)"><span class="ql-btn-label" id="ql-lbl-1"></span><span class="ql-btn-badge" id="ql-badge-1"></span></button>
        <button class="ql-btn" id="ql-btn-2" onclick="onQuickLinkClick(2)"><span class="ql-btn-label" id="ql-lbl-2"></span><span class="ql-btn-badge" id="ql-badge-2"></span></button>
        <button class="ql-btn" id="ql-btn-3" onclick="onQuickLinkClick(3)"><span class="ql-btn-label" id="ql-lbl-3"></span><span class="ql-btn-badge" id="ql-badge-3"></span></button>
        <button class="ql-btn" id="ql-btn-4" onclick="onQuickLinkClick(4)"><span class="ql-btn-label" id="ql-lbl-4"></span><span class="ql-btn-badge" id="ql-badge-4"></span></button>
        <button class="ql-btn" id="ql-btn-5" onclick="onQuickLinkClick(5)"><span class="ql-btn-label" id="ql-lbl-5"></span><span class="ql-btn-badge" id="ql-badge-5"></span></button>
        <button class="ql-btn" id="ql-btn-6" onclick="onQuickLinkClick(6)"><span class="ql-btn-label" id="ql-lbl-6"></span><span class="ql-btn-badge" id="ql-badge-6"></span></button>
        <button class="ql-btn" id="ql-btn-7" onclick="onQuickLinkClick(7)"><span class="ql-btn-label" id="ql-lbl-7"></span><span class="ql-btn-badge" id="ql-badge-7"></span></button>
        <button class="ql-btn" id="ql-btn-8" onclick="onQuickLinkClick(8)"><span class="ql-btn-label" id="ql-lbl-8"></span><span class="ql-btn-badge" id="ql-badge-8"></span></button>
        <button class="ql-btn" id="ql-btn-9" onclick="onQuickLinkClick(9)"><span class="ql-btn-label" id="ql-lbl-9"></span><span class="ql-btn-badge" id="ql-badge-9"></span></button>
        <button class="ql-btn" id="ql-btn-10" onclick="onQuickLinkClick(10)"><span class="ql-btn-label" id="ql-lbl-10"></span><span class="ql-btn-badge" id="ql-badge-10"></span></button>
        <button class="ql-btn" id="ql-btn-11" onclick="onQuickLinkClick(11)"><span class="ql-btn-label" id="ql-lbl-11"></span><span class="ql-btn-badge" id="ql-badge-11"></span></button>
      </div>
      <div class="ql-pagination" id="qlPagination"></div>
    </div>
  </div>

  <!-- TAB 3: MOUSE -->
  <div class="tab-content" id="tab-mouse">
    <div class="mouse-wrapper">
      <!-- Navigation Row Above Trackpad: Back, Home, Keyboard -->
      <div class="nav-row">
        <button class="nav-btn" onclick="sendAction('BACK')">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="15 18 9 12 15 6"/></svg>
          Back
        </button>
        <button class="nav-btn" onclick="sendAction('HOME')">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/></svg>
          Home
        </button>
        <button class="nav-btn" onclick="openKeyboardModal()" title="Keyboard Input">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <rect x="2" y="4" width="20" height="16" rx="2.5" />
            <path d="M6 8h.01M10 8h.01M14 8h.01M18 8h.01M6 12h.01M10 12h.01M14 12h.01M18 12h.01M7 16h10" />
          </svg>
          Keyboard
        </button>
      </div>
      <div class="trackpad-container">
        <div class="trackpad" id="trackpad">
          <span class="trackpad-hint">Slide to move &bull; Tap to click</span>
        </div>
        <div class="scroll-strip" id="scrollStrip">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="18 15 12 9 6 15"/></svg>
          <span>SCROLL</span>
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="6 9 12 15 18 9"/></svg>
        </div>
      </div>
      <div class="mouse-buttons-row">
        <button class="mouse-btn" id="btnMouseLeft">Left Click</button>
        <button class="mouse-btn" id="btnMouseRight">Right Click</button>
      </div>
    </div>
  </div>

  <!-- TAB 4: SETTINGS -->
  <div class="tab-content" id="tab-settings">
    <div class="settings-wrapper">

      <!-- Top Actions (About, Help & Add to Home Screen) -->
      <div class="settings-group">
        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
          <button class="primary-btn" onclick="openAboutModal()" style="display: flex; align-items: center; justify-content: center; gap: 8px; padding: 11px 8px;">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
              <circle cx="12" cy="12" r="10" />
              <line x1="12" y1="16" x2="12" y2="12" />
              <line x1="12" y1="8" x2="12.01" y2="8" />
            </svg>
            <span>About</span>
          </button>
          <button class="primary-btn" onclick="openHelpModal()" style="display: flex; align-items: center; justify-content: center; gap: 8px; padding: 11px 8px;">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
              <circle cx="12" cy="12" r="10" />
              <path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3" />
              <line x1="12" y1="17" x2="12.01" y2="17" />
            </svg>
            <span>Help</span>
          </button>
        </div>
        <button id="btnAddToHome" class="primary-btn" onclick="onAddToHomeScreen()" style="margin-top: 10px; display: none; align-items: center; justify-content: center; gap: 8px;">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
            <rect x="5" y="2" width="14" height="20" rx="2" ry="2" />
            <line x1="12" y1="18" x2="12.01" y2="18" />
            <line x1="12" y1="8" x2="12" y2="14" />
            <polyline points="9 11 12 8 15 11" />
          </svg>
          <span>Add to Home Screen</span>
        </button>
      </div>

      <!-- 1. Appearance & Remote -->
      <div class="settings-group">
        <h2>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <circle cx="12" cy="12" r="3" />
            <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z" />
          </svg>
          Appearance &amp; Remote
        </h2>

        <div class="setting-item">
          <label>Theme Accent Color</label>
          <div class="color-swatch-picker" id="themePicker" style="margin: 6px 0 0 0;">
            <div class="color-swatch-pill selected" data-theme="cyan" style="background:#06b6d4;" title="Cyan Electric" onclick="setAppTheme('cyan')"></div>
            <div class="color-swatch-pill" data-theme="grey" style="background:#94a3b8;" title="Slate Grey" onclick="setAppTheme('grey')"></div>
            <div class="color-swatch-pill" data-theme="sky" style="background:#38bdf8;" title="Sky Blue" onclick="setAppTheme('sky')"></div>
            <div class="color-swatch-pill" data-theme="green" style="background:#34d399;" title="Emerald Green" onclick="setAppTheme('green')"></div>
            <div class="color-swatch-pill" data-theme="purple" style="background:#a78bfa;" title="Violet Purple" onclick="setAppTheme('purple')"></div>
            <div class="color-swatch-pill" data-theme="amber" style="background:#fbbf24;" title="Amber Gold" onclick="setAppTheme('amber')"></div>
            <div class="color-swatch-pill" data-theme="rose" style="background:#fb7185;" title="Rose Red" onclick="setAppTheme('rose')"></div>
          </div>
        </div>

        <div class="setting-item" style="margin-top: 14px;">
          <label>Appearance Mode</label>
          <div class="mode-toggle-group">
            <button type="button" class="mode-toggle-btn active" id="btnModeDark" onclick="setThemeMode('dark')">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="vertical-align: -2px; margin-right: 6px;"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"></path></svg>
              Dark
            </button>
            <button type="button" class="mode-toggle-btn" id="btnModeLight" onclick="setThemeMode('light')">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="vertical-align: -2px; margin-right: 6px;"><circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/></svg>
              Light
            </button>
          </div>
        </div>

        <div class="setting-item" style="margin-top: 14px;">
          <label>Trackpad Sensitivity: <span id="sensValue" style="color: var(--primary); font-weight: 700;">1.3x</span></label>
          <input type="range" id="cfg-sens" min="0.5" max="3.0" step="0.1" value="1.3" style="width: 100%; accent-color: var(--primary); cursor: pointer; margin-top: 4px;" oninput="onSensSliderChange(this.value)">
        </div>

        <div class="setting-item" style="margin-top: 14px;">
          <label>Quick Links Pages (12 links per page)</label>
          <select id="cfg-ql-pages" class="setting-input" onchange="onQlPagesSettingChange()">
            <option value="1">1 Page (12 Links)</option>
            <option value="2">2 Pages (24 Links)</option>
            <option value="3" selected>3 Pages (36 Links)</option>
            <option value="4">4 Pages (48 Links)</option>
            <option value="5">5 Pages (60 Links)</option>
          </select>
        </div>
      </div>

      <!-- 2. Hardware & Power -->
      <div class="settings-group">
        <h2>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2" />
          </svg>
          Hardware &amp; Power
        </h2>

        <div class="setting-item" style="display: flex; align-items: center; justify-content: space-between;">
          <div>
            <label style="margin: 0; font-weight: 600;">Disable Status LED</label>
            <span style="font-size: 11px; color: #94a3b8; display: block;">Turn off the onboard RGB &amp; status activity LED</span>
          </div>
          <label class="switch">
            <input type="checkbox" id="cfg-disable-led" onchange="onDisableLedToggle(this)">
            <span class="slider"></span>
          </label>
        </div>

        <div class="setting-item" style="margin-top: 14px; display: flex; align-items: center; justify-content: space-between;">
          <div>
            <label style="margin: 0; font-weight: 600;">Power Saving Mode</label>
            <span style="font-size: 11px; color: #94a3b8; display: block;">Max modem sleep, 80 MHz lock, low Wi-Fi TX &amp; 60s LED sleep</span>
          </div>
          <label class="switch">
            <input type="checkbox" id="cfg-power-save" onchange="onPowerSaveToggle(this)">
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <!-- 3. Controller & Bluetooth Settings -->
      <div class="settings-group">
        <h2>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <polyline points="6.5 6.5 17.5 17.5 12 23 12 1 17.5 6.5 6.5 17.5" />
          </svg>
          Controller &amp; Bluetooth
        </h2>
        <div class="setting-item" style="margin-bottom: 12px;">
          <label>Controller Output Mode</label>
          <div class="mode-toggle-group">
            <button type="button" class="mode-toggle-btn active" id="btnOutputBle" onclick="setControllerOutputMode(false)">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="vertical-align: -2px; margin-right: 5px;"><polyline points="6.5 6.5 17.5 17.5 12 23 12 1 17.5 6.5 6.5 17.5"/></svg>
              Bluetooth (BLE)
            </button>
            <button type="button" class="mode-toggle-btn" id="btnOutputUsb" onclick="setControllerOutputMode(true)">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="vertical-align: -2px; margin-right: 5px;"><rect x="7" y="2" width="10" height="8" rx="1"/><path d="M5 10h14v5a7 7 0 0 1-14 0v-5z"/><line x1="12" y1="19" x2="12" y2="22"/></svg>
              Wired USB
            </button>
          </div>
          <span style="font-size: 11px; color: #94a3b8; display: block; margin-top: 6px;">Choose wireless BLE or direct plug-and-play USB HID over the USB-C port</span>
        </div>
        <button id="btnBleDisconnect" class="primary-btn" onclick="disconnectBle()">Cycle BLE Connection</button>
        <button id="btnBleRadio" class="danger-btn" style="margin-top: 10px;" onclick="toggleBleRadio()">Turn Off BLE Radio</button>
        <button id="btnBleForget" class="danger-btn" style="margin-top: 10px;" onclick="forgetBleBonds()">Forget BLE Bonds &amp; Re-pair</button>
      </div>

      <!-- 4. WiFi Settings -->
      <div class="settings-group">
        <h2>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M5 12.55a11 11 0 0 1 14.08 0" />
            <path d="M1.42 9a16 16 0 0 1 21.16 0" />
            <path d="M8.53 16.11a6 6 0 0 1 6.95 0" />
            <line x1="12" y1="20" x2="12.01" y2="20" />
          </svg>
          WiFi Settings
        </h2>
        <div class="setting-item">
          <label>Network: <span id="wifiSsidValue" style="color: var(--primary); font-weight: 700;">Connecting...</span></label>
        </div>
        <div class="setting-item">
          <label>Current IP Address: <span id="ipValue" style="color: var(--primary); font-weight: 700;">Connecting...</span></label>
        </div>
        <div class="setting-item">
          <label>Signal Strength: <span id="wifiRssiValue" style="color: var(--primary); font-weight: 700;">Connecting...</span></label>
        </div>
        <div class="setting-item">
          <label>Hostname</label>
          <input type="text" id="cfg-host" class="setting-input" value="CastPad" maxlength="32" placeholder="CastPad">
        </div>
        <button class="primary-btn" onclick="saveSettings()">Save Hostname</button>
        <div class="setting-item" style="margin-top: 14px; display: flex; align-items: center; justify-content: space-between;">
          <div>
            <label style="margin: 0; font-weight: 600;">Standalone AP Mode</label>
            <span style="font-size: 11px; color: #94a3b8; display: block;">Direct Wi-Fi without home router</span>
          </div>
          <label class="switch">
            <input type="checkbox" id="cfg-ap-mode" onchange="onApModeToggle(this)">
            <span class="slider"></span>
          </label>
        </div>
        <button class="danger-btn" style="margin-top: 12px;" onclick="confirmForgetWifi()">Forget Wi-Fi Settings</button>
      </div>

      <!-- 5. Button Backup & Restore -->
      <div class="settings-group">
        <h2>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
            <polyline points="7 10 12 15 17 10" />
            <line x1="12" y1="15" x2="12" y2="3" />
          </svg>
          Button Backup &amp; Restore
        </h2>
        <p style="font-size: 12px; color: #94a3b8; margin: 0 0 12px 0;">
          Export or restore your custom button mappings, sequences, labels, icons, and Quick Links. Does not include Wi-Fi credentials or system passwords.
        </p>
        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
          <button type="button" class="primary-btn" onclick="exportButtonConfig()" style="margin:0; display:flex; align-items:center; justify-content:center; gap:6px;">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
            <span>Export</span>
          </button>
          <button type="button" class="btn-secondary" onclick="document.getElementById('importConfigFile').click()" style="margin:0; display:flex; align-items:center; justify-content:center; gap:6px;">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
            <span>Import</span>
          </button>
        </div>
        <input type="file" id="importConfigFile" accept=".json" style="display:none;" onchange="importButtonConfig(this)">
      </div>

      <!-- 6. System & Maintenance -->
      <div class="settings-group">
        <h2>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <rect x="4" y="4" width="16" height="16" rx="2" />
            <rect x="9" y="9" width="6" height="6" />
            <line x1="9" y1="1" x2="9" y2="4" /><line x1="15" y1="1" x2="15" y2="4" />
            <line x1="9" y1="20" x2="9" y2="23" /><line x1="15" y1="20" x2="15" y2="23" />
            <line x1="20" y1="9" x2="23" y2="9" /><line x1="20" y1="15" x2="23" y2="15" />
            <line x1="1" y1="9" x2="4" y2="9" /><line x1="1" y1="15" x2="4" y2="15" />
          </svg>
          System &amp; Maintenance
        </h2>
        <div class="setting-item">
          <label>Firmware Version: <span id="fwVersion" style="color: var(--primary); font-weight: 700;">v2.8.3</span></label>
        </div>
        <div class="setting-item" style="margin-top: 10px;">
          <label>Select firmware file (.bin)</label>
          <input type="file" id="otaFile" accept=".bin" class="setting-input" style="padding: 8px;" onchange="onOtaFileSelect(this)">
          <div id="otaFileInfo" style="font-size: 11px; color: #94a3b8; margin-top: 4px; display: none;"></div>
        </div>
        <button class="primary-btn" id="otaUploadBtn" onclick="startOtaUpload()">Upload &amp; Flash Firmware</button>
        <div class="progress-bar" id="otaProgressBar">
          <div class="progress-fill" id="otaProgressFill"></div>
        </div>
        <div class="ota-status" id="otaStatusText"></div>

        <!-- System Actions Divider -->
        <div style="border-top: 1px solid var(--border); margin: 18px 0 14px 0;"></div>

        <button class="primary-btn" onclick="confirmRestartDevice()">Restart Device</button>
        <button class="danger-btn" onclick="confirmResetDevice()" style="margin-top: 8px;">Reset Device (Factory Reset)</button>
      </div>

    </div>
  </div>
  </div> <!-- /.phone-frame -->

  <!-- Keyboard Text Modal (Dual-Mode Live & Batch Keyboard) -->
  <div class="modal-overlay" id="kbModal" onclick="closeKeyboardModal(event)">
    <div class="modal-sheet kb-sheet" onclick="event.stopPropagation()">
      <div class="modal-header" style="justify-content: space-between; margin-bottom: 10px;">
        <h3 style="font-size: 17px; display: flex; align-items: center; gap: 7px;">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="var(--primary)" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
            <rect x="2" y="4" width="20" height="16" rx="2.5" />
            <path d="M6 8h.01M10 8h.01M14 8h.01M18 8h.01M6 12h.01M10 12h.01M14 12h.01M18 12h.01M7 16h10" />
          </svg>
          Keyboard
        </h3>
        <div style="display: flex; align-items: center; gap: 6px;">
          <div class="kb-mode-switch" title="Switch between real-time Live typing and Batch/Paste mode">
            <button type="button" class="kb-mode-pill active" id="kbModeLive" onclick="setKeyboardMode('live')">⚡ Live</button>
            <button type="button" class="kb-mode-pill" id="kbModeBatch" onclick="setKeyboardMode('batch')">📋 Batch</button>
          </div>
          <button type="button" class="btn-secondary" onclick="closeKeyboardModal()" style="padding: 5px 10px; font-size: 12px; border-radius: 999px;">Done</button>
        </div>
      </div>

      <form onsubmit="handleKeyboardSubmit(event)">
        <div class="kb-input-wrap">
          <input type="text" id="kbTextInput" placeholder="Type or dictate — sends live to TV..." autocomplete="off" autocapitalize="none" autocorrect="off" spellcheck="false" enterkeyhint="send">
          <button type="button" class="kb-input-clear" id="kbLocalClearBtn" onclick="clearLocalKbInput()" title="Clear input box">✕</button>
        </div>
        <div class="kb-hint" id="kbHintText">
          <span class="kb-live-dot"></span>
          <span>Live: keys &amp; voice dictation stream directly to your TV</span>
        </div>

        <!-- Batch Mode Send Buttons (hidden in Live Mode) -->
        <div class="modal-btn-row" id="kbBatchRow" style="display: none; margin: 0 0 10px 0;">
          <button type="button" class="btn-secondary" onclick="handleKeyboardSend(false)" style="padding: 9px; font-size: 13px;">Send Text</button>
          <button type="submit" class="primary-btn" style="margin: 0; padding: 9px; font-size: 13px;">Send + Enter ↵</button>
        </div>

        <!-- Quick-Edit Action Row -->
        <div class="kb-tools-row">
          <button type="button" class="kb-tool-btn" id="kbBtnBackspace" title="Tap or hold to delete characters on TV">⌫ Bksp</button>
          <button type="button" class="kb-tool-btn" onclick="kbTapKey('SPACE')" title="Send Spacebar">␣ Space</button>
          <button type="button" class="kb-tool-btn kb-danger" onclick="kbClearTvField()" title="Select All &amp; Clear the entire TV text field">🗑️ Clear TV</button>
          <button type="button" class="kb-tool-btn kb-primary" onclick="kbTapKey('ENTER')" title="Send Enter / Submit">↵ Enter</button>
        </div>

        <!-- Quick URL / Symbol Chips -->
        <div class="kb-chips-row">
          <button type="button" class="kb-chip" onclick="kbInsertChip('http://')">http://</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('https://')">https://</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('www.')">www.</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('.com')">.com</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('.net')">.net</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('.org')">.org</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('@')">@</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('/')">/</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip(':')">:</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('-')">-</button>
          <button type="button" class="kb-chip" onclick="kbInsertChip('_')">_</button>
        </div>

        <!-- Mini TV Navigation Row (move between fields or pick suggestions without closing keyboard) -->
        <div class="kb-nav-row">
          <button type="button" class="kb-nav-btn" onclick="kbTapNav('BACK')" title="TV Back">Back</button>
          <button type="button" class="kb-nav-btn" onclick="kbTapNav('LEFT')" title="Move Left">◀</button>
          <button type="button" class="kb-nav-btn" onclick="kbTapNav('UP')" title="Move Up">▲</button>
          <button type="button" class="kb-nav-btn" onclick="kbTapNav('DOWN')" title="Move Down">▼</button>
          <button type="button" class="kb-nav-btn" onclick="kbTapNav('RIGHT')" title="Move Right">▶</button>
          <button type="button" class="kb-nav-btn kb-ok" onclick="kbTapNav('ENTER')" title="TV OK / Select">OK</button>
        </div>
      </form>
    </div>
  </div>

  <!-- Button Config Modal (Long-Press Direct Customization) -->
  <div class="modal-overlay" id="btnConfigModal" onclick="closeButtonConfigModal(event)">
    <div class="modal-sheet" id="btnConfigSheet" onclick="event.stopPropagation()" style="max-height: 85vh; overflow-y: auto; transition: border-color 0.2s ease;">
      <div class="modal-header">
        <h3 id="btnConfigTitle"><span class="btn-cfg-swatch" id="btnConfigSwatch"></span>Configure Button</h3>
      </div>

      <!-- Icon & Label Row -->
      <label style="font-size:12px; color:#94a3b8; display:block; margin-bottom:4px;">Icon &amp; Label (Emoji over Text)</label>
      <div class="emoji-suggestions" style="display:flex; gap:6px; flex-wrap:wrap; margin-bottom:6px;">
        <span class="emoji-chip" onclick="setModalEmoji('📺')">📺</span>
        <span class="emoji-chip" onclick="setModalEmoji('🍿')">🍿</span>
        <span class="emoji-chip" onclick="setModalEmoji('🎬')">🎬</span>
        <span class="emoji-chip" onclick="setModalEmoji('🔊')">🔊</span>
        <span class="emoji-chip" onclick="setModalEmoji('⚡')">⚡</span>
        <span class="emoji-chip" onclick="setModalEmoji('⚙️')">⚙️</span>
        <span class="emoji-chip" onclick="setModalEmoji('▶️')">▶️</span>
        <span class="emoji-chip" onclick="setModalEmoji('⭐')">⭐</span>
        <span class="emoji-chip" onclick="setModalEmoji('💡')">💡</span>
        <span class="emoji-chip" onclick="setModalEmoji('🎮')">🎮</span>
        <span class="emoji-chip" onclick="setModalEmoji('')" title="Clear Emoji" style="color:#ef4444; font-size:12px; font-weight:700;">✕</span>
      </div>
      <div style="display:flex; gap:8px; margin-bottom:12px;">
        <input type="text" id="btnConfigEmoji" placeholder="Emoji" style="width:72px; text-align:center; font-size:18px;" maxlength="4">
        <input type="text" id="btnConfigLabel" placeholder="Button Label (e.g. Netflix, Mute)" style="flex:1;">
      </div>

      <!-- Action / Sequence Section -->
      <label style="font-size:12px; color:#94a3b8; display:block; margin-bottom:4px;">Action / Key or Sequence</label>
      <div style="display:flex; gap:8px; margin-bottom:8px;">
        <select id="btnConfigAction" class="setting-input" onchange="onBtnConfigActionChange()" style="flex:1; margin-bottom:0;">
          <option value="MENU">Menu</option>
          <option value="PLAY_PAUSE">Play / Pause</option>
          <option value="REWIND">Rewind</option>
          <option value="FAST_FORWARD">Fast Forward</option>
          <option value="MUTE">Mute Audio</option>
          <option value="VOL_UP">Volume Up (+)</option>
          <option value="VOL_DOWN">Volume Down (-)</option>
          <option value="SEARCH">Search</option>
          <option value="HOME">Home</option>
          <option value="BACK">Back</option>
          <option value="ENTER">Enter / OK</option>
          <option value="SPACE">Spacebar</option>
          <option value="PAGE_UP">CH+ / Page Up</option>
          <option value="PAGE_DOWN">CH- / Page Down</option>
          <option value="F1">F1</option>
          <option value="F2">F2</option>
          <option value="F3">F3</option>
          <option value="F4">F4</option>
          <option value="F5">F5</option>
          <option value="F6">F6</option>
          <option value="F7">F7</option>
          <option value="F8">F8</option>
          <option value="F9">F9</option>
          <option value="F10">F10</option>
          <option value="F11">F11</option>
          <option value="F12">F12</option>
          <option value="TAB">Tab</option>
          <option value="BACKSPACE">Backspace</option>
          <option value="NONE">None / Disabled</option>
          <option value="CUSTOM">Custom Code / Text / Digits...</option>
        </select>
        <button type="button" class="btn-secondary" onclick="openSequenceBuilderModal()" style="display:inline-flex; align-items:center; gap:4px; padding:0 12px; white-space:nowrap; font-weight:700;">
          ⚡ Sequence
        </button>
      </div>

      <div id="btnConfigCustomWrap" style="display:none; margin-bottom:12px;">
        <input type="text" id="btnConfigCustom" placeholder="e.g. 501, 0x66, C:0x0223, K:0x28, F1, or text">
      </div>

      <!-- Preview badge when a multi-step sequence is configured -->
      <div id="btnConfigSeqPreviewWrap" style="display:none; margin-bottom:12px; background:rgba(0,0,0,0.3); border:1px solid rgba(255,255,255,0.1); border-radius:10px; padding:8px 10px;">
        <div style="display:flex; align-items:center; justify-content:space-between; margin-bottom:4px;">
          <span style="font-size:11px; font-weight:700; color:var(--primary);">⚡ ACTIVE SEQUENCE</span>
          <button type="button" class="btn-ghost" onclick="openSequenceBuilderModal()" style="font-size:11px; padding:2px 6px;">Edit Steps</button>
        </div>
        <div id="btnConfigSeqFlow" style="font-size:12px; color:#cbd5e1; word-break:break-all;"></div>
      </div>

      <!-- Color Accent Section -->
      <label style="font-size:12px; color:#94a3b8; display:block; margin:8px 0 6px 0;">Color Accent</label>
      <div class="color-swatch-picker" id="modalColorPicker" style="margin:0 0 12px 0;">
        <div class="color-swatch-pill selected" data-color="theme" style="background:var(--primary); display:flex; align-items:center; justify-content:center; font-size:13px; color:#fff;" title="Default (Match App Theme)" onclick="selectModalColor('theme')">✨</div>
        <div class="color-swatch-pill" data-color="sky" style="background:#38bdf8;" title="Sky Blue" onclick="selectModalColor('sky')"></div>
        <div class="color-swatch-pill" data-color="green" style="background:#22c55e;" title="Green" onclick="selectModalColor('green')"></div>
        <div class="color-swatch-pill" data-color="amber" style="background:#f59e0b;" title="Amber / Orange" onclick="selectModalColor('amber')"></div>
        <div class="color-swatch-pill" data-color="red" style="background:#ef4444;" title="Red" onclick="selectModalColor('red')"></div>
        <div class="color-swatch-pill" data-color="indigo" style="background:#6366f1;" title="Indigo" onclick="selectModalColor('indigo')"></div>
        <div class="color-swatch-pill" data-color="purple" style="background:#a855f7;" title="Purple" onclick="selectModalColor('purple')"></div>
        <div class="color-swatch-pill" data-color="pink" style="background:#f43f5e;" title="Pink" onclick="selectModalColor('pink')"></div>
        <div class="color-swatch-pill" data-color="slate" style="background:#64748b;" title="Slate / Neutral" onclick="selectModalColor('slate')"></div>
      </div>

      <div class="modal-btn-row">
        <button type="button" class="btn-ghost-danger" onclick="clearButtonConfig()">Clear</button>
        <button type="button" class="btn-secondary" onclick="closeButtonConfigModal()">Cancel</button>
        <button type="button" class="primary-btn" onclick="saveButtonConfig()">Save</button>
      </div>
    </div>
  </div>

  <!-- Advanced Sequence Builder Modal -->
  <div class="modal-overlay" id="sequenceModal" onclick="closeSequenceBuilderModal(event)">
    <div class="modal-sheet" onclick="event.stopPropagation()" style="max-height: 85vh; overflow-y: auto;">
      <div class="modal-header">
        <h3><span style="color:var(--primary); margin-right:6px;">⚡</span>Sequence Builder</h3>
      </div>
      <p style="font-size:12px; color:#94a3b8; margin:0 0 12px 0;">Configure multi-step actions and pauses executed sequentially on the ESP32.</p>

      <div id="seqStepsContainer">
        <!-- Rendered dynamically -->
      </div>

      <div style="display:flex; gap:6px; flex-wrap:wrap; margin:10px 0 14px 0;">
        <button type="button" class="btn-secondary" onclick="addSeqStep('key')" style="padding:6px 10px; font-size:12px;">+ Add Key</button>
        <button type="button" class="btn-secondary" onclick="addSeqStep('wait')" style="padding:6px 10px; font-size:12px;">+ Add Pause</button>
        <button type="button" class="btn-secondary" onclick="addSeqStep('text')" style="padding:6px 10px; font-size:12px;">+ Add Text / Digits</button>
      </div>

      <div style="background:rgba(0,0,0,0.3); border:1px solid rgba(255,255,255,0.08); border-radius:10px; padding:8px 10px; margin-bottom:14px;">
        <div style="font-size:11px; font-weight:700; color:#94a3b8; margin-bottom:6px; text-transform:uppercase;">Sequence Flow Preview</div>
        <div id="seqFlowPreview" style="display:flex; align-items:center; gap:6px; flex-wrap:wrap; min-height:24px;"></div>
      </div>

      <div class="modal-btn-row">
        <button type="button" class="btn-secondary" onclick="closeSequenceBuilderModal()">Cancel</button>
        <button type="button" class="primary-btn" onclick="applySequenceBuilder()">Done</button>
      </div>
    </div>
  </div>

  <!-- About Modal -->
  <div class="modal-overlay" id="aboutModal" onclick="closeAboutModal(event)">
    <div class="modal-sheet" onclick="event.stopPropagation()" style="max-height: 85vh; overflow-y: auto;">
      <div class="modal-header">
        <h3>About CastPad</h3>
      </div>
      <div style="font-size: 13px; color: #cbd5e1; line-height: 1.6; display: flex; flex-direction: column; gap: 12px;">
        <p><strong>CastPad</strong> is a high-performance, ultra-low-latency ESP32-S3 wireless Bluetooth 5.0 (BLE) and plug-and-play Wired USB HID combo remote, mouse trackpad, and dual-mode keyboard designed for streaming devices, Fire TV, Google TV, smart TVs, PCs, and Macs.</p>
        <p>It provides a responsive web remote with a square D-Pad, customizable corner &amp; color macro buttons with multi-step sequences, swipeable Quick Links pages, a sub-pixel trackpad with inertia and scroll strip, a live/batch streaming keyboard, smart power management, and rollback-protected OTA firmware updates.</p>
        <div style="background: rgba(15, 23, 42, 0.6); padding: 10px 12px; border-radius: 12px; border: 1px solid rgba(255,255,255,0.08); font-size: 12px; display: flex; flex-direction: column; gap: 4px;">
          <div><strong>Device Uptime:</strong> <span id="uptimeValue" style="color: var(--primary); font-weight: 700;">0m 0s</span></div>
          <div><strong>Firmware Version:</strong> <span id="aboutFwVersion" style="color: var(--primary); font-weight: 700;">v2.8.3</span></div>
          <div><strong>Platform:</strong> ESP32-S3 Super Mini (BLE 5.0 + Native USB HID)</div>
        </div>

        <div>
          <h4 style="color: var(--primary); font-size: 13px; font-weight: 700; margin-bottom: 6px;">Libraries &amp; Components</h4>
          <ul style="padding-left: 18px; font-size: 12px; color: #94a3b8; display: flex; flex-direction: column; gap: 4px;">
            <li><a href="https://github.com/h2zero/NimBLE-Arduino" target="_blank" rel="noopener noreferrer" style="color: var(--primary); text-decoration: underline;">NimBLE-Arduino</a> &ndash; Bluetooth 5.0 (2M PHY) Low Energy Stack</li>
            <li><a href="https://github.com/kokofixcomputers/ESP32BLECombo" target="_blank" rel="noopener noreferrer" style="color: var(--primary); text-decoration: underline;">ESP32 BLE Combo</a> &ndash; Combined BLE HID Keyboard, Mouse &amp; Consumer Control</li>
            <li><a href="https://github.com/hathach/tinyusb" target="_blank" rel="noopener noreferrer" style="color: var(--primary); text-decoration: underline;">TinyUSB</a> &ndash; Native ESP32-S3 Wired USB HID Stack</li>
            <li><a href="https://github.com/esp32async/ESPAsyncWebServer" target="_blank" rel="noopener noreferrer" style="color: var(--primary); text-decoration: underline;">ESPAsyncWebServer</a> &ndash; Asynchronous Web Server &amp; WebSockets</li>
            <li><a href="https://github.com/esp32async/AsyncTCP" target="_blank" rel="noopener noreferrer" style="color: var(--primary); text-decoration: underline;">AsyncTCP</a> &ndash; Dual-Core Asynchronous TCP Library for ESP32</li>
            <li><a href="https://arduinojson.org/" target="_blank" rel="noopener noreferrer" style="color: var(--primary); text-decoration: underline;">ArduinoJson</a> &ndash; C++ JSON Library by Benoit Blanchon</li>
            <li><a href="https://github.com/espressif/arduino-esp32" target="_blank" rel="noopener noreferrer" style="color: var(--primary); text-decoration: underline;">arduino-esp32</a> &ndash; Official Espressif ESP32-S3 Arduino Core</li>
          </ul>
        </div>

        <button type="button" class="primary-btn" onclick="closeAboutModal()" style="margin-top: 4px;">Close</button>
      </div>
    </div>
  </div>

  <!-- Help & User Guide Modal -->
  <div class="modal-overlay" id="helpModal" onclick="closeHelpModal(event)">
    <div class="modal-sheet" onclick="event.stopPropagation()" style="max-height: 85vh; overflow-y: auto;">
      <div class="modal-header">
        <h3>CastPad User Guide</h3>
      </div>
      <div style="font-size: 13px; color: #cbd5e1; line-height: 1.6; display: flex; flex-direction: column; gap: 14px;">
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">🎮 Remote &amp; D-Pad</h4>
          <p>Tap Up, Down, Left, Right, and Center OK to navigate menus. Home, Back, Menu, Volume, Mute, and Media transport buttons control your TV or streaming device directly.</p>
        </div>
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">🔌 Bluetooth (BLE) vs. Wired USB Mode</h4>
          <p>In <strong>Settings &rarr; Controller &amp; Bluetooth</strong>, switch between wireless <strong>Bluetooth (BLE)</strong> and plug-and-play <strong>Wired USB</strong>. In Wired USB mode, the BLE radio is powered down and the ESP32-S3 USB-C port acts as a direct USB HID keyboard, mouse, and media remote.</p>
        </div>
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">⚙️ Custom Buttons &amp; Macro Sequences</h4>
          <p><strong>Long-press</strong> any colored button (Red, Green, Yellow, Blue) or corner D-Pad button (Custom 1&ndash;4) for 0.5s to customize it. Set a custom label, Emoji icon, color swatch, preset key, raw HID hex code, or build a <strong>Multi-Step Sequence</strong> (combining Key presses, Wait delays, and Text typing). Use <strong>Button Backup &amp; Restore</strong> in Settings to export or import your layout as JSON.</p>
        </div>
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">⚡ Quick Links &amp; Pages</h4>
          <p>Single-tap any empty Quick Link slot to set it up, or <strong>long-press</strong> any configured slot to edit its channel number, macro sequence, icon, and color. Swipe left/right or tap the pagination dots to flip pages (1&ndash;5 pages / up to 60 slots). On desktop browsers, click <strong>Quick Edit All Slots</strong> to batch-edit every slot in one table.</p>
        </div>
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">🖱️ Mouse &amp; Trackpad</h4>
          <p>Slide across the trackpad for sub-pixel cursor movement with coalesced zero-lag updates. <strong>Single-tap</strong> for Left Click, <strong>two-finger tap</strong> for Right Click, or <strong>double-tap &amp; hold</strong> to drag. Slide along the right-edge <strong>SCROLL</strong> strip to scroll. On desktop, toggle <strong>PC Keys</strong> in the top header to type and navigate directly from your computer keyboard.</p>
        </div>
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">⌨️ Dual-Mode Keyboard (Live &amp; Batch)</h4>
          <p>Tap <strong>Keyboard</strong> to open the persistent keyboard sheet. Use <strong>⚡ Live</strong> mode to stream characters, voice dictation, and Backspace in real time as you type, or switch to <strong>📋 Batch</strong> mode to paste long URLs and passwords. Includes hold-to-repeat Backspace, one-tap <strong>Clear TV</strong>, URL chips (<code>http://</code>, <code>.com</code>), and a built-in mini D-Pad.</p>
        </div>
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">📡 Standalone AP Mode &amp; Physical BOOT Button</h4>
          <p>When away from home Wi-Fi, enable <strong>Standalone AP Mode</strong> in Settings — or hold the physical <strong>BOOT</strong> button on the ESP32-S3 for <strong>4 seconds</strong> to toggle it on/off. CastPad will broadcast its own hotspot (SSID: <code>CastPad</code>) at <code>http://192.168.4.1</code>.</p>
        </div>
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">💡 RGB Status LED &amp; Power Saving</h4>
          <ul style="padding-left: 18px; font-size: 12px; color: #cbd5e1; display: flex; flex-direction: column; gap: 3px;">
            <li><strong>Amber Pulse:</strong> Connecting to Wi-Fi (fast) or Setup / Standalone AP Mode (slow).</li>
            <li><strong>Blue Slow Pulse:</strong> Connected to Wi-Fi, waiting for Bluetooth pairing.</li>
            <li><strong>Solid Green/Cyan:</strong> BLE host paired or Wired USB active (auto-sleeps after 60s of inactivity when <strong>Power Saving Mode</strong> is enabled).</li>
            <li><strong>Purple Flash (3&times;):</strong> Remote, mouse, or keyboard command transmitted.</li>
          </ul>
        </div>
        <div>
          <h4 style="color: var(--primary); font-size: 14px; margin-bottom: 4px;">📺 StbEmu (IPTV Emulator) Setup &amp; Best Practices</h4>
          <p style="margin-bottom: 6px;">CastPad is optimized out of the box for StbEmu Pro / Free on Fire TV and Android TV devices:</p>
          <ul style="padding-left: 18px; font-size: 12px; color: #cbd5e1; display: flex; flex-direction: column; gap: 4px;">
            <li><strong>Channel Tuning via Quick Links:</strong> Enter your favorite IPTV channel numbers (e.g. <code>501</code>) into Quick Links slots. Tapping the link sends the digit sequence followed by OK, instantly tuning channels in StbEmu.</li>
            <li><strong>Portal Menu &amp; Color Keys:</strong> Press the <strong>Menu</strong> button to open the StbEmu portal menu, or use the Red, Green, Yellow, and Blue buttons (mapped to <code>F1&ndash;F4</code> by default) for MAG color shortcuts (Guide, Aspect, Info, Audio).</li>
            <li><strong>Corner Aux Buttons:</strong> Configure <strong>Custom 1&ndash;4</strong> as <code>CH+ / Page Up</code>, <code>CH- / Page Down</code>, <code>Info</code>, or multi-step macros for rapid channel surfing.</li>
            <li><strong>StbEmu Settings:</strong> Use the <strong>Mouse &amp; Trackpad</strong> tab and <strong>📋 Batch</strong> keyboard mode to easily enter StbEmu portal URLs and MAC addresses.</li>
          </ul>
        </div>
        <button type="button" class="primary-btn" onclick="closeHelpModal()" style="margin-top: 6px;">Got It</button>
      </div>
    </div>
  </div>

  <!-- Add to Home Screen Modal (iOS / Mobile Guide) -->
  <div class="modal-overlay" id="pwaModal" onclick="closeAddToHomeModal(event)">
    <div class="modal-sheet" onclick="event.stopPropagation()">
      <div class="modal-header">
        <h3>Add to Home Screen</h3>
      </div>
      <div style="font-size: 13px; color: #cbd5e1; line-height: 1.6; display: flex; flex-direction: column; gap: 12px;">
        <p>Install <strong>CastPad</strong> as a standalone fullscreen app on your mobile device:</p>
        <div style="background: rgba(15, 23, 42, 0.6); padding: 12px; border-radius: 12px; border: 1px solid rgba(255,255,255,0.08); font-size: 13px; display: flex; flex-direction: column; gap: 10px;">
          <div style="display: flex; align-items: center; gap: 10px;">
            <div style="background: var(--primary-glow); color: var(--primary); width: 28px; height: 28px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-weight: 700; flex-shrink: 0;">1</div>
            <div>Tap the <strong>Share</strong> button in your browser toolbar (<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--primary)" stroke-width="2.5" style="vertical-align: -2px;"><path d="M4 12v8a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-8"/><polyline points="16 6 12 2 8 6"/><line x1="12" y1="2" x2="12" y2="15"/></svg>).</div>
          </div>
          <div style="display: flex; align-items: center; gap: 10px;">
            <div style="background: var(--primary-glow); color: var(--primary); width: 28px; height: 28px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-weight: 700; flex-shrink: 0;">2</div>
            <div>Select <strong>Add to Home Screen</strong> (<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--primary)" stroke-width="2.5" style="vertical-align: -2px;"><rect x="3" y="3" width="18" height="18" rx="3"/><line x1="12" y1="8" x2="12" y2="16"/><line x1="8" y1="12" x2="16" y2="12"/></svg>).</div>
          </div>
        </div>
        <button type="button" class="primary-btn" onclick="closeAddToHomeModal()" style="margin-top: 4px;">Got It</button>
      </div>
    </div>
  </div>

  <!-- Confirmation Modal -->
  <div class="modal-overlay" id="confirmModal" onclick="closeConfirmModal(event)">
    <div class="modal-sheet" onclick="event.stopPropagation()">
      <div class="modal-header">
        <h3 id="confirmTitle">Confirm Action</h3>
      </div>
      <p id="confirmMessage" style="font-size: 13px; color: #cbd5e1; line-height: 1.5; margin: 12px 0 18px;"></p>
      <div class="modal-btn-row">
        <button type="button" class="btn-secondary" onclick="closeConfirmModal()">Cancel</button>
        <button type="button" id="confirmOkBtn" class="primary-btn" style="margin: 0;">Confirm</button>
      </div>
    </div>
  </div>

  <!-- Batch Quick Edit Modal (Desktop) -->
  <div class="modal-overlay" id="batchEditModal" onclick="closeBatchQuickEditModal(event)">
    <div class="modal-sheet batch-edit-sheet" onclick="event.stopPropagation()">
      <div class="modal-header" style="justify-content: space-between;">
        <div style="display:flex; align-items:center; gap:8px;">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" style="color:var(--primary);">
            <path d="M12 20h9"/>
            <path d="M16.5 3.5a2.121 2.121 0 0 1 3 3L7 19l-4 1 1-4L16.5 3.5z"/>
          </svg>
          <h3>Quick Edit All Slots</h3>
        </div>
        <button type="button" class="btn-secondary" onclick="closeBatchQuickEditModal()" style="padding: 6px 12px; font-size: 13px;">✕</button>
      </div>
      <p style="font-size: 13px; color: var(--text-muted); margin-bottom: 10px;">
        Quickly review and edit all quick link slots across all pages. Blank digits or labels will leave the slot unassigned.
      </p>
      <div class="batch-edit-table-wrap">
        <table class="batch-edit-table">
          <thead>
            <tr>
              <th style="width: 60px;">Slot</th>
              <th style="width: 55px; text-align: center;">Icon</th>
              <th style="min-width: 140px;">Label / Channel Name</th>
              <th style="width: 120px;">Digits / Key Code</th>
              <th style="width: 110px;">Color</th>
              <th style="width: 70px; text-align: center;">Action</th>
            </tr>
          </thead>
          <tbody id="batchEditTableBody">
            <!-- Populated dynamically via openBatchQuickEditModal() -->
          </tbody>
        </table>
      </div>
      <div class="modal-btn-row" style="margin-top: 14px;">
        <button type="button" class="btn-secondary" onclick="closeBatchQuickEditModal()">Cancel</button>
        <button type="button" class="primary-btn" onclick="saveBatchQuickEdit()" style="margin: 0;">Save All Slots</button>
      </div>
    </div>
  </div>

  <!-- Toast Notification -->
  <div id="toast" class="toast"></div>

  <script>
    let ws = null;
    let sensitivity = 1.3;
    let txFlashTimer = null;
    let toastTimer = null;
    let suppressClicksUntil = 0;
    let bleRadioEnabled = true;
    let usbModeEnabled = false;
    let isBleConnected = false;

    function isControllerActive() {
      return usbModeEnabled || bleRadioEnabled;
    }
    let currentConfigTarget = null;
    let qlPages = 3;
    let qlCurrentPage = 0;
    let quickLinksData = Array.from({ length: 60 }, () => ({ label: '', num: '', color: 'theme', icon: '' }));
    let currentSelectedModalColor = 'theme';
    let currentSequenceSteps = [];
    const QL_COLORS = {
      theme: 'var(--primary)',
      sky: '#38bdf8',
      green: '#22c55e',
      amber: '#f59e0b',
      red: '#ef4444',
      indigo: '#6366f1',
      purple: '#a855f7',
      pink: '#f43f5e',
      slate: '#64748b'
    };

    let buttonActions = {
      red: 'F1',
      green: 'F2',
      yellow: 'F3',
      blue: 'F4',
      custom1: 'NONE',
      custom2: 'NONE',
      custom3: 'NONE',
      custom4: 'NONE'
    };
    let buttonLabels = {
      red: '',
      green: '',
      yellow: '',
      blue: '',
      custom1: '',
      custom2: '',
      custom3: '',
      custom4: ''
    };
    let buttonIcons = {
      red: '',
      green: '',
      yellow: '',
      blue: '',
      custom1: '',
      custom2: '',
      custom3: '',
      custom4: ''
    };
    let buttonColors = {
      red: 'theme',
      green: 'theme',
      yellow: 'theme',
      blue: 'theme',
      custom1: 'theme',
      custom2: 'theme',
      custom3: 'theme',
      custom4: 'theme'
    };

    function escapeHtml(str) {
      if (!str) return '';
      return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
    }

    function updateButtonDisplay(id) {
      const lbl = (buttonLabels[id] || '').trim();
      const icon = (buttonIcons[id] || '').trim();
      const col = buttonColors[id] || 'theme';
      const hasAction = buttonActions[id] && buttonActions[id] !== 'NONE';

      const auxEl = document.getElementById('aux-btn-' + id);
      const colorEl = document.querySelector('.color-btn.btn-' + id);
      const btnEl = auxEl || colorEl;
      if (!btnEl) return;

      btnEl.setAttribute('data-color', col);

      btnEl.innerHTML = '';
      if (icon && lbl) {
        btnEl.innerHTML = `<span class="btn-emoji">${escapeHtml(icon)}</span><span class="btn-text-label">${escapeHtml(lbl)}</span>`;
      } else if (icon) {
        btnEl.innerHTML = `<span class="btn-emoji large">${escapeHtml(icon)}</span>`;
      } else if (lbl) {
        btnEl.innerHTML = `<span class="btn-text-label">${escapeHtml(lbl)}</span>`;
      } else {
        if (auxEl) {
          btnEl.innerHTML = `<span class="dpad-aux-label"></span>`;
        } else if (colorEl) {
          btnEl.innerHTML = `<span class="color-label"></span>`;
        }
      }

      if (auxEl) {
        if (lbl || icon || hasAction) {
          auxEl.classList.add('configured');
        } else {
          auxEl.classList.remove('configured');
        }
      }
    }

    function cacheButtonsLocally() {
      try {
        localStorage.setItem('castpad_ql_data', JSON.stringify(quickLinksData));
        localStorage.setItem('castpad_btn_labels', JSON.stringify(buttonLabels));
        localStorage.setItem('castpad_btn_icons', JSON.stringify(buttonIcons));
        localStorage.setItem('castpad_btn_colors', JSON.stringify(buttonColors));
        localStorage.setItem('castpad_btn_actions', JSON.stringify(buttonActions));
      } catch(e) {}
    }

    function restoreCachedButtons() {
      try {
        const cachedQl = localStorage.getItem('castpad_ql_data');
        if (cachedQl) {
          const parsed = JSON.parse(cachedQl);
          if (Array.isArray(parsed)) {
            for (let i = 0; i < parsed.length && i < 60; i++) {
              if (parsed[i]) quickLinksData[i] = parsed[i];
            }
            renderQuickLinksGrid();
          }
        }
        const cachedLabels = localStorage.getItem('castpad_btn_labels');
        if (cachedLabels) Object.assign(buttonLabels, JSON.parse(cachedLabels));
        const cachedIcons = localStorage.getItem('castpad_btn_icons');
        if (cachedIcons) Object.assign(buttonIcons, JSON.parse(cachedIcons));
        const cachedColors = localStorage.getItem('castpad_btn_colors');
        if (cachedColors) Object.assign(buttonColors, JSON.parse(cachedColors));
        const cachedActions = localStorage.getItem('castpad_btn_actions');
        if (cachedActions) Object.assign(buttonActions, JSON.parse(cachedActions));
        ['red','green','yellow','blue','custom1','custom2','custom3','custom4'].forEach(updateButtonDisplay);
      } catch(e) {}
    }
    let bootUptimeSeconds = 0;
    let bootTimeLocal = performance.now();

    function formatUptime(totalSecs) {
      const days = Math.floor(totalSecs / 86400);
      const hours = Math.floor((totalSecs % 86400) / 3600);
      const minutes = Math.floor((totalSecs % 3600) / 60);
      const seconds = Math.floor(totalSecs % 60);
      let parts = [];
      if (days > 0) parts.push(days + 'd');
      if (hours > 0 || days > 0) parts.push(hours + 'h');
      parts.push(minutes + 'm');
      parts.push(seconds + 's');
      return parts.join(' ');
    }

    function updateUptimeTicker() {
      const el = document.getElementById('uptimeValue');
      if (!el) return;
      const elapsedSec = (performance.now() - bootTimeLocal) / 1000;
      const currentUptime = Math.floor(bootUptimeSeconds + elapsedSec);
      el.textContent = formatUptime(currentUptime);
    }

    function haptic() {
      if (navigator.vibrate) navigator.vibrate(20);
    }

    function showToast(msg) {
      const toast = document.getElementById('toast');
      if (!toast) return;
      toast.textContent = msg;
      toast.classList.add('show');
      if (toastTimer) clearTimeout(toastTimer);
      toastTimer = setTimeout(() => {
        toast.classList.remove('show');
      }, 2000);
    }

    function flashBleTx() {
      const badge = document.getElementById('bleBadge');
      if (!badge) return;
      badge.classList.add('tx-flash');
      if (txFlashTimer) clearTimeout(txFlashTimer);
      txFlashTimer = setTimeout(() => {
        badge.classList.remove('tx-flash');
      }, 140);
    }

    function spawnRipple(el, clientX, clientY) {
      if (!el) return;
      const rect = el.getBoundingClientRect();
      const size = Math.max(rect.width, rect.height, 44);
      const x = (clientX !== undefined ? clientX - rect.left : rect.width / 2) - size / 2;
      const y = (clientY !== undefined ? clientY - rect.top : rect.height / 2) - size / 2;
      const ripple = document.createElement('span');
      ripple.className = 'touch-ripple';
      ripple.style.width = ripple.style.height = size + 'px';
      ripple.style.left = x + 'px';
      ripple.style.top = y + 'px';
      el.appendChild(ripple);
      setTimeout(() => ripple.remove(), 430);
    }

    function initButtonVisualFeedback() {
      const selector = '.dpad-btn, .dpad-center, .dpad-aux, .nav-btn, .color-btn, .ql-btn, .kb-circle-btn, .mouse-btn, .primary-btn, .danger-btn, .tab-btn';
      document.querySelectorAll(selector).forEach(btn => {
        let pressStart = 0;
        let releaseTimer = null;

        const onPress = (e) => {
          if (releaseTimer) {
            clearTimeout(releaseTimer);
            releaseTimer = null;
          }
          pressStart = performance.now();
          btn.classList.add('btn-pressed');
          let cx = e.clientX, cy = e.clientY;
          if (e.touches && e.touches[0]) {
            cx = e.touches[0].clientX;
            cy = e.touches[0].clientY;
          }
          spawnRipple(btn, cx, cy);
        };

        const onRelease = () => {
          if (!btn.classList.contains('btn-pressed')) return;
          const elapsed = performance.now() - pressStart;
          const remaining = Math.max(0, 140 - elapsed);
          if (releaseTimer) clearTimeout(releaseTimer);
          releaseTimer = setTimeout(() => {
            btn.classList.remove('btn-pressed');
            releaseTimer = null;
          }, remaining);
        };

        if (window.PointerEvent) {
          btn.addEventListener('pointerdown', onPress, { passive: true });
          btn.addEventListener('pointerup', onRelease, { passive: true });
          btn.addEventListener('pointercancel', onRelease, { passive: true });
          btn.addEventListener('pointerleave', (e) => {
            if (e.pointerType === 'mouse') onRelease();
          }, { passive: true });
        } else {
          btn.addEventListener('touchstart', onPress, { passive: true });
          btn.addEventListener('touchend', onRelease, { passive: true });
          btn.addEventListener('touchcancel', onRelease, { passive: true });
          btn.addEventListener('mousedown', onPress, { passive: true });
          btn.addEventListener('mouseup', onRelease, { passive: true });
          btn.addEventListener('mouseleave', onRelease, { passive: true });
        }
      });

      // Clean pointer/touch binding for Left/Right Mouse Buttons (prevents duplicate synthetic mouse events)
      ['Left', 'Right'].forEach(side => {
        const btn = document.getElementById('btnMouse' + side);
        if (!btn) return;
        const sideLower = side.toLowerCase();
        let isDown = false;

        const handleDown = (e) => {
          if (e.cancelable) e.preventDefault();
          if (isDown) return;
          isDown = true;
          mouseDown(sideLower);
        };

        const handleUp = () => {
          if (!isDown) return;
          isDown = false;
          mouseUp(sideLower);
        };

        if (window.PointerEvent) {
          btn.addEventListener('pointerdown', (e) => {
            if (btn.setPointerCapture && e.pointerId !== undefined) {
              try { btn.setPointerCapture(e.pointerId); } catch (_) {}
            }
            handleDown(e);
          });
          btn.addEventListener('pointerup', handleUp);
          btn.addEventListener('pointercancel', handleUp);
          btn.addEventListener('lostpointercapture', handleUp);
          btn.addEventListener('pointerleave', (e) => {
            if (e.pointerType === 'mouse') handleUp();
          });
        } else {
          btn.addEventListener('touchstart', handleDown, { passive: false });
          btn.addEventListener('touchend', handleUp, { passive: false });
          btn.addEventListener('touchcancel', handleUp, { passive: false });
          btn.addEventListener('mousedown', handleDown);
          btn.addEventListener('mouseup', handleUp);
          btn.addEventListener('mouseleave', handleUp);
        }
      });
    }

    function switchTab(name, btnEl, updateHash = true, triggerHaptic = true) {
      if (triggerHaptic) haptic();
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));

      let targetBtn = btnEl;
      if (!targetBtn) {
        if (name === 'quicklinks') targetBtn = document.querySelector('.tab-btn.tab-ql');
        else targetBtn = document.querySelector('.tab-btn.tab-' + name);
      }
      if (targetBtn) targetBtn.classList.add('active');
      const targetTab = document.getElementById('tab-' + name);
      if (targetTab) targetTab.classList.add('active');

      try {
        localStorage.setItem('castpad_tab', name);
      } catch (e) {}

      if (updateHash) {
        try {
          if (window.location.hash !== '#' + name) {
            history.replaceState(null, '', '#' + name);
          }
        } catch (e) {
          window.location.hash = name;
        }
      }
    }

    function refreshApp() {
      const currentTab = localStorage.getItem('castpad_tab') || (window.location.hash || '').replace(/^#/, '') || 'remote';
      try {
        window.location.hash = currentTab;
      } catch (e) {}
      window.location.reload();
    }

    function restoreActiveTab() {
      const hash = (window.location.hash || '').replace(/^#/, '');
      if (hash && document.getElementById('tab-' + hash)) {
        switchTab(hash, null, false, false);
        return;
      }
      try {
        const saved = localStorage.getItem('castpad_tab');
        if (saved && document.getElementById('tab-' + saved)) {
          switchTab(saved, null, true, false);
          return;
        }
      } catch (e) {}
    }

    // Immediately activate the correct tab upon script parse
    restoreActiveTab();

    window.addEventListener('hashchange', () => {
      const hash = (window.location.hash || '').replace(/^#/, '');
      if (hash && document.getElementById('tab-' + hash)) {
        switchTab(hash, null, false, false);
      }
    });

    const APP_THEMES = {
      cyan: {
        primary: '#06b6d4',
        glow: 'rgba(6, 182, 212, 0.3)',
        btnBg: '#0e7490',
        btnBorder: 'rgba(6, 182, 212, 0.5)',
        cardBorderDark: 'rgba(6, 182, 212, 0.3)',
        cardBorderLight: 'rgba(6, 182, 212, 0.35)'
      },
      grey: {
        primary: '#94a3b8',
        glow: 'rgba(148, 163, 184, 0.25)',
        btnBg: '#475569',
        btnBorder: 'rgba(148, 163, 184, 0.5)',
        cardBorderDark: 'rgba(148, 163, 184, 0.25)',
        cardBorderLight: 'rgba(100, 116, 139, 0.3)'
      },
      sky: {
        primary: '#38bdf8',
        glow: 'rgba(56, 189, 248, 0.28)',
        btnBg: '#0284c7',
        btnBorder: 'rgba(56, 189, 248, 0.5)',
        cardBorderDark: 'rgba(56, 189, 248, 0.28)',
        cardBorderLight: 'rgba(2, 132, 199, 0.32)'
      },
      green: {
        primary: '#34d399',
        glow: 'rgba(52, 211, 153, 0.22)',
        btnBg: '#047857',
        btnBorder: 'rgba(52, 211, 153, 0.45)',
        cardBorderDark: 'rgba(52, 211, 153, 0.25)',
        cardBorderLight: 'rgba(5, 150, 105, 0.3)'
      },
      purple: {
        primary: '#a78bfa',
        glow: 'rgba(167, 139, 250, 0.22)',
        btnBg: '#6d28d9',
        btnBorder: 'rgba(167, 139, 250, 0.45)',
        cardBorderDark: 'rgba(167, 139, 250, 0.25)',
        cardBorderLight: 'rgba(124, 58, 237, 0.3)'
      },
      amber: {
        primary: '#fbbf24',
        glow: 'rgba(251, 191, 36, 0.2)',
        btnBg: '#b45309',
        btnBorder: 'rgba(251, 191, 36, 0.45)',
        cardBorderDark: 'rgba(251, 191, 36, 0.25)',
        cardBorderLight: 'rgba(217, 119, 6, 0.3)'
      },
      rose: {
        primary: '#fb7185',
        glow: 'rgba(251, 113, 133, 0.2)',
        btnBg: '#be123c',
        btnBorder: 'rgba(251, 113, 133, 0.45)',
        cardBorderDark: 'rgba(251, 113, 133, 0.25)',
        cardBorderLight: 'rgba(225, 29, 72, 0.3)'
      }
    };
    let currentAppTheme = 'cyan';

    function setAppTheme(themeKey, persist = true) {
      if (!APP_THEMES[themeKey]) themeKey = 'cyan';
      currentAppTheme = themeKey;
      const t = APP_THEMES[themeKey];
      const root = document.documentElement;
      root.style.setProperty('--primary', t.primary);
      root.style.setProperty('--primary-glow', t.glow);
      root.style.setProperty('--btn-bg', t.btnBg);
      root.style.setProperty('--btn-border', t.btnBorder);
      root.style.setProperty('--border', currentThemeMode === 'light' ? t.cardBorderLight : t.cardBorderDark);

      document.querySelectorAll('#themePicker .color-swatch-pill').forEach(pill => {
        if (pill.getAttribute('data-theme') === themeKey) {
          pill.classList.add('selected');
        } else {
          pill.classList.remove('selected');
        }
      });

      try {
        localStorage.setItem('castpad_theme', themeKey);
      } catch (e) {}

      if (persist && ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'set_theme', theme: themeKey }));
      }
    }

    try {
      const savedTheme = localStorage.getItem('castpad_theme');
      if (savedTheme && APP_THEMES[savedTheme]) {
        setAppTheme(savedTheme, false);
      } else {
        setAppTheme('cyan', false);
      }
    } catch (e) {}

    let currentThemeMode = 'dark';

    function setThemeMode(mode, persist = true) {
      if (mode !== 'light' && mode !== 'dark') mode = 'dark';
      currentThemeMode = mode;
      document.documentElement.setAttribute('data-theme-mode', mode);

      const t = APP_THEMES[currentAppTheme] || APP_THEMES.cyan;
      document.documentElement.style.setProperty('--border', mode === 'light' ? t.cardBorderLight : t.cardBorderDark);

      const btnDark = document.getElementById('btnModeDark');
      const btnLight = document.getElementById('btnModeLight');
      if (btnDark && btnLight) {
        if (mode === 'light') {
          btnLight.classList.add('active');
          btnDark.classList.remove('active');
        } else {
          btnDark.classList.add('active');
          btnLight.classList.remove('active');
        }
      }

      const metaThemeColor = document.querySelector('meta[name="theme-color"]');
      if (metaThemeColor) {
        metaThemeColor.setAttribute('content', mode === 'light' ? '#f8fafc' : '#0b0f19');
      }

      try {
        localStorage.setItem('castpad_mode', mode);
      } catch (e) {}

      if (persist && ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'set_theme_mode', mode: mode }));
      }
    }

    try {
      const savedMode = localStorage.getItem('castpad_mode');
      if (savedMode === 'light' || savedMode === 'dark') {
        setThemeMode(savedMode, false);
      }
    } catch (e) {}

    try {
      const savedPs = localStorage.getItem('castpad_powersave');
      const psCheck = document.getElementById('cfg-power-save');
      if (psCheck && savedPs !== null) {
        psCheck.checked = (savedPs === '1');
      }
      const savedDisLed = localStorage.getItem('castpad_disable_led');
      const disLedCheck = document.getElementById('cfg-disable-led');
      if (disLedCheck && savedDisLed !== null) {
        disLedCheck.checked = (savedDisLed === '1');
      }
    } catch (e) {}

    let reconnectTimeout = null;

    function initWebSocket() {
      if (reconnectTimeout) {
        clearTimeout(reconnectTimeout);
        reconnectTimeout = null;
      }
      if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
        return;
      }
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${protocol}//${window.location.host}/ws`;

      ws = new WebSocket(wsUrl);

      ws.onopen = () => {
        updateBleStatus(false);
        ws.send(JSON.stringify({ type: 'get_settings' }));
      };

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          if (msg.type === 'status') {
            if (msg.usb_mode !== undefined) {
              updateUsbModeUI(msg.usb_mode);
            }
            if (msg.ble_radio !== undefined) {
              updateBleRadioUI(msg.ble_radio);
            }
            if (msg.rssi !== undefined) {
              updateWifiRssiUI(msg.rssi, msg.ap_mode);
            }
            updateBleStatus(msg.ble_connected);
          } else if (msg.type === 'settings') {
            applySettings(msg);
          }
        } catch (e) {}
      };

      ws.onclose = () => {
        if (!reconnectTimeout) {
          reconnectTimeout = setTimeout(initWebSocket, 2000);
        }
      };
      ws.onerror = () => {
        try { ws.close(); } catch (e) {}
      };
    }

    // Instant reconnect on phone unlock / tab focus
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
          initWebSocket();
        }
      }
    });
    window.addEventListener('focus', () => {
      if (!ws || ws.readyState !== WebSocket.OPEN) {
        initWebSocket();
      }
    });

    function updateWifiRssiUI(rssi, isAp) {
      const el = document.getElementById('wifiRssiValue');
      if (!el) return;
      const val = Number(rssi);
      if (isAp || !val || val >= 0) {
        el.textContent = 'Direct AP Hotspot';
        el.style.color = 'var(--primary)';
        return;
      }
      const pct = Math.max(0, Math.min(100, 2 * (val + 100)));
      let quality = 'Weak';
      if (val >= -55) quality = 'Excellent';
      else if (val >= -67) quality = 'Good';
      else if (val >= -75) quality = 'Fair';
      el.textContent = `${val} dBm (${quality} · ${pct}%)`;
      el.style.color = val < -75 ? '#f87171' : (val < -67 ? '#facc15' : 'var(--primary)');
    }

    function updateUsbModeUI(usb) {
      usbModeEnabled = !!usb;
      const btnBle = document.getElementById('btnOutputBle');
      const btnUsb = document.getElementById('btnOutputUsb');
      if (btnBle && btnUsb) {
        if (usbModeEnabled) {
          btnUsb.classList.add('active');
          btnBle.classList.remove('active');
        } else {
          btnBle.classList.add('active');
          btnUsb.classList.remove('active');
        }
      }
      try { localStorage.setItem('castpad_usb_mode', usbModeEnabled ? '1' : '0'); } catch (e) {}
      updateBleRadioUI(bleRadioEnabled);
    }

    function setControllerOutputMode(usb) {
      const targetUsb = !!usb;
      if (targetUsb === usbModeEnabled) return;
      haptic();
      openConfirmModal(
        targetUsb ? 'Switch to Wired USB Mode?' : 'Switch to Bluetooth (BLE) Mode?',
        targetUsb
          ? 'CastPad will reboot as a direct plug-and-play Wired USB HID Keyboard, Mouse & Media Remote over the USB-C port, and turn off the Bluetooth radio.\n\nProceed and reboot?'
          : 'CastPad will reboot in wireless Bluetooth (BLE) mode.\n\nProceed and reboot?',
        targetUsb ? 'Switch to USB' : 'Switch to BLE',
        false,
        () => {
          updateUsbModeUI(targetUsb);
          if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'set_usb_mode', enabled: targetUsb }));
            showToast(targetUsb ? 'Rebooting into Wired USB mode...' : 'Rebooting into BLE mode...');
          }
        }
      );
    }

    function updateBleRadioUI(enabled) {
      bleRadioEnabled = !!enabled;
      const rBtn = document.getElementById('btnBleRadio');
      const dBtn = document.getElementById('btnBleDisconnect');
      const fBtn = document.getElementById('btnBleForget');

      if (rBtn) {
        if (usbModeEnabled) {
          rBtn.textContent = 'BLE Disabled (Wired USB Mode)';
          rBtn.className = 'btn-secondary';
          rBtn.disabled = true;
          rBtn.style.opacity = '0.4';
          rBtn.style.pointerEvents = 'none';
        } else {
          rBtn.disabled = false;
          rBtn.style.opacity = '1';
          rBtn.style.pointerEvents = 'auto';
          if (bleRadioEnabled) {
            rBtn.textContent = 'Turn Off BLE Radio';
            rBtn.className = 'danger-btn';
          } else {
            rBtn.textContent = 'Turn On BLE Radio';
            rBtn.className = 'primary-btn';
          }
        }
      }
      const bleControlsActive = !usbModeEnabled && bleRadioEnabled;
      if (dBtn) {
        dBtn.disabled = !bleControlsActive;
        dBtn.style.opacity = bleControlsActive ? '1' : '0.4';
        dBtn.style.pointerEvents = bleControlsActive ? 'auto' : 'none';
      }
      if (fBtn) {
        fBtn.disabled = !bleControlsActive;
        fBtn.style.opacity = bleControlsActive ? '1' : '0.4';
        fBtn.style.pointerEvents = bleControlsActive ? 'auto' : 'none';
      }
      updateBleStatus(isBleConnected);
    }

    function toggleBleRadio() {
      if (usbModeEnabled) return;
      haptic();
      const nextState = !bleRadioEnabled;
      updateBleRadioUI(nextState);
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'toggle_ble_radio', enabled: nextState }));
      }
      showToast(nextState ? 'BLE Radio turned ON' : 'BLE Radio turned OFF');
    }

    function updateBleStatus(connected) {
      if (connected !== undefined) {
        isBleConnected = !!connected;
      }
      const badge = document.getElementById('bleBadge');
      const text = document.getElementById('bleText');
      if (!badge || !text) return;

      badge.classList.remove('connected', 'radio-off');
      if (usbModeEnabled) {
        badge.classList.add('connected');
        text.textContent = 'USB: Active';
      } else if (!bleRadioEnabled) {
        badge.classList.add('radio-off');
        text.textContent = 'BLE: Radio Off';
      } else if (isBleConnected) {
        badge.classList.add('connected');
        text.textContent = 'BLE: Connected';
      } else {
        text.textContent = 'BLE: Pairing...';
      }
    }

    function sendAction(action) {
      if (!isControllerActive()) {
        showToast('BLE Radio is turned off in Settings');
        return;
      }
      haptic();
      flashBleTx();
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('k,' + action);
      }
    }

    function sendColorBtn(color) {
      if (Date.now() < suppressClicksUntil) return;
      if (!isControllerActive()) {
        showToast('BLE Radio is turned off in Settings');
        return;
      }
      const action = buttonActions[color];
      if (!action || action === 'NONE' || !action.trim()) {
        return;
      }
      haptic();
      flashBleTx();
      let code = color;
      if (color === 'red') code = 'r';
      else if (color === 'green') code = 'g';
      else if (color === 'yellow') code = 'y';
      else if (color === 'blue') code = 'b';
      else if (color === 'custom1') code = '1';
      else if (color === 'custom2') code = '2';
      else if (color === 'custom3') code = '3';
      else if (color === 'custom4') code = '4';
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('c,' + code);
      }
    }

    function selectModalColor(color) {
      currentSelectedModalColor = color || 'theme';
      document.querySelectorAll('#modalColorPicker .color-swatch-pill').forEach(pill => {
        if (pill.getAttribute('data-color') === currentSelectedModalColor) {
          pill.classList.add('selected');
        } else {
          pill.classList.remove('selected');
        }
      });
      const hexColor = (currentSelectedModalColor === 'theme') ? 'var(--primary)' : (QL_COLORS[currentSelectedModalColor] || 'var(--primary)');
      const swatch = document.getElementById('btnConfigSwatch');
      if (swatch) {
        swatch.style.background = hexColor;
      }
      const sheet = document.getElementById('btnConfigSheet');
      if (sheet) {
        sheet.style.borderColor = hexColor;
      }

      // Live-reflect on the screen behind the modal
      if (currentConfigTarget) {
        if (currentConfigTarget.type === 'color' || currentConfigTarget.type === 'aux') {
          const bgBtn = document.getElementById('aux-btn-' + currentConfigTarget.id) || document.querySelector('.color-btn.btn-' + currentConfigTarget.id);
          if (bgBtn) bgBtn.setAttribute('data-color', currentSelectedModalColor);
        } else if (currentConfigTarget.type === 'ql') {
          const bgQl = document.getElementById('ql-btn-' + (currentConfigTarget.id % 12));
          if (bgQl) bgQl.setAttribute('data-color', currentSelectedModalColor);
        }
      }
    }

    function selectQlColor(color) {
      selectModalColor(color);
    }

    function setModalEmoji(emoji) {
      const inp = document.getElementById('btnConfigEmoji');
      if (inp) inp.value = emoji;
    }

    function renderQuickLinksGrid() {
      const baseIdx = qlCurrentPage * 12;
      for (let s = 0; s < 12; s++) {
        const idx = baseIdx + s;
        const item = quickLinksData[idx] || { label: '', num: '', color: 'theme', icon: '' };
        const btn = document.getElementById('ql-btn-' + s);
        if (!btn) continue;

        const lbl = (item.label || '').trim();
        const icon = (item.icon || '').trim();
        const num = (item.num || '').trim();
        const hasConfig = !!(lbl || icon || num);
        const color = item.color || 'theme';
        btn.setAttribute('data-color', color);

        if (hasConfig) {
          btn.classList.remove('unconfigured');
          if (icon && lbl) {
            btn.innerHTML = `<span class="btn-emoji">${escapeHtml(icon)}</span><span class="btn-text-label">${escapeHtml(lbl)}</span>`;
          } else if (icon) {
            btn.innerHTML = `<span class="btn-emoji large">${escapeHtml(icon)}</span>`;
          } else if (lbl) {
            btn.innerHTML = `<span class="btn-text-label">${escapeHtml(lbl)}</span>`;
          } else {
            btn.innerHTML = `<span class="btn-emoji large">⚡</span>`;
          }
        } else {
          btn.classList.add('unconfigured');
          btn.innerHTML = `<span class="ql-slot-num">#${idx + 1}</span>`;
        }
      }
    }

    function renderQuickLinksPagination() {
      const container = document.getElementById('qlPagination');
      if (!container) return;
      if (qlPages <= 1) {
        container.style.display = 'none';
        container.innerHTML = '';
        return;
      }
      container.style.display = 'flex';
      let html = '';
      for (let i = 0; i < qlPages; i++) {
        const activeCls = (i === qlCurrentPage) ? 'active' : '';
        html += `<span class="ql-dot ${activeCls}" onclick="setQlPage(${i})" title="Page ${i + 1}"></span>`;
      }
      container.innerHTML = html;
    }

    function setQlPage(page) {
      if (page < 0 || page >= qlPages || page === qlCurrentPage) return;
      haptic();
      const grid = document.getElementById('qlGrid');
      if (grid) {
        grid.style.opacity = '0';
        grid.style.transform = (page > qlCurrentPage) ? 'translateX(-12px)' : 'translateX(12px)';
        setTimeout(() => {
          qlCurrentPage = page;
          renderQuickLinksGrid();
          renderQuickLinksPagination();
          grid.style.transform = (page > qlCurrentPage) ? 'translateX(12px)' : 'translateX(-12px)';
          void grid.offsetWidth;
          grid.style.opacity = '1';
          grid.style.transform = 'translateX(0)';
        }, 120);
      } else {
        qlCurrentPage = page;
        renderQuickLinksGrid();
        renderQuickLinksPagination();
      }
    }

    function onQlPagesSettingChange() {
      const sel = document.getElementById('cfg-ql-pages');
      if (!sel) return;
      const newPages = Math.min(5, Math.max(1, parseInt(sel.value, 10) || 3));
      qlPages = newPages;
      if (qlCurrentPage >= qlPages) {
        qlCurrentPage = qlPages - 1;
      }
      renderQuickLinksGrid();
      renderQuickLinksPagination();
      saveSettings(true);
      showToast('Quick Links pages set to ' + qlPages);
    }

    function onQuickLinkClick(slotIndex) {
      if (Date.now() < suppressClicksUntil) return;
      const globalIndex = qlCurrentPage * 12 + slotIndex;
      const item = quickLinksData[globalIndex];
      if (!item || !item.num || !item.num.trim()) {
        haptic();
        openButtonConfigModal({ type: 'ql', id: globalIndex });
        return;
      }
      if (!isControllerActive()) {
        showToast('BLE Radio is turned off in Settings');
        return;
      }
      haptic();
      flashBleTx();
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('q,' + globalIndex);
      }
    }

    function sendQuickLink(index) {
      onQuickLinkClick(index % 12);
    }

    // Button Config Modal Handlers (Long-Press Customization)
    // Sequence Builder Helpers
    function parseSequenceString(seqStr) {
      if (!seqStr) return [];
      const steps = [];
      const parts = seqStr.split('|');
      parts.forEach(p => {
        p = p.trim();
        if (!p) return;
        if (p.startsWith('W:') || p.startsWith('w:')) {
          steps.push({ type: 'wait', val: parseInt(p.substring(2), 10) || 500 });
        } else if (p.startsWith('K:') || p.startsWith('k:')) {
          steps.push({ type: 'key', val: p.substring(2) });
        } else if (p.startsWith('T:') || p.startsWith('t:')) {
          steps.push({ type: 'text', val: p.substring(2) });
        } else if (p.startsWith('N:') || p.startsWith('n:')) {
          steps.push({ type: 'text', val: p.substring(2) });
        } else {
          steps.push({ type: 'key', val: p });
        }
      });
      return steps;
    }

    function buildSequenceString(steps) {
      if (!steps || !steps.length) return '';
      return steps.map(s => {
        if (s.type === 'wait') return 'W:' + (parseInt(s.val, 10) || 500);
        if (s.type === 'text') return 'T:' + (s.val || '').trim();
        return 'K:' + (s.val || '').trim();
      }).join('|');
    }

    function formatStepPreview(step) {
      if (step.type === 'wait') {
        const ms = parseInt(step.val, 10) || 0;
        return (ms >= 1000) ? ((ms / 1000).toFixed(1).replace(/\.0$/, '') + 's pause') : (ms + 'ms pause');
      }
      if (step.type === 'text') {
        return 'Type "' + step.val + '"';
      }
      return step.val || 'Key';
    }

    function renderSeqFlowPreview() {
      const container = document.getElementById('seqFlowPreview');
      const modalFlow = document.getElementById('btnConfigSeqFlow');
      const modalPreviewWrap = document.getElementById('btnConfigSeqPreviewWrap');
      if (!currentSequenceSteps || currentSequenceSteps.length === 0) {
        if (container) container.innerHTML = '<span style="color:#64748b; font-size:12px;">No steps added yet</span>';
        if (modalPreviewWrap) modalPreviewWrap.style.display = 'none';
        return;
      }
      let html = '';
      currentSequenceSteps.forEach((s, idx) => {
        const pillText = formatStepPreview(s);
        const icon = s.type === 'wait' ? '⏱️' : (s.type === 'text' ? '⌨️' : '🔘');
        html += `<span class="seq-flow-pill">${icon} ${escapeHtml(pillText)}</span>`;
        if (idx < currentSequenceSteps.length - 1) {
          html += `<span class="seq-flow-arrow">➔</span>`;
        }
      });
      if (container) container.innerHTML = html;
      if (modalFlow) modalFlow.innerHTML = html;
      if (modalPreviewWrap) modalPreviewWrap.style.display = 'block';
    }

    function renderSeqSteps() {
      const container = document.getElementById('seqStepsContainer');
      if (!container) return;
      container.innerHTML = '';
      const commonKeys = [
        'HOME', 'ENTER', 'BACK', 'MENU', 'UP', 'DOWN', 'LEFT', 'RIGHT',
        'PLAY_PAUSE', 'MUTE', 'VOL_UP', 'VOL_DOWN', 'SEARCH', 'SPACE',
        'PAGE_UP', 'PAGE_DOWN', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12'
      ];
      currentSequenceSteps.forEach((step, idx) => {
        const row = document.createElement('div');
        row.className = 'seq-step-row';

        const num = document.createElement('span');
        num.className = 'seq-step-num';
        num.textContent = '#' + (idx + 1);
        row.appendChild(num);

        const contentWrap = document.createElement('div');
        contentWrap.style.flex = '1';
        contentWrap.style.display = 'flex';
        contentWrap.style.alignItems = 'center';
        contentWrap.style.gap = '8px';

        if (step.type === 'key') {
          const sel = document.createElement('select');
          sel.className = 'setting-input';
          sel.style.marginBottom = '0';
          sel.style.padding = '6px 8px';
          sel.style.fontSize = '12px';
          commonKeys.forEach(k => {
            const opt = document.createElement('option');
            opt.value = k;
            opt.textContent = k;
            if (step.val === k) opt.selected = true;
            sel.appendChild(opt);
          });
          const optCustom = document.createElement('option');
          optCustom.value = 'CUSTOM';
          optCustom.textContent = 'Custom Key...';
          if (!commonKeys.includes(step.val)) optCustom.selected = true;
          sel.appendChild(optCustom);

          const customInp = document.createElement('input');
          customInp.type = 'text';
          customInp.placeholder = 'e.g. 0x0223';
          customInp.style.width = '95px';
          customInp.style.padding = '6px 8px';
          customInp.style.fontSize = '12px';
          customInp.value = (!commonKeys.includes(step.val)) ? (step.val || '') : '';
          customInp.style.display = (!commonKeys.includes(step.val)) ? 'block' : 'none';

          sel.onchange = () => {
            if (sel.value === 'CUSTOM') {
              customInp.style.display = 'block';
              step.val = customInp.value.trim() || 'HOME';
            } else {
              customInp.style.display = 'none';
              step.val = sel.value;
            }
            renderSeqFlowPreview();
          };
          customInp.oninput = () => {
            step.val = customInp.value.trim() || 'HOME';
            renderSeqFlowPreview();
          };

          contentWrap.appendChild(sel);
          contentWrap.appendChild(customInp);
        } else if (step.type === 'wait') {
          const inp = document.createElement('input');
          inp.type = 'number';
          inp.min = '10';
          inp.max = '10000';
          inp.step = '50';
          inp.value = step.val || 500;
          inp.style.width = '75px';
          inp.style.padding = '6px 8px';
          inp.style.fontSize = '12px';
          inp.oninput = () => {
            step.val = parseInt(inp.value, 10) || 500;
            renderSeqFlowPreview();
          };

          const chipWrap = document.createElement('div');
          chipWrap.style.display = 'flex';
          chipWrap.style.gap = '4px';
          [100, 1000].forEach(ms => {
            const chip = document.createElement('button');
            chip.type = 'button';
            chip.className = 'seq-icon-btn';
            chip.style.fontSize = '11px';
            chip.textContent = (ms >= 1000 ? (ms/1000 + 's') : (ms + 'ms'));
            chip.onclick = () => {
              step.val = ms;
              inp.value = ms;
              renderSeqFlowPreview();
            };
            chipWrap.appendChild(chip);
          });

          contentWrap.appendChild(inp);
          contentWrap.appendChild(chipWrap);
        } else if (step.type === 'text') {
          const inp = document.createElement('input');
          inp.type = 'text';
          inp.placeholder = 'Text / Digits, e.g. 206';
          inp.value = step.val || '';
          inp.style.padding = '6px 8px';
          inp.style.fontSize = '12px';
          inp.style.flex = '1';
          inp.oninput = () => {
            step.val = inp.value;
            renderSeqFlowPreview();
          };
          contentWrap.appendChild(inp);
        }

        row.appendChild(contentWrap);

        const ctrls = document.createElement('div');
        ctrls.className = 'seq-step-ctrls';

        const upBtn = document.createElement('button');
        upBtn.type = 'button';
        upBtn.className = 'seq-icon-btn';
        upBtn.textContent = '▲';
        upBtn.title = 'Move Up';
        upBtn.disabled = (idx === 0);
        upBtn.onclick = () => moveSeqStep(idx, -1);
        ctrls.appendChild(upBtn);

        const downBtn = document.createElement('button');
        downBtn.type = 'button';
        downBtn.className = 'seq-icon-btn';
        downBtn.textContent = '▼';
        downBtn.title = 'Move Down';
        downBtn.disabled = (idx === currentSequenceSteps.length - 1);
        downBtn.onclick = () => moveSeqStep(idx, 1);
        ctrls.appendChild(downBtn);

        const delBtn = document.createElement('button');
        delBtn.type = 'button';
        delBtn.className = 'seq-icon-btn seq-del-btn';
        delBtn.textContent = '✕';
        delBtn.title = 'Remove Step';
        delBtn.onclick = () => removeSeqStep(idx);
        ctrls.appendChild(delBtn);

        row.appendChild(ctrls);
        container.appendChild(row);
      });
      renderSeqFlowPreview();
    }

    function addSeqStep(type) {
      if (type === 'wait') {
        currentSequenceSteps.push({ type: 'wait', val: 500 });
      } else if (type === 'text') {
        currentSequenceSteps.push({ type: 'text', val: '' });
      } else {
        currentSequenceSteps.push({ type: 'key', val: 'HOME' });
      }
      renderSeqSteps();
    }

    function removeSeqStep(idx) {
      currentSequenceSteps.splice(idx, 1);
      renderSeqSteps();
    }

    function moveSeqStep(idx, dir) {
      const target = idx + dir;
      if (target < 0 || target >= currentSequenceSteps.length) return;
      const tmp = currentSequenceSteps[idx];
      currentSequenceSteps[idx] = currentSequenceSteps[target];
      currentSequenceSteps[target] = tmp;
      renderSeqSteps();
    }

    function openSequenceBuilderModal() {
      haptic();
      if (!currentSequenceSteps || currentSequenceSteps.length === 0) {
        const customInput = document.getElementById('btnConfigCustom');
        const customVal = customInput ? customInput.value.trim() : '';
        if (customVal.startsWith('SEQ:')) {
          currentSequenceSteps = parseSequenceString(customVal.substring(4));
        } else if (customVal) {
          currentSequenceSteps = [{ type: 'key', val: customVal }];
        } else {
          currentSequenceSteps = [
            { type: 'key', val: 'HOME' },
            { type: 'wait', val: 500 }
          ];
        }
      }
      renderSeqSteps();
      document.getElementById('sequenceModal').classList.add('open');
    }

    function closeSequenceBuilderModal(e) {
      if (e && e.target !== document.getElementById('sequenceModal')) return;
      document.getElementById('sequenceModal').classList.remove('open');
    }

    function applySequenceBuilder() {
      haptic();
      document.getElementById('sequenceModal').classList.remove('open');
      renderSeqFlowPreview();
      const customWrap = document.getElementById('btnConfigCustomWrap');
      if (customWrap) customWrap.style.display = 'none';
      const sel = document.getElementById('btnConfigAction');
      if (sel) sel.value = 'CUSTOM';
    }

    // Unified Button Config Modal Handlers (Long-Press Customization)
    function openButtonConfigModal(target) {
      currentConfigTarget = target;
      const modal = document.getElementById('btnConfigModal');
      const swatch = document.getElementById('btnConfigSwatch');
      const title = document.getElementById('btnConfigTitle');
      const labelInput = document.getElementById('btnConfigLabel');
      const emojiInput = document.getElementById('btnConfigEmoji');
      const sel = document.getElementById('btnConfigAction');
      const customWrap = document.getElementById('btnConfigCustomWrap');
      const customInput = document.getElementById('btnConfigCustom');

      let name = '';
      let existingLabel = '';
      let existingEmoji = '';
      let existingColor = 'theme';
      let existingAction = '';

      if (target.type === 'color' || target.type === 'aux') {
        if (target.id === 'red') name = 'Red Button';
        else if (target.id === 'green') name = 'Green Button';
        else if (target.id === 'yellow') name = 'Yellow Button';
        else if (target.id === 'blue') name = 'Blue Button';
        else if (target.id === 'custom1') name = 'Custom 1';
        else if (target.id === 'custom2') name = 'Custom 2';
        else if (target.id === 'custom3') name = 'Custom 3';
        else if (target.id === 'custom4') name = 'Custom 4';

        existingLabel = buttonLabels[target.id] || '';
        existingEmoji = buttonIcons[target.id] || '';
        existingColor = buttonColors[target.id] || 'theme';
        existingAction = buttonActions[target.id] || '';
      } else if (target.type === 'ql') {
        name = 'Quick Link #' + (target.id + 1);
        const cur = quickLinksData[target.id] || { label: '', num: '', color: 'theme', icon: '' };
        existingLabel = cur.label || '';
        existingEmoji = cur.icon || '';
        existingColor = cur.color || 'theme';
        existingAction = cur.num || '';
      }

      selectModalColor(existingColor);
      title.replaceChildren(swatch, document.createTextNode('Configure ' + name));
      labelInput.value = existingLabel;
      emojiInput.value = existingEmoji;

      if (existingAction.startsWith('SEQ:')) {
        currentSequenceSteps = parseSequenceString(existingAction.substring(4));
        sel.value = 'CUSTOM';
        customWrap.style.display = 'none';
        customInput.value = existingAction;
        renderSeqFlowPreview();
      } else {
        currentSequenceSteps = [];
        renderSeqFlowPreview();
        const presetMatch = Array.from(sel.options).some(opt => opt.value !== 'CUSTOM' && opt.value === existingAction);
        if (presetMatch) {
          sel.value = existingAction || 'NONE';
          customWrap.style.display = 'none';
          customInput.value = '';
        } else {
          sel.value = 'CUSTOM';
          customWrap.style.display = 'block';
          customInput.value = existingAction;
        }
      }

      modal.classList.add('open');
    }

    function closeButtonConfigModal(e) {
      if (e && e.target !== document.getElementById('btnConfigModal')) return;
      if (e && Date.now() < suppressClicksUntil) return;
      if (currentConfigTarget) {
        if (currentConfigTarget.type === 'color' || currentConfigTarget.type === 'aux') {
          const bgBtn = document.getElementById('aux-btn-' + currentConfigTarget.id) || document.querySelector('.color-btn.btn-' + currentConfigTarget.id);
          if (bgBtn) bgBtn.setAttribute('data-color', buttonColors[currentConfigTarget.id] || 'theme');
        } else if (currentConfigTarget.type === 'ql') {
          const bgQl = document.getElementById('ql-btn-' + (currentConfigTarget.id % 12));
          if (bgQl) bgQl.setAttribute('data-color', (quickLinksData[currentConfigTarget.id] && quickLinksData[currentConfigTarget.id].color) || 'theme');
        }
      }
      document.getElementById('btnConfigModal').classList.remove('open');
      currentConfigTarget = null;
    }

    function onBtnConfigActionChange() {
      const sel = document.getElementById('btnConfigAction');
      const customWrap = document.getElementById('btnConfigCustomWrap');
      if (sel.value === 'CUSTOM') {
        customWrap.style.display = 'block';
      } else {
        customWrap.style.display = 'none';
        currentSequenceSteps = [];
        renderSeqFlowPreview();
      }
    }

    function saveButtonConfig() {
      if (!currentConfigTarget) return;
      const target = currentConfigTarget;
      const newLbl = document.getElementById('btnConfigLabel').value.trim();
      const newEmoji = document.getElementById('btnConfigEmoji').value.trim();
      const selVal = document.getElementById('btnConfigAction').value;
      const customVal = document.getElementById('btnConfigCustom').value.trim();

      let finalAction = '';
      if (currentSequenceSteps && currentSequenceSteps.length > 0) {
        finalAction = 'SEQ:' + buildSequenceString(currentSequenceSteps);
      } else if (selVal === 'CUSTOM') {
        finalAction = customVal;
      } else {
        finalAction = selVal;
      }

      if (target.type === 'color' || target.type === 'aux') {
        buttonLabels[target.id] = newLbl;
        buttonIcons[target.id] = newEmoji;
        buttonActions[target.id] = finalAction;
        buttonColors[target.id] = currentSelectedModalColor;
        updateButtonDisplay(target.id);
      } else if (target.type === 'ql') {
        quickLinksData[target.id] = {
          label: newLbl,
          num: finalAction,
          color: currentSelectedModalColor,
          icon: newEmoji
        };
        renderQuickLinksGrid();
      }

      if (ws && ws.readyState === WebSocket.OPEN) {
        if (target.type === 'color' || target.type === 'aux') {
          ws.send(JSON.stringify({
            type: 'save_button',
            id: target.id,
            action: finalAction,
            label: newLbl,
            icon: newEmoji,
            color: currentSelectedModalColor
          }));
        } else if (target.type === 'ql') {
          ws.send(JSON.stringify({
            type: 'save_quick_link',
            index: target.id,
            label: newLbl,
            num: finalAction,
            color: currentSelectedModalColor,
            icon: newEmoji
          }));
        }
      }

      cacheButtonsLocally();
      closeButtonConfigModal();
      saveSettings(true);
      showToast('Button updated & saved!');
    }

    function clearButtonConfig() {
      if (!currentConfigTarget) return;
      const target = currentConfigTarget;
      haptic();
      document.getElementById('btnConfigLabel').value = '';
      document.getElementById('btnConfigEmoji').value = '';
      const defaultAction = (target.type === 'color') ? ('F' + (['red','green','yellow','blue'].indexOf(target.id)+1)) : 'NONE';
      document.getElementById('btnConfigAction').value = defaultAction;
      document.getElementById('btnConfigCustom').value = '';
      document.getElementById('btnConfigCustomWrap').style.display = 'none';
      currentSequenceSteps = [];
      renderSeqFlowPreview();
      selectModalColor('theme');
    }

    function initLongPressCustomization() {
      const targets = [
        { sel: '.color-btn.btn-red', type: 'color', id: 'red' },
        { sel: '.color-btn.btn-green', type: 'color', id: 'green' },
        { sel: '.color-btn.btn-yellow', type: 'color', id: 'yellow' },
        { sel: '.color-btn.btn-blue', type: 'color', id: 'blue' },
        { sel: '#aux-btn-custom1', type: 'aux', id: 'custom1' },
        { sel: '#aux-btn-custom2', type: 'aux', id: 'custom2' },
        { sel: '#aux-btn-custom3', type: 'aux', id: 'custom3' },
        { sel: '#aux-btn-custom4', type: 'aux', id: 'custom4' }
      ];
      for (let i = 0; i < 12; i++) {
        targets.push({ sel: '#ql-btn-' + i, type: 'ql', id: i });
      }

      targets.forEach(({ sel, type, id }) => {
        const el = document.querySelector(sel);
        if (!el) return;
        let timer = null;
        let startX = 0, startY = 0;

        const start = (e) => {
          if (e.type === 'touchstart') {
            startX = e.touches[0].clientX;
            startY = e.touches[0].clientY;
          }
          el.classList.add('long-pressing');
          if (timer) clearTimeout(timer);
          timer = setTimeout(() => {
            timer = null;
            el.classList.remove('long-pressing');
            suppressClicksUntil = Date.now() + 600;
            if (navigator.vibrate) navigator.vibrate(60);
            const targetId = (type === 'ql') ? (qlCurrentPage * 12 + id) : id;
            openButtonConfigModal({ type, id: targetId });
          }, 500);
        };

        const cancel = (e) => {
          if (e && e.type === 'touchmove') {
            const moveX = Math.abs(e.touches[0].clientX - startX);
            const moveY = Math.abs(e.touches[0].clientY - startY);
            if (moveX < 10 && moveY < 10) return;
          }
          if (timer) {
            clearTimeout(timer);
            timer = null;
          }
          el.classList.remove('long-pressing');
        };

        el.addEventListener('contextmenu', (e) => e.preventDefault());
        el.addEventListener('mousedown', start);
        el.addEventListener('mouseup', cancel);
        el.addEventListener('mouseleave', cancel);
        el.addEventListener('touchstart', start, { passive: true });
        el.addEventListener('touchend', cancel, { passive: true });
        el.addEventListener('touchcancel', cancel, { passive: true });
        el.addEventListener('touchmove', cancel, { passive: true });
      });
    }

    function initQuickLinksGestures() {
      const grid = document.getElementById('qlGrid');
      if (!grid) return;
      let startX = 0, startY = 0, isSwipe = false;

      grid.addEventListener('touchstart', (e) => {
        if (e.touches.length === 1) {
          startX = e.touches[0].clientX;
          startY = e.touches[0].clientY;
          isSwipe = true;
        }
      }, { passive: true });

      grid.addEventListener('touchend', (e) => {
        if (!isSwipe || !e.changedTouches.length) return;
        isSwipe = false;
        const dx = e.changedTouches[0].clientX - startX;
        const dy = e.changedTouches[0].clientY - startY;
        if (Math.abs(dx) > 40 && Math.abs(dx) > Math.abs(dy) * 1.5) {
          if (dx < 0 && qlCurrentPage < qlPages - 1) {
            setQlPage(qlCurrentPage + 1);
          } else if (dx > 0 && qlCurrentPage > 0) {
            setQlPage(qlCurrentPage - 1);
          }
        }
      }, { passive: true });
    }

    function sendTextRaw(text) {
      if (!text) return;
      if (!isControllerActive()) {
        showToast('BLE Radio is turned off in Settings');
        return;
      }
      flashBleTx();
      if (ws && ws.readyState === WebSocket.OPEN) {
        if (text.length < 56) {
          ws.send('t,' + text);
        } else {
          ws.send(JSON.stringify({ type: 'type_text', text: text, enter: false }));
        }
      }
    }

    // Modal Keyboard Handlers (Dual-Mode Live & Batch + Quick Tools)
    let kbMode = 'live';
    let kbLastLiveValue = '';
    let kbPendingEcho = '';
    let kbEchoExpireAt = 0;
    let kbBkspRepeatTimer = null;
    let kbBkspRepeatInterval = null;

    function setKeyboardMode(mode, persist = true) {
      kbMode = (mode === 'batch') ? 'batch' : 'live';
      const btnLive = document.getElementById('kbModeLive');
      const btnBatch = document.getElementById('kbModeBatch');
      const batchRow = document.getElementById('kbBatchRow');
      const hint = document.getElementById('kbHintText');
      const input = document.getElementById('kbTextInput');

      if (btnLive && btnBatch) {
        if (kbMode === 'live') {
          btnLive.classList.add('active');
          btnBatch.classList.remove('active');
        } else {
          btnBatch.classList.add('active');
          btnLive.classList.remove('active');
        }
      }
      if (batchRow) {
        batchRow.style.display = (kbMode === 'batch') ? 'flex' : 'none';
      }
      if (hint) {
        if (kbMode === 'live') {
          hint.innerHTML = '<span class="kb-live-dot"></span><span>Live: keys &amp; voice dictation stream directly to your TV</span>';
        } else {
          hint.innerHTML = '<span>Batch: type or paste text below, then tap Send</span>';
        }
      }
      if (input) {
        input.placeholder = (kbMode === 'live')
          ? 'Type or dictate — sends live to TV...'
          : 'Type or paste text, then tap Send...';
        kbLastLiveValue = input.value;
      }
      if (persist) {
        haptic();
        try { localStorage.setItem('castpad_kb_mode', kbMode); } catch (e) {}
      }
    }

    function updateKbClearBtnVisibility() {
      const input = document.getElementById('kbTextInput');
      const clearBtn = document.getElementById('kbLocalClearBtn');
      if (input && clearBtn) {
        clearBtn.style.display = input.value.length > 0 ? 'flex' : 'none';
      }
    }

    function clearLocalKbInput() {
      haptic();
      const input = document.getElementById('kbTextInput');
      if (input) {
        input.value = '';
        kbLastLiveValue = '';
        updateKbClearBtnVisibility();
        input.focus();
      }
    }

    function openKeyboardModal() {
      haptic();
      try {
        const saved = localStorage.getItem('castpad_kb_mode');
        if (saved === 'live' || saved === 'batch') {
          setKeyboardMode(saved, false);
        } else {
          setKeyboardMode('live', false);
        }
      } catch (e) {
        setKeyboardMode('live', false);
      }
      document.getElementById('kbModal').classList.add('open');
      const input = document.getElementById('kbTextInput');
      if (input) {
        input.value = '';
        kbLastLiveValue = '';
        updateKbClearBtnVisibility();
        setTimeout(() => input.focus(), 80);
      }
    }

    function closeKeyboardModal(e) {
      if (e && e.target !== document.getElementById('kbModal')) return;
      stopBkspRepeat();
      document.getElementById('kbModal').classList.remove('open');
    }

    function handleKeyboardSend(withEnter) {
      if (!isControllerActive()) {
        showToast('BLE Radio is turned off in Settings');
        return;
      }
      const input = document.getElementById('kbTextInput');
      if (!input) return;

      if (kbMode === 'live') {
        if (withEnter) {
          sendAction('ENTER');
          input.value = '';
          kbLastLiveValue = '';
          updateKbClearBtnVisibility();
          showToast('Enter ↵ sent');
        }
        input.focus();
        return;
      }

      const text = input.value;
      if (text && ws && ws.readyState === WebSocket.OPEN) {
        haptic();
        flashBleTx();
        ws.send(JSON.stringify({ type: 'type_text', text: text, enter: !!withEnter }));
        input.value = '';
        kbLastLiveValue = '';
        updateKbClearBtnVisibility();
        showToast(withEnter ? 'Sent text + Enter!' : 'Sent text to TV!');
      } else if (withEnter) {
        sendAction('ENTER');
        showToast('Enter ↵ sent');
      }
      input.focus();
    }

    function handleKeyboardSubmit(e) {
      if (e) e.preventDefault();
      handleKeyboardSend(true);
    }

    function kbTapKey(action) {
      if (!isControllerActive()) {
        showToast('BLE Radio is turned off in Settings');
        return;
      }
      const input = document.getElementById('kbTextInput');
      if (action === 'SPACE') {
        if (kbMode === 'batch' && input) {
          input.value += ' ';
          updateKbClearBtnVisibility();
          input.focus();
          return;
        }
        if (input && kbMode === 'live') {
          input.value += ' ';
          kbLastLiveValue = input.value;
          updateKbClearBtnVisibility();
        }
      } else if (action === 'ENTER' && input && kbMode === 'live') {
        input.value = '';
        kbLastLiveValue = '';
        updateKbClearBtnVisibility();
      }
      sendAction(action);
      if (input) input.focus();
    }

    function kbTapNav(action) {
      sendAction(action);
    }

    function kbClearTvField() {
      if (!isControllerActive()) {
        showToast('BLE Radio is turned off in Settings');
        return;
      }
      haptic();
      const input = document.getElementById('kbTextInput');
      if (input) {
        input.value = '';
        kbLastLiveValue = '';
        updateKbClearBtnVisibility();
      }
      sendAction('CLEAR_FIELD');
      showToast('Clearing TV text field...');
      if (input) input.focus();
    }

    function kbInsertChip(chipText) {
      if (!chipText) return;
      haptic();
      const input = document.getElementById('kbTextInput');
      if (kbMode === 'live') {
        if (input) {
          input.value += chipText;
          kbLastLiveValue = input.value;
          updateKbClearBtnVisibility();
        }
        sendTextRaw(chipText);
      } else if (input) {
        const start = input.selectionStart !== null ? input.selectionStart : input.value.length;
        const end = input.selectionEnd !== null ? input.selectionEnd : input.value.length;
        input.value = input.value.slice(0, start) + chipText + input.value.slice(end);
        const newPos = start + chipText.length;
        try { input.setSelectionRange(newPos, newPos); } catch (_) {}
        updateKbClearBtnVisibility();
      }
      if (input) input.focus();
    }

    function kbStepBackspace() {
      if (!isControllerActive()) return;
      const input = document.getElementById('kbTextInput');
      if (kbMode === 'batch' && input && input.value.length > 0) {
        input.value = input.value.slice(0, -1);
        updateKbClearBtnVisibility();
        return;
      }
      if (input && kbMode === 'live' && input.value.length > 0) {
        input.value = input.value.slice(0, -1);
        kbLastLiveValue = input.value;
        updateKbClearBtnVisibility();
      }
      sendAction('BACKSPACE');
    }

    function stopBkspRepeat() {
      if (kbBkspRepeatTimer) {
        clearTimeout(kbBkspRepeatTimer);
        kbBkspRepeatTimer = null;
      }
      if (kbBkspRepeatInterval) {
        clearInterval(kbBkspRepeatInterval);
        kbBkspRepeatInterval = null;
      }
    }

    function initKeyboardModalEvents() {
      const input = document.getElementById('kbTextInput');
      if (input) {
        input.addEventListener('keydown', (e) => {
          if (kbMode !== 'live') return;
          if (e.key === 'Backspace' && input.value.length === 0) {
            e.preventDefault();
            sendAction('BACKSPACE');
          } else if (e.key === 'Tab') {
            e.preventDefault();
            sendAction('TAB');
          }
        });

        input.addEventListener('input', () => {
          updateKbClearBtnVisibility();
          if (kbMode !== 'live') return;
          const cur = input.value;
          const prev = kbLastLiveValue;
          if (cur === prev) return;

          let pLen = 0;
          const minLen = Math.min(prev.length, cur.length);
          while (pLen < minLen && prev.charCodeAt(pLen) === cur.charCodeAt(pLen)) {
            pLen++;
          }
          const deletedCount = prev.length - pLen;
          const addedText = cur.slice(pLen);

          // Guard against USB/BLE loopback echo when the web UI is open on the
          // same computer that the CastPad USB/BLE keyboard is typing into
          if (deletedCount === 0 && addedText.length > 0 && kbPendingEcho.length > 0 && Date.now() < kbEchoExpireAt) {
            if (kbPendingEcho.startsWith(addedText)) {
              kbPendingEcho = kbPendingEcho.slice(addedText.length);
              input.value = prev;
              updateKbClearBtnVisibility();
              return;
            }
          }

          kbLastLiveValue = cur;

          for (let i = 0; i < deletedCount; i++) {
            sendAction('BACKSPACE');
          }
          if (addedText.length > 0) {
            kbPendingEcho += addedText;
            kbEchoExpireAt = Date.now() + 450;
            sendTextRaw(addedText);
          }
        });
      }

      const bkspBtn = document.getElementById('kbBtnBackspace');
      if (bkspBtn) {
        const startHold = (e) => {
          if (e.cancelable) e.preventDefault();
          stopBkspRepeat();
          kbStepBackspace();
          kbBkspRepeatTimer = setTimeout(() => {
            kbBkspRepeatInterval = setInterval(() => {
              kbStepBackspace();
            }, 95);
          }, 360);
        };
        const endHold = () => {
          stopBkspRepeat();
        };
        if (window.PointerEvent) {
          bkspBtn.addEventListener('pointerdown', startHold);
          bkspBtn.addEventListener('pointerup', endHold);
          bkspBtn.addEventListener('pointercancel', endHold);
          bkspBtn.addEventListener('pointerleave', endHold);
        } else {
          bkspBtn.addEventListener('touchstart', startHold, { passive: false });
          bkspBtn.addEventListener('touchend', endHold);
          bkspBtn.addEventListener('touchcancel', endHold);
          bkspBtn.addEventListener('mousedown', startHold);
          bkspBtn.addEventListener('mouseup', endHold);
          bkspBtn.addEventListener('mouseleave', endHold);
        }
      }
    }

    // Trackpad Handlers with sub-pixel remainder preservation + immediate 14ms throttle
    const trackpad = document.getElementById('trackpad');
    let tpActive = false;
    let lastX = 0, lastY = 0;
    let remX = 0, remY = 0;
    let touchStartTime = 0;
    let touchDistance = 0;
    let accumDx = 0, accumDy = 0;
    let lastMouseSendTime = 0;
    let mouseFlushTimer = null;

    function flushTrackpadMove() {
      if (mouseFlushTimer) {
        clearTimeout(mouseFlushTimer);
        mouseFlushTimer = null;
      }
      if (!isControllerActive()) {
        accumDx = 0;
        accumDy = 0;
        return;
      }
      if ((accumDx !== 0 || accumDy !== 0) && ws && ws.readyState === WebSocket.OPEN) {
        flashBleTx();
        ws.send(`m,${accumDx},${accumDy}`);
        accumDx = 0;
        accumDy = 0;
        lastMouseSendTime = Date.now();
      }
    }

    function onTrackpadStart(clientX, clientY) {
      tpActive = true;
      lastX = clientX;
      lastY = clientY;
      remX = 0;
      remY = 0;
      touchStartTime = Date.now();
      touchDistance = 0;
      accumDx = 0;
      accumDy = 0;
    }

    function onTrackpadMove(clientX, clientY) {
      if (!tpActive) return;
      const rawStepX = clientX - lastX;
      const rawStepY = clientY - lastY;
      lastX = clientX;
      lastY = clientY;
      touchDistance += Math.abs(rawStepX) + Math.abs(rawStepY);

      const scaledX = rawStepX * sensitivity + remX;
      const scaledY = rawStepY * sensitivity + remY;
      const dx = Math.trunc(scaledX);
      const dy = Math.trunc(scaledY);
      remX = scaledX - dx;
      remY = scaledY - dy;

      accumDx += dx;
      accumDy += dy;

      if (accumDx !== 0 || accumDy !== 0) {
        const now = Date.now();
        if (now - lastMouseSendTime >= 14) {
          flushTrackpadMove();
        } else if (!mouseFlushTimer) {
          mouseFlushTimer = setTimeout(flushTrackpadMove, 14);
        }
      }
    }

    function onTrackpadEnd(clientX, clientY) {
      if (!tpActive) return;
      tpActive = false;
      const duration = Date.now() - touchStartTime;
      if (duration < 260 && touchDistance < 10) {
        if (mouseFlushTimer) {
          clearTimeout(mouseFlushTimer);
          mouseFlushTimer = null;
        }
        accumDx = 0;
        accumDy = 0;
        if (!isControllerActive()) {
          showToast('BLE Radio is turned off in Settings');
          return;
        }
        haptic();
        flashBleTx();
        spawnRipple(trackpad, clientX !== undefined ? clientX : lastX, clientY !== undefined ? clientY : lastY);
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send('mc,left');
        }
      } else {
        flushTrackpadMove();
      }
    }

    trackpad.addEventListener('touchstart', (e) => {
      e.preventDefault();
      const t = e.touches[0];
      onTrackpadStart(t.clientX, t.clientY);
    }, { passive: false });

    trackpad.addEventListener('touchmove', (e) => {
      e.preventDefault();
      const t = e.touches[0];
      onTrackpadMove(t.clientX, t.clientY);
    }, { passive: false });

    trackpad.addEventListener('touchend', (e) => {
      e.preventDefault();
      const ct = (e.changedTouches && e.changedTouches[0]) ? e.changedTouches[0] : null;
      onTrackpadEnd(ct ? ct.clientX : lastX, ct ? ct.clientY : lastY);
    }, { passive: false });

    trackpad.addEventListener('touchcancel', () => {
      tpActive = false;
    }, { passive: true });

    if (window.PointerEvent) {
      trackpad.addEventListener('pointerdown', (e) => {
        if (e.pointerType === 'touch' || (e.pointerType === 'mouse' && e.button !== 0)) return;
        if (trackpad.setPointerCapture && e.pointerId !== undefined) {
          try { trackpad.setPointerCapture(e.pointerId); } catch (_) {}
        }
        onTrackpadStart(e.clientX, e.clientY);
      });
      trackpad.addEventListener('pointermove', (e) => {
        if (e.pointerType === 'touch' || !tpActive) return;
        onTrackpadMove(e.clientX, e.clientY);
      });
      trackpad.addEventListener('pointerup', (e) => {
        if (e.pointerType === 'touch' || !tpActive) return;
        onTrackpadEnd(e.clientX, e.clientY);
      });
      trackpad.addEventListener('pointercancel', (e) => {
        if (e.pointerType === 'touch') return;
        tpActive = false;
      });
    } else {
      trackpad.addEventListener('mousedown', (e) => {
        if (e.button !== 0) return;
        onTrackpadStart(e.clientX, e.clientY);
      });
      window.addEventListener('mousemove', (e) => {
        if (tpActive) onTrackpadMove(e.clientX, e.clientY);
      });
      window.addEventListener('mouseup', (e) => {
        if (tpActive) onTrackpadEnd(e.clientX, e.clientY);
      });
    }

    // Scroll Strip Handlers with sub-pixel remainder preservation
    const scrollStrip = document.getElementById('scrollStrip');
    let scrollActive = false;
    let lastScrollY = 0;
    let remScroll = 0;
    let accumScroll = 0;
    let lastScrollSendTime = 0;
    let scrollFlushTimer = null;

    function flushScrollMove() {
      if (scrollFlushTimer) {
        clearTimeout(scrollFlushTimer);
        scrollFlushTimer = null;
      }
      if (accumScroll !== 0 && ws && ws.readyState === WebSocket.OPEN) {
        flashBleTx();
        ws.send(`s,${accumScroll}`);
        accumScroll = 0;
        lastScrollSendTime = Date.now();
      }
    }

    function onScrollStart(clientY) {
      scrollActive = true;
      scrollStrip.classList.add('active');
      lastScrollY = clientY;
      remScroll = 0;
      accumScroll = 0;
    }

    function onScrollMove(clientY) {
      if (!scrollActive) return;
      const scaled = (lastScrollY - clientY) * 0.2 + remScroll;
      lastScrollY = clientY;
      const dy = Math.trunc(scaled);
      remScroll = scaled - dy;
      if (dy !== 0) {
        accumScroll += dy;
        const now = Date.now();
        if (now - lastScrollSendTime >= 20) {
          flushScrollMove();
        } else if (!scrollFlushTimer) {
          scrollFlushTimer = setTimeout(flushScrollMove, 20);
        }
      }
    }

    function onScrollEnd() {
      if (!scrollActive) return;
      scrollActive = false;
      scrollStrip.classList.remove('active');
      flushScrollMove();
    }

    scrollStrip.addEventListener('touchstart', (e) => {
      e.preventDefault();
      onScrollStart(e.touches[0].clientY);
    }, { passive: false });

    scrollStrip.addEventListener('touchmove', (e) => {
      e.preventDefault();
      onScrollMove(e.touches[0].clientY);
    }, { passive: false });

    scrollStrip.addEventListener('touchend', onScrollEnd, { passive: true });
    scrollStrip.addEventListener('touchcancel', onScrollEnd, { passive: true });

    if (window.PointerEvent) {
      scrollStrip.addEventListener('pointerdown', (e) => {
        if (e.pointerType === 'touch' || (e.pointerType === 'mouse' && e.button !== 0)) return;
        if (scrollStrip.setPointerCapture && e.pointerId !== undefined) {
          try { scrollStrip.setPointerCapture(e.pointerId); } catch (_) {}
        }
        onScrollStart(e.clientY);
      });
      scrollStrip.addEventListener('pointermove', (e) => {
        if (e.pointerType === 'touch' || !scrollActive) return;
        onScrollMove(e.clientY);
      });
      scrollStrip.addEventListener('pointerup', (e) => {
        if (e.pointerType === 'touch' || !scrollActive) return;
        onScrollEnd();
      });
      scrollStrip.addEventListener('pointercancel', (e) => {
        if (e.pointerType === 'touch') return;
        onScrollEnd();
      });
    } else {
      scrollStrip.addEventListener('mousedown', (e) => {
        if (e.button !== 0) return;
        onScrollStart(e.clientY);
      });
      window.addEventListener('mousemove', (e) => {
        if (scrollActive) onScrollMove(e.clientY);
      });
      window.addEventListener('mouseup', () => {
        if (scrollActive) onScrollEnd();
      });
    }

    // Physical Mouse Buttons
    function mouseDown(btn) {
      if (!isControllerActive()) {
        showToast('BLE Radio is turned off in Settings');
        return;
      }
      haptic();
      flashBleTx();
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('md,' + btn);
      }
    }

    function mouseUp(btn) {
      if (!isControllerActive()) return;
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('mu,' + btn);
      }
    }

    // Settings
    function applySettings(cfg) {
      if (cfg.sens !== undefined) {
        sensitivity = parseFloat(cfg.sens) || 1.3;
        const slider = document.getElementById('cfg-sens');
        if (slider) slider.value = sensitivity;
        const lbl = document.getElementById('sensValue');
        if (lbl) lbl.textContent = sensitivity.toFixed(1) + 'x';
      }
      if (cfg.apMode !== undefined) {
        const chk = document.getElementById('cfg-ap-mode');
        if (chk) chk.checked = !!cfg.apMode;
      }
      if (cfg.usbMode !== undefined) {
        updateUsbModeUI(cfg.usbMode);
      }
      if (cfg.bleRadio !== undefined) {
        updateBleRadioUI(cfg.bleRadio);
      }
      if (cfg.uptime !== undefined) {
        bootUptimeSeconds = Number(cfg.uptime) || 0;
        bootTimeLocal = performance.now();
        updateUptimeTicker();
      }

      const btnIds = ['red', 'green', 'yellow', 'blue', 'custom1', 'custom2', 'custom3', 'custom4'];
      btnIds.forEach(id => {
        const cap = id.charAt(0).toUpperCase() + id.slice(1);
        if (cfg['btn' + cap] !== undefined) buttonActions[id] = cfg['btn' + cap];
        if (cfg['lbl' + cap] !== undefined) buttonLabels[id] = cfg['lbl' + cap];
        if (cfg['icon' + cap] !== undefined) buttonIcons[id] = cfg['icon' + cap];
        if (cfg['col' + cap] !== undefined) buttonColors[id] = cfg['col' + cap];
        updateButtonDisplay(id);
      });

      if (cfg.qlPages !== undefined) {
        qlPages = Math.min(5, Math.max(1, parseInt(cfg.qlPages, 10) || 3));
        const sel = document.getElementById('cfg-ql-pages');
        if (sel) sel.value = String(qlPages);
      }

      if (Array.isArray(cfg.quickLinks)) {
        for (let i = 0; i < cfg.quickLinks.length && i < 60; i++) {
          const item = cfg.quickLinks[i] || {};
          const lbl = item.label !== undefined ? String(item.label) : '';
          const num = item.num !== undefined ? String(item.num) : '';
          const col = item.color !== undefined && item.color ? String(item.color) : 'theme';
          const icon = item.icon !== undefined ? String(item.icon) : '';
          quickLinksData[i] = { label: lbl, num: num, color: col, icon: icon };
        }
        renderQuickLinksGrid();
        renderQuickLinksPagination();
      }

      if (cfg.host) {
        document.getElementById('cfg-host').value = cfg.host;
      }
      if (cfg.ssid) {
        const sEl = document.getElementById('wifiSsidValue');
        if (sEl) sEl.textContent = cfg.ssid;
      }
      if (cfg.ip) {
        document.getElementById('ipValue').textContent = cfg.ip;
      }
      if (cfg.rssi !== undefined) {
        updateWifiRssiUI(cfg.rssi, cfg.apMode);
      }
      if (cfg.version) {
        document.getElementById('fwVersion').textContent = 'v' + cfg.version;
        const aboutFw = document.getElementById('aboutFwVersion');
        if (aboutFw) aboutFw.textContent = 'v' + cfg.version;
      }
      if (cfg.powerSave !== undefined) {
        const psCheck = document.getElementById('cfg-power-save');
        const isEnabled = Boolean(cfg.powerSave);
        if (psCheck) psCheck.checked = isEnabled;
        try { localStorage.setItem('castpad_powersave', isEnabled ? '1' : '0'); } catch (e) {}
      }
      if (cfg.ledDisabled !== undefined) {
        const disLedCheck = document.getElementById('cfg-disable-led');
        const isDis = Boolean(cfg.ledDisabled);
        if (disLedCheck) disLedCheck.checked = isDis;
        try { localStorage.setItem('castpad_disable_led', isDis ? '1' : '0'); } catch (e) {}
      }
      const localSavedTheme = localStorage.getItem('castpad_theme');
      if (localSavedTheme && APP_THEMES[localSavedTheme]) {
        setAppTheme(localSavedTheme, false);
      } else if (cfg.theme && APP_THEMES[cfg.theme]) {
        setAppTheme(cfg.theme, false);
      }
      const localSavedMode = localStorage.getItem('castpad_mode');
      if (localSavedMode === 'light' || localSavedMode === 'dark') {
        setThemeMode(localSavedMode, false);
      } else if (cfg.themeMode) {
        setThemeMode(cfg.themeMode, false);
      }
      cacheButtonsLocally();
    }

    function saveSettings(silent = false) {
      if (!silent) haptic();
      cacheButtonsLocally();
      const qlTotal = qlPages * 12;
      const qlList = [];
      for (let i = 0; i < qlTotal; i++) {
        const item = quickLinksData[i] || { label: '', num: '', color: 'theme', icon: '' };
        qlList.push({
          label: (item.label || '').trim(),
          num: (item.num || '').trim(),
          color: (item.color || 'theme').trim(),
          icon: (item.icon || '').trim()
        });
      }

      const rawHost = document.getElementById('cfg-host').value.trim();
      let validHost = rawHost.replace(/[^a-zA-Z0-9-]/g, '').replace(/^-+|-+$/g, '').slice(0, 32);
      if (!validHost) validHost = 'CastPad';
      document.getElementById('cfg-host').value = validHost;

      const payload = {
        type: 'save_settings',
        sens: sensitivity,
        qlPages: qlPages,
        theme: currentAppTheme,
        themeMode: currentThemeMode,
        powerSave: document.getElementById('cfg-power-save') ? document.getElementById('cfg-power-save').checked : (localStorage.getItem('castpad_powersave') === '1'),
        ledDisabled: document.getElementById('cfg-disable-led') ? document.getElementById('cfg-disable-led').checked : (localStorage.getItem('castpad_disable_led') === '1'),
        btnCustom1: buttonActions.custom1 || 'NONE',
        btnCustom2: buttonActions.custom2 || 'NONE',
        btnCustom3: buttonActions.custom3 || 'NONE',
        btnCustom4: buttonActions.custom4 || 'NONE',
        btnRed: buttonActions.red,
        btnGreen: buttonActions.green,
        btnYellow: buttonActions.yellow,
        btnBlue: buttonActions.blue,
        lblCustom1: buttonLabels.custom1 || '',
        lblCustom2: buttonLabels.custom2 || '',
        lblCustom3: buttonLabels.custom3 || '',
        lblCustom4: buttonLabels.custom4 || '',
        lblRed: buttonLabels.red,
        lblGreen: buttonLabels.green,
        lblYellow: buttonLabels.yellow,
        lblBlue: buttonLabels.blue,
        iconCustom1: buttonIcons.custom1 || '',
        iconCustom2: buttonIcons.custom2 || '',
        iconCustom3: buttonIcons.custom3 || '',
        iconCustom4: buttonIcons.custom4 || '',
        iconRed: buttonIcons.red || '',
        iconGreen: buttonIcons.green || '',
        iconYellow: buttonIcons.yellow || '',
        iconBlue: buttonIcons.blue || '',
        colCustom1: buttonColors.custom1 || 'theme',
        colCustom2: buttonColors.custom2 || 'theme',
        colCustom3: buttonColors.custom3 || 'theme',
        colCustom4: buttonColors.custom4 || 'theme',
        colRed: buttonColors.red || 'theme',
        colGreen: buttonColors.green || 'theme',
        colYellow: buttonColors.yellow || 'theme',
        colBlue: buttonColors.blue || 'theme',
        quickLinks: qlList,
        host: validHost
      };

      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(payload));
        applySettings(payload);
        if (!silent) {
          showToast('Settings saved!');
        }
      }
    }

    // Desktop Batch Quick Edit Helpers
    function openBatchQuickEditModal() {
      haptic();
      const tbody = document.getElementById('batchEditTableBody');
      if (!tbody) return;
      tbody.innerHTML = '';
      const totalSlots = qlPages * 12;
      const colors = ['theme', 'sky', 'green', 'amber', 'red', 'indigo', 'purple', 'pink', 'slate'];

      for (let i = 0; i < totalSlots; i++) {
        const item = quickLinksData[i] || { label: '', num: '', color: 'theme' };
        const pageNum = Math.floor(i / 12) + 1;

        const tr = document.createElement('tr');
        tr.id = 'batch-row-' + i;

        // Slot ID cell
        const tdSlot = document.createElement('td');
        tdSlot.style.fontWeight = '700';
        tdSlot.style.color = 'var(--primary)';
        tdSlot.innerHTML = `<span style="font-size:11px; opacity:0.75; display:block;">P${pageNum}</span>#${i + 1}`;
        tr.appendChild(tdSlot);

        // Icon input cell
        const tdIcon = document.createElement('td');
        const iconInp = document.createElement('input');
        iconInp.type = 'text';
        iconInp.className = 'batch-input';
        iconInp.id = 'batch-icon-' + i;
        iconInp.maxLength = 4;
        iconInp.style.textAlign = 'center';
        iconInp.style.fontSize = '16px';
        iconInp.placeholder = '📺';
        iconInp.value = item.icon || '';
        tdIcon.appendChild(iconInp);
        tr.appendChild(tdIcon);

        // Label input cell
        const tdLabel = document.createElement('td');
        const lblInp = document.createElement('input');
        lblInp.type = 'text';
        lblInp.className = 'batch-input';
        lblInp.id = 'batch-lbl-' + i;
        lblInp.maxLength = 16;
        lblInp.placeholder = 'e.g. ESPN, Netflix';
        lblInp.value = item.label || '';
        tdLabel.appendChild(lblInp);
        tr.appendChild(tdLabel);

        // Digits input cell
        const tdNum = document.createElement('td');
        const numInp = document.createElement('input');
        numInp.type = 'text';
        numInp.className = 'batch-input';
        numInp.id = 'batch-num-' + i;
        numInp.placeholder = 'e.g. 206 or SEQ:...';
        numInp.value = item.num || '';
        tdNum.appendChild(numInp);
        tr.appendChild(tdNum);

        // Color select cell
        const tdColor = document.createElement('td');
        const colSel = document.createElement('select');
        colSel.className = 'batch-color-select';
        colSel.id = 'batch-col-' + i;
        colors.forEach(c => {
          const opt = document.createElement('option');
          opt.value = c;
          opt.textContent = (c === 'theme') ? 'Theme Accent (Default)' : (c.charAt(0).toUpperCase() + c.slice(1));
          if ((item.color || 'theme') === c) opt.selected = true;
          colSel.appendChild(opt);
        });
        tdColor.appendChild(colSel);
        tr.appendChild(tdColor);

        // Clear action cell
        const tdAction = document.createElement('td');
        tdAction.style.textAlign = 'center';
        const clearBtn = document.createElement('button');
        clearBtn.type = 'button';
        clearBtn.className = 'batch-clear-btn';
        clearBtn.title = 'Clear this slot';
        clearBtn.textContent = 'Clear';
        clearBtn.onclick = () => clearBatchRow(i);
        tdAction.appendChild(clearBtn);
        tr.appendChild(tdAction);

        tbody.appendChild(tr);
      }

      document.getElementById('batchEditModal').classList.add('open');
    }

    function clearBatchRow(i) {
      const icon = document.getElementById('batch-icon-' + i);
      const lbl = document.getElementById('batch-lbl-' + i);
      const num = document.getElementById('batch-num-' + i);
      if (icon) icon.value = '';
      if (lbl) lbl.value = '';
      if (num) num.value = '';
    }

    function closeBatchQuickEditModal(e) {
      if (e && e.target !== document.getElementById('batchEditModal')) return;
      document.getElementById('batchEditModal').classList.remove('open');
    }

    function saveBatchQuickEdit() {
      haptic();
      const totalSlots = qlPages * 12;
      for (let i = 0; i < totalSlots; i++) {
        const iconInput = document.getElementById('batch-icon-' + i);
        const lblInput = document.getElementById('batch-lbl-' + i);
        const numInput = document.getElementById('batch-num-' + i);
        const colSelect = document.getElementById('batch-col-' + i);
        if (lblInput && numInput && colSelect) {
          quickLinksData[i] = {
            label: lblInput.value.trim(),
            num: numInput.value.trim(),
            color: colSelect.value || 'theme',
            icon: iconInput ? iconInput.value.trim() : ''
          };
        }
      }
      renderQuickLinksGrid();
      cacheButtonsLocally();
      saveSettings(false);
      document.getElementById('batchEditModal').classList.remove('open');
      showToast('All quick links updated!');
    }

    // Button Backup & Restore (JSON Export / Import)
    function exportButtonConfig() {
      haptic();
      const exportData = {
        app: 'CastPad',
        format: 'castpad-buttons',
        version: 1,
        exportedAt: new Date().toISOString(),
        qlPages: qlPages,
        buttons: {
          red: { action: buttonActions.red || 'NONE', label: buttonLabels.red || '', icon: buttonIcons.red || '', color: buttonColors.red || 'red' },
          green: { action: buttonActions.green || 'NONE', label: buttonLabels.green || '', icon: buttonIcons.green || '', color: buttonColors.green || 'green' },
          yellow: { action: buttonActions.yellow || 'NONE', label: buttonLabels.yellow || '', icon: buttonIcons.yellow || '', color: buttonColors.yellow || 'yellow' },
          blue: { action: buttonActions.blue || 'NONE', label: buttonLabels.blue || '', icon: buttonIcons.blue || '', color: buttonColors.blue || 'blue' },
          custom1: { action: buttonActions.custom1 || 'NONE', label: buttonLabels.custom1 || '', icon: buttonIcons.custom1 || '', color: buttonColors.custom1 || 'theme' },
          custom2: { action: buttonActions.custom2 || 'NONE', label: buttonLabels.custom2 || '', icon: buttonIcons.custom2 || '', color: buttonColors.custom2 || 'theme' },
          custom3: { action: buttonActions.custom3 || 'NONE', label: buttonLabels.custom3 || '', icon: buttonIcons.custom3 || '', color: buttonColors.custom3 || 'theme' },
          custom4: { action: buttonActions.custom4 || 'NONE', label: buttonLabels.custom4 || '', icon: buttonIcons.custom4 || '', color: buttonColors.custom4 || 'theme' }
        },
        quickLinks: []
      };

      const total = qlPages * 12;
      for (let i = 0; i < total; i++) {
        const item = quickLinksData[i] || { label: '', num: '', color: 'theme', icon: '' };
        exportData.quickLinks.push({
          label: (item.label || '').trim(),
          num: (item.num || '').trim(),
          color: (item.color || 'theme').trim(),
          icon: (item.icon || '').trim()
        });
      }

      const jsonStr = JSON.stringify(exportData, null, 2);
      const blob = new Blob([jsonStr], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      const dStr = new Date().toISOString().slice(0, 10);
      a.href = url;
      a.download = `castpad-buttons-${dStr}.json`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      showToast('Button config exported!');
    }

    function importButtonConfig(input) {
      if (!input || !input.files || !input.files[0]) return;
      const file = input.files[0];
      const reader = new FileReader();
      reader.onload = (e) => {
        try {
          const data = JSON.parse(e.target.result);
          if (!data || (!data.buttons && !data.quickLinks)) {
            throw new Error('Invalid button config format');
          }

          if (data.buttons && typeof data.buttons === 'object') {
            const btnKeys = ['red', 'green', 'yellow', 'blue', 'custom1', 'custom2', 'custom3', 'custom4'];
            btnKeys.forEach(k => {
              if (data.buttons[k]) {
                buttonActions[k] = data.buttons[k].action || 'NONE';
                buttonLabels[k] = data.buttons[k].label || '';
                buttonIcons[k] = data.buttons[k].icon || '';
                buttonColors[k] = data.buttons[k].color || (k.startsWith('custom') ? 'theme' : k);
                updateButtonDisplay(k);
              }
            });
          }

          if (data.qlPages && typeof data.qlPages === 'number') {
            qlPages = Math.min(10, Math.max(1, data.qlPages));
            const qlCountEl = document.getElementById('cfg-ql-pages');
            if (qlCountEl) qlCountEl.value = qlPages;
          }

          if (Array.isArray(data.quickLinks)) {
            quickLinksData = data.quickLinks.map(item => ({
              label: (item && item.label) ? String(item.label) : '',
              num: (item && item.num) ? String(item.num) : '',
              color: (item && item.color) ? String(item.color) : 'theme',
              icon: (item && item.icon) ? String(item.icon) : ''
            }));
            while (quickLinksData.length < qlPages * 12) {
              quickLinksData.push({ label: '', num: '', color: 'theme', icon: '' });
            }
          }

          currentQlPage = 1;
          renderQuickLinksGrid();
          renderQuickLinksPagination();
          cacheButtonsLocally();
          saveSettings(false);
          showToast('Button config restored!');
        } catch (err) {
          alert('Failed to import file: ' + (err.message || 'Invalid JSON file'));
        } finally {
          input.value = '';
        }
      };
      reader.readAsText(file);
    }

    // Physical Keyboard Passthrough
    let keyboardPassthroughEnabled = false;

    function toggleKeyboardPassthrough(enable) {
      keyboardPassthroughEnabled = !!enable;
      try {
        localStorage.setItem('castpad_kb_passthrough', keyboardPassthroughEnabled ? '1' : '0');
      } catch (e) {}

      const dToggle = document.getElementById('desktopKbToggle');
      if (dToggle) {
        if (dToggle.checked !== keyboardPassthroughEnabled) dToggle.checked = keyboardPassthroughEnabled;
        dToggle.blur();
      }
      const dCard = document.getElementById('desktopKbCard');
      if (dCard) {
        if (keyboardPassthroughEnabled) dCard.classList.add('active');
        else dCard.classList.remove('active');
      }
      showToast(keyboardPassthroughEnabled ? 'PC Keyboard Passthrough ON' : 'PC Keyboard Passthrough OFF');
    }

    function initKeyboardPassthrough() {
      let saved = false;
      try {
        saved = localStorage.getItem('castpad_kb_passthrough') === '1';
      } catch (e) {}
      keyboardPassthroughEnabled = saved;
      const dToggle = document.getElementById('desktopKbToggle');
      if (dToggle) dToggle.checked = keyboardPassthroughEnabled;
      const dCard = document.getElementById('desktopKbCard');
      if (dCard && keyboardPassthroughEnabled) dCard.classList.add('active');

      window.addEventListener('keydown', handleGlobalKeydown);
    }

    function handleGlobalKeydown(e) {
      if (!keyboardPassthroughEnabled) return;

      const active = document.activeElement;
      if (active) {
        const tag = (active.tagName || '').toUpperCase();
        // Ignore passthrough only when focused inside actual text-entry fields
        if (tag === 'TEXTAREA' || tag === 'SELECT' || active.isContentEditable) {
          return;
        }
        if (tag === 'INPUT') {
          const type = (active.type || '').toLowerCase();
          if (type !== 'checkbox' && type !== 'radio' && type !== 'range') {
            return;
          }
        }
      }

      if (document.querySelector('.modal-overlay.open')) {
        return;
      }

      if (e.ctrlKey || e.altKey || e.metaKey) {
        return;
      }

      let cmd = null;
      let rawChar = null;
      const key = e.key;
      const code = e.code;

      if (code === 'Space' || key === ' ' || key === 'Spacebar') {
        cmd = 'SPACE';
      } else {
        switch (key) {
          case 'ArrowUp':
            cmd = 'UP';
            break;
          case 'ArrowDown':
            cmd = 'DOWN';
            break;
          case 'ArrowLeft':
            cmd = 'LEFT';
            break;
          case 'ArrowRight':
            cmd = 'RIGHT';
            break;
          case 'Enter':
            cmd = 'ENTER';
            break;
          case 'Escape':
            cmd = 'BACK';
            break;
          case 'Backspace':
            cmd = 'BACKSPACE';
            break;
          case 'Tab':
            cmd = 'TAB';
            break;
          case 'Home':
            cmd = 'HOME';
            break;
          case 'PageUp':
            cmd = 'PAGE_UP';
            break;
          case 'PageDown':
            cmd = 'PAGE_DOWN';
            break;
          case 'AudioVolumeUp':
            cmd = 'VOL_UP';
            break;
          case 'AudioVolumeDown':
            cmd = 'VOL_DOWN';
            break;
          case 'AudioVolumeMute':
            cmd = 'MUTE';
            break;
          default:
            if (/^F([1-9]|1[0-2])$/.test(key)) {
              cmd = key;
            } else if (key && key.length === 1 && key.charCodeAt(0) >= 33 && key.charCodeAt(0) <= 126) {
              rawChar = key;
            }
            break;
        }
      }

      if (cmd || rawChar) {
        e.preventDefault();
        if (active && active.tagName === 'BUTTON') {
          active.blur();
        }
        if (rawChar) {
          sendTextRaw(rawChar);
        } else {
          sendAction(cmd);
        }
      }
    }

    // Modal Helpers: About, Help, Confirm
    function openAboutModal() {
      haptic();
      document.getElementById('aboutModal').classList.add('open');
    }
    function closeAboutModal(e) {
      if (e && e.target !== document.getElementById('aboutModal')) return;
      document.getElementById('aboutModal').classList.remove('open');
    }

    function openHelpModal() {
      haptic();
      document.getElementById('helpModal').classList.add('open');
    }
    function closeHelpModal(e) {
      if (e && e.target !== document.getElementById('helpModal')) return;
      document.getElementById('helpModal').classList.remove('open');
    }

    function openAddToHomeModal() {
      haptic();
      document.getElementById('pwaModal').classList.add('open');
    }
    function closeAddToHomeModal(e) {
      if (e && e.target !== document.getElementById('pwaModal')) return;
      document.getElementById('pwaModal').classList.remove('open');
    }

    let deferredInstallPrompt = null;

    function isRunningStandalone() {
      return (window.matchMedia && window.matchMedia('(display-mode: standalone)').matches) ||
             (window.navigator.standalone === true);
    }

    function isMobileDevice() {
      const ua = navigator.userAgent || navigator.vendor || window.opera;
      const isTouch = ('ontouchstart' in window) || (navigator.maxTouchPoints > 0);
      const isMobileUA = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(ua);
      return isMobileUA || (isTouch && window.innerWidth <= 840);
    }


    function checkAddToHomeScreenVisibility() {
      const btn = document.getElementById('btnAddToHome');
      if (!btn) return;
      if (isMobileDevice() && !isRunningStandalone()) {
        btn.style.display = 'flex';
      } else {
        btn.style.display = 'none';
      }
    }

    window.addEventListener('beforeinstallprompt', (e) => {
      e.preventDefault();
      deferredInstallPrompt = e;
      checkAddToHomeScreenVisibility();
    });

    window.addEventListener('appinstalled', () => {
      deferredInstallPrompt = null;
      checkAddToHomeScreenVisibility();
    });

    function onAddToHomeScreen() {
      haptic();
      if (deferredInstallPrompt) {
        deferredInstallPrompt.prompt();
        deferredInstallPrompt.userChoice.then((choiceResult) => {
          if (choiceResult.outcome === 'accepted') {
            showToast('CastPad added to Home Screen!');
            const btn = document.getElementById('btnAddToHome');
            if (btn) btn.style.display = 'none';
          }
          deferredInstallPrompt = null;
        });
      } else {
        openAddToHomeModal();
      }
    }

    let confirmCallback = null;
    function openConfirmModal(title, message, okText, isDanger, onOk) {
      haptic();
      document.getElementById('confirmTitle').textContent = title;
      document.getElementById('confirmMessage').innerText = message;
      const okBtn = document.getElementById('confirmOkBtn');
      okBtn.textContent = okText || 'Confirm';
      okBtn.className = isDanger ? 'danger-btn' : 'primary-btn';
      confirmCallback = onOk;
      okBtn.onclick = () => {
        const cb = confirmCallback;
        closeConfirmModal();
        if (cb) cb();
      };
      document.getElementById('confirmModal').classList.add('open');
    }
    function closeConfirmModal(e) {
      if (e && e.target !== document.getElementById('confirmModal')) return;
      document.getElementById('confirmModal').classList.remove('open');
      confirmCallback = null;
    }

    function onSensSliderChange(val) {
      sensitivity = parseFloat(val) || 1.3;
      const lbl = document.getElementById('sensValue');
      if (lbl) lbl.textContent = sensitivity.toFixed(1) + 'x';
      saveSettings(true);
    }

    function confirmRestartDevice() {
      openConfirmModal(
        'Restart CastPad?',
        'CastPad will reboot. All saved settings, custom buttons, and quick links will be preserved.',
        'Restart',
        false,
        () => {
          if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'reboot_device' }));
            showToast('Rebooting device...');
          }
        }
      );
    }

    function confirmResetDevice() {
      openConfirmModal(
        'Factory Reset CastPad?',
        'Warning: This will permanently erase ALL saved Wi-Fi networks, custom buttons, quick links, and settings. The board will reboot into initial setup mode.\n\nAre you sure you want to proceed?',
        'Erase & Reset',
        true,
        () => {
          if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'factory_reset' }));
            showToast('Factory resetting...');
          }
        }
      );
    }

    function confirmForgetWifi() {
      openConfirmModal(
        'Forget Wi-Fi Settings?',
        'CastPad will forget the saved Wi-Fi credentials and reboot into setup mode.',
        'Forget Wi-Fi',
        true,
        () => {
          if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'forget_wifi' }));
            showToast('Rebooting into setup mode...');
          }
        }
      );
    }

    function onPowerSaveToggle(checkbox) {
      const willEnable = checkbox.checked;
      checkbox.checked = !willEnable;
      if (willEnable) {
        openConfirmModal(
          'Enable Power Saving Mode?',
          'Warning: Enabling Power Saving Mode (Wi-Fi Modem Sleep and 80 MHz CPU clock) lowers power consumption and heat, but you may see a degradation in responsiveness (such as minor latency on initial clicks or trackpad gestures).\n\nDo you want to enable Power Saving?',
          'Enable Power Save',
          false,
          () => {
            checkbox.checked = true;
            try { localStorage.setItem('castpad_powersave', '1'); } catch (e) {}
            if (ws && ws.readyState === WebSocket.OPEN) {
              ws.send(JSON.stringify({ type: 'set_power_save', enabled: true }));
              showToast('Power saving enabled');
            }
          }
        );
      } else {
        checkbox.checked = false;
        try { localStorage.setItem('castpad_powersave', '0'); } catch (e) {}
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ type: 'set_power_save', enabled: false }));
          showToast('Power saving disabled (Full Performance)');
        }
      }
    }

    function onDisableLedToggle(checkbox) {
      const willDisable = checkbox.checked;
      try { localStorage.setItem('castpad_disable_led', willDisable ? '1' : '0'); } catch (e) {}
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'set_led_disabled', disabled: willDisable }));
      }
      showToast(willDisable ? 'Status LED disabled' : 'Status LED enabled');
    }

    function onApModeToggle(checkbox) {
      const willEnable = checkbox.checked;
      checkbox.checked = !willEnable;
      const currentHost = document.getElementById('cfg-host').value.trim() || 'CastPad';
      if (willEnable) {
        openConfirmModal(
          'Enable Standalone AP Mode?',
          'CastPad will broadcast its own direct Wi-Fi network instead of connecting to a home router.\n\n• Wi-Fi Network (SSID): ' + currentHost + ' (Open)\n• URL / IP: http://192.168.4.1\n\nTo control your device, connect your phone directly to the "' + currentHost + '" Wi-Fi network.',
          'Enable AP Mode',
          false,
          () => {
            checkbox.checked = true;
            if (ws && ws.readyState === WebSocket.OPEN) {
              ws.send(JSON.stringify({ type: 'set_ap_mode', enabled: true }));
              showToast('Switching to AP Mode...');
            }
          }
        );
      } else {
        openConfirmModal(
          'Disable Standalone AP Mode?',
          'CastPad will turn off direct AP broadcasting and attempt to connect to your home Wi-Fi network.\n\nProceed?',
          'Disable AP Mode',
          false,
          () => {
            checkbox.checked = false;
            if (ws && ws.readyState === WebSocket.OPEN) {
              ws.send(JSON.stringify({ type: 'set_ap_mode', enabled: false }));
              showToast('Disabling AP Mode...');
            }
          }
        );
      }
    }

    function resetWifi() {
      confirmForgetWifi();
    }

    function disconnectBle() {
      haptic();
      flashBleTx();
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'ble_disconnect' }));
        showToast('Cycling BLE connection...');
      }
    }

    function forgetBleBonds() {
      haptic();
      openConfirmModal(
        'Forget BLE Bonds & Re-pair',
        'Forget all saved BLE pairings and restart BLE advertising?\n\nNote: If re-pairing with a previously paired Fire TV, smart TV, or PC, remember to also "Forget" CastPad on that device first.',
        'Forget Bonds',
        true,
        () => {
          flashBleTx();
          if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'ble_forget_bonds' }));
            showToast('BLE bonds erased. Advertising...');
          }
        }
      );
    }

    function onOtaFileSelect(input) {
      const info = document.getElementById('otaFileInfo');
      if (!info) return;
      if (input.files && input.files.length > 0) {
        const file = input.files[0];
        const sz = (file.size / 1024 / 1024).toFixed(2);
        info.textContent = `Selected: ${file.name} (${sz} MB)`;
        info.style.display = 'block';
      } else {
        info.style.display = 'none';
      }
    }

    // Web OTA Firmware Upload
    function startOtaUpload() {
      const fileInput = document.getElementById('otaFile');
      if (!fileInput.files.length) {
        showToast('Please select a .bin firmware file first');
        return;
      }

      const file = fileInput.files[0];
      const progressBar = document.getElementById('otaProgressBar');
      const progressFill = document.getElementById('otaProgressFill');
      const statusText = document.getElementById('otaStatusText');
      const uploadBtn = document.getElementById('otaUploadBtn');

      progressBar.style.display = 'block';
      uploadBtn.disabled = true;
      statusText.textContent = 'Uploading firmware...';

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/update');

      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          progressFill.style.width = percent + '%';
          statusText.textContent = `Uploading: ${percent}%`;
        }
      };

      xhr.onload = () => {
        if (xhr.status === 200 && xhr.responseText.includes('OK')) {
          statusText.textContent = 'Flash successful! Rebooting device in 10s...';
          progressFill.style.width = '100%';
          let count = 10;
          const interval = setInterval(() => {
            count--;
            if (count > 0) {
              statusText.textContent = `Rebooting... refreshing in ${count}s`;
            } else {
              clearInterval(interval);
              window.location.reload();
            }
          }, 1000);
        } else {
          statusText.textContent = 'Update failed: ' + xhr.responseText;
          uploadBtn.disabled = false;
        }
      };

      xhr.onerror = () => {
        statusText.textContent = 'Upload error. Please try again.';
        uploadBtn.disabled = false;
      };

      const formData = new FormData();
      formData.append('firmware', file);
      xhr.send(formData);
    }

    window.onload = () => {
      restoreCachedButtons();
      initButtonVisualFeedback();
      initLongPressCustomization();
      initQuickLinksGestures();
      initKeyboardModalEvents();
      initKeyboardPassthrough();
      renderQuickLinksGrid();
      renderQuickLinksPagination();
      initWebSocket();
      setInterval(updateUptimeTicker, 1000);
      checkAddToHomeScreenVisibility();
      window.addEventListener('resize', checkAddToHomeScreenVisibility);

      restoreActiveTab();
    };
  </script>
</body>
</html>
)rawliteral";
