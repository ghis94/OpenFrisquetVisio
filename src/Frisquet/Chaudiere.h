#pragma once

#include <Arduino.h>
#include <functional>
#include "../MQTT/MqttManager.h"
#include "../Config.h"

class Chaudiere {
public:
    enum MODE_ECS : uint8_t {
        INCONNU = 0XFF,
        STOP = 0x29,
        MAX = 0x01,
        ECO = 0x09,
        ECO_HORAIRES = 0x11,
        ECOPLUS = 0x19,
        ECOPLUS_HORAIRES = 0x21
    };

    Chaudiere(MqttManager& mqtt, Config& cfg) : _mqtt(mqtt), _cfg(cfg) {}

    void begin(std::function<void(const String&)> modeEcsCommandCb = {});
    void publishMqtt();
    void publishModeEcs();

    void setTemperatureExterieure(float temperature);
    void setTemperatureECS(float temperature);
    void setTemperatureCDC(float temperature);

    float getTemperatureExterieure() const { return _temperatureExterieure; }
    float getTemperatureECS() const { return _temperatureECS; }
    float getTemperatureCDC() const { return _temperatureCDC; }

    void setConsommationECS(int16_t consommation);
    void setConsommationChauffage(int16_t consommation);
    int16_t getConsommationChauffage() const { return _consommationGazChauffage; }
    int16_t getConsommationECS() const { return _consommationGazECS; }

    bool setModeECS(MODE_ECS modeECS);
    bool setModeECS(const String& modeECS);
    MODE_ECS getModeECS() const { return _modeECS; }
    String getNomModeECS() const;

    void setPression(float pression);
    float getPression() const { return _pression; }

private:
    MqttManager& _mqtt;
    Config& _cfg;

    float _temperatureECS = NAN;
    float _temperatureCDC = NAN;
    float _temperatureExterieure = NAN;
    float _pression = NAN;

    int16_t _consommationGazECS = -1;
    int16_t _consommationGazChauffage = -1;

    MODE_ECS _modeECS = MODE_ECS::INCONNU;

    int16_t _lastPubConsommationECS = -1;
    int16_t _lastPubConsommationChauffage = -1;

    struct {
        MqttEntity modeECS;
        MqttEntity tempECS;
        MqttEntity tempCDC;
        MqttEntity tempExterieure;
        MqttEntity consommationChauffage;
        MqttEntity consommationECS;
        MqttEntity pression;
    } _mqttEntities;
};
