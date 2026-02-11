#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include "MQTT/MqttManager.h"
#include "MQTT/MqttDevice.h"
#include "Config.h"
#include "NetworkManager.h"
#include "Portal.h"
#include "Logs.h"
#include "OTA.h"

#include "Frisquet/FrisquetRadio.h"
#include "FrisquetManager.h"

class App {
public:
  App();

  void begin();
  void loop();

private:
  // Dépendances et services
  Config _cfg;
  WiFiClient _wifiClient;
  MqttManager _mqtt;
  NetworkManager _networkManager;
  OTA _ota;

  FrisquetManager _frisquetManager;

  // Portail
  Portal* _portal = nullptr;

  // Radio
  FrisquetRadio _radio;

  // AP fallback after disconnect
  bool _apFallbackArmed = false;
  uint32_t _apFallbackAtMs = 0;

  // Étapes
  void initConfig();
  void initNetwork();
  void initMqtt();
  void initPortal();
  void initOta();

  // Affichage OLED à la demande (bouton device)
  void initButtons();
  void handleDisplayButtons();
  void showDisplay();
  void hideDisplay();

  static constexpr uint8_t DISPLAY_BUTTON_COUNT = 2;
  static constexpr uint32_t DISPLAY_TIMEOUT_MS = 10000;
  uint8_t _displayButtonPins[DISPLAY_BUTTON_COUNT] = {0, 21};
  bool _displayButtonState[DISPLAY_BUTTON_COUNT] = {true, true};
  bool _displayEnabled = false;
  uint32_t _displayDeadlineMs = 0;
};
