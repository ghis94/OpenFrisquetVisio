#include "App.h"

App::App() : _mqtt(_wifiClient), _frisquetManager(_radio, _cfg, _mqtt) {}

void App::begin() {
  Serial.begin(115200);
  delay(50);

  Heltec.begin(false /*DisplayEnable disable*/, false /*LoRa Disable*/, true /*Serial Enable*/);
  initButtons();

  initConfig();
  initNetwork();
  initMqtt();
  initPortal();
  initOta();

  _frisquetManager.begin();
}

void App::loop() {
  handleDisplayButtons();

  // boucle des services
  _networkManager.loop();
  if (_apFallbackArmed && !WiFi.isConnected()) {
    if ((int32_t)(millis() - _apFallbackAtMs) >= 0) {
      _apFallbackArmed = false;
      WiFi.mode(WIFI_AP_STA);
      info("[WIFI] AP fallback activé après délai.");
    }
  }
  _ota.loop();
  _portal->loop();
  _mqtt.loop();
  _frisquetManager.loop();
  updateDisplay();
  delay(10);
}

void App::initConfig() {
  _cfg.load();
}

void App::initNetwork() {
    _networkManager.onConnected([&](){
        info("[WIFI] CONNECTED  IP=%s  RSSI=%ddBm\n", _networkManager.ipStr().c_str(), _networkManager.rssi());
        _apFallbackArmed = false;
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
    });
    _networkManager.onDisconnected([&](const String& reason){
        info("[WIFI] DISCONNECTED (%s)\n", reason.c_str());
        _apFallbackArmed = true;
        _apFallbackAtMs = millis() + 60000;
    });

    _networkManager.begin(_cfg.getWiFiOptions());
}

void App::initMqtt() {
  _mqtt.begin(_cfg.getMQTTOptions());
}

void App::initPortal() {
  _portal = new Portal(_frisquetManager);
  _portal->begin(/*startApFallbackIfNoWifi=*/true);

  info("[PORTAIL] Portail initialisé.");
}

void App::initOta() {
  _ota.begin(_networkManager.hostname().c_str());
}

void App::initButtons() {
  for (uint8_t i = 0; i < DISPLAY_BUTTON_COUNT; ++i) {
    pinMode(_displayButtonPins[i], INPUT_PULLUP);
    _displayButtonState[i] = digitalRead(_displayButtonPins[i]);
  }
}

void App::handleDisplayButtons() {
  bool displayRequested = false;

  for (uint8_t i = 0; i < DISPLAY_BUTTON_COUNT; ++i) {
    const bool currentState = digitalRead(_displayButtonPins[i]);
    const bool previousState = _displayButtonState[i];

    // Front descendant: bouton pressé (INPUT_PULLUP => LOW)
    if (previousState == HIGH && currentState == LOW) {
      displayRequested = true;
    }

    _displayButtonState[i] = currentState;
  }

  if (displayRequested) {
    showDisplay();
  }

  if (_displayEnabled && _displayDeadlineMs != 0 && static_cast<int32_t>(millis() - _displayDeadlineMs) >= 0) {
    hideDisplay();
  }
}

void App::showDisplay() {
  if (!Heltec.display) return;
  Heltec.display->displayOn();
  _displayEnabled = true;
  _displayDeadlineMs = millis() + DISPLAY_TIMEOUT_MS;
  _lastDisplayMs = 0;
  _lastDisplayPageMs = millis();
  updateDisplay(true);
}

void App::hideDisplay() {
  if (!Heltec.display) return;
  Heltec.display->clear();
  Heltec.display->display();
  Heltec.display->displayOff();
  _displayEnabled = false;
  _displayDeadlineMs = 0;
}

void App::updateDisplay(bool force) {
  if (!_displayEnabled || !Heltec.display) return;

  const uint32_t now = millis();
  if (!force && (now - _lastDisplayMs) < DISPLAY_REFRESH_MS) return;
  _lastDisplayMs = now;

  if ((now - _lastDisplayPageMs) >= DISPLAY_PAGE_ROTATE_MS) {
    _displayPage = (_displayPage + 1) % 3;
    _lastDisplayPageMs = now;
  }

  auto fmtTemp = [](float v) -> String {
    if (isnan(v)) return String("--.-");
    return String(v, 1);
  };

  const bool wifiOk = _networkManager.isConnected();
  const bool mqttOk = _mqtt.connected();
  const auto& ch = _frisquetManager.chaudiere();
  const auto& z1 = _frisquetManager.connect().getZone1();

  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_10);

  if (_displayPage == 0) {
    Heltec.display->drawString(0, 0, String("WiFi: ") + (wifiOk ? "OK" : "OFF"));
    Heltec.display->drawString(64, 0, String("MQTT: ") + (mqttOk ? "OK" : "OFF"));
    Heltec.display->drawStringMaxWidth(0, 14, 128, String("IP: ") + (wifiOk ? _networkManager.ipStr() : "-"));
    Heltec.display->drawString(0, 28, String("RSSI: ") + (wifiOk ? String(_networkManager.rssi()) + "dBm" : "-"));
    Heltec.display->drawStringMaxWidth(0, 42, 128, String("SSID: ") + (wifiOk ? _networkManager.ssid() : "-"));
  } else if (_displayPage == 1) {
    Heltec.display->drawString(0, 0, "Chaudiere");
    Heltec.display->drawString(0, 14, String("CDC: ") + fmtTemp(ch.getTemperatureCDC()) + "C");
    Heltec.display->drawString(64, 14, String("ECS: ") + fmtTemp(ch.getTemperatureECS()) + "C");
    Heltec.display->drawString(0, 28, String("Ext: ") + fmtTemp(ch.getTemperatureExterieure()) + "C");
    Heltec.display->drawString(64, 28, String("P: ") + fmtTemp(ch.getPression()) + "b");
    Heltec.display->drawStringMaxWidth(0, 42, 128, String("Mode ECS: ") + ch.getNomModeECS());
  } else {
    Heltec.display->drawString(0, 0, "Zone 1 / Consignes");
    Heltec.display->drawString(0, 14, String("Amb: ") + fmtTemp(z1.getTemperatureAmbiante()) + "C");
    Heltec.display->drawString(64, 14, String("Cons: ") + fmtTemp(z1.getTemperatureConsigne()) + "C");
    Heltec.display->drawString(0, 28, String("Depart: ") + fmtTemp(z1.getTemperatureDepart()) + "C");
    Heltec.display->drawString(64, 28, String("Boost: ") + (z1.boostActif() ? "ON" : "OFF"));
    Heltec.display->drawStringMaxWidth(0, 42, 128, String("Sonde->Chaud: ") + fmtTemp(_frisquetManager.sondeExterieure().getTemperatureExterieure()) + "C");
  }

  Heltec.display->display();
}
