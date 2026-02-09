#include "Chaudiere.h"
#include "../Logs.h"
#include <math.h>

void Chaudiere::begin(std::function<void(const String&)> modeEcsCommandCb) {
    info("[CHAUDIERE][MQTT] Initialisation des entités.");

    MqttDevice* device = _mqtt.getDevice("openFrisquetVisio");

    _mqttEntities.tempECS.id = "temperatureECS";
    _mqttEntities.tempECS.name = "Température ECS";
    _mqttEntities.tempECS.component = "sensor";
    _mqttEntities.tempECS.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "chaudiere", "temperatureECS"}), 0, true);
    _mqttEntities.tempECS.commandTopic = MqttTopic(MqttManager::compose({device->baseTopic, "chaudiere", "temperatureECS", "set"}), 0, true);
    _mqttEntities.tempECS.set("device_class", "temperature");
    _mqttEntities.tempECS.set("state_class", "measurement");
    _mqttEntities.tempECS.set("unit_of_measurement", "°C");
    _mqtt.registerEntity(*device, _mqttEntities.tempECS, true);

    _mqttEntities.tempCDC.id = "temperatureCDC";
    _mqttEntities.tempCDC.name = "Température CDC";
    _mqttEntities.tempCDC.component = "sensor";
    _mqttEntities.tempCDC.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "chaudiere", "temperatureCDC"}), 0, true);
    _mqttEntities.tempCDC.set("device_class", "temperature");
    _mqttEntities.tempCDC.set("state_class", "measurement");
    _mqttEntities.tempCDC.set("unit_of_measurement", "°C");
    _mqtt.registerEntity(*device, _mqttEntities.tempCDC, true);

    _mqttEntities.tempExterieure.id = "temperatureExterieure";
    _mqttEntities.tempExterieure.name = "Température extérieure";
    _mqttEntities.tempExterieure.component = "sensor";
    if (_cfg.useSondeExterieure() && !_cfg.useDS18B20()) {
        _mqttEntities.tempExterieure.component = "number";
    }
    _mqttEntities.tempExterieure.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "sondeExterieure", "temperatureExterieure"}), 0, true);
    _mqttEntities.tempExterieure.set("device_class", "temperature");
    _mqttEntities.tempExterieure.set("state_class", "measurement");
    _mqttEntities.tempExterieure.set("unit_of_measurement", "°C");
    _mqtt.registerEntity(*device, _mqttEntities.tempExterieure, true);

    _mqttEntities.consommationChauffage.id = "consommationChauffage";
    _mqttEntities.consommationChauffage.name = "Consommation chauffage";
    _mqttEntities.consommationChauffage.component = "sensor";
    _mqttEntities.consommationChauffage.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "chaudiere", "consommationChauffage"}), 0, true);
    _mqttEntities.consommationChauffage.set("device_class", "energy");
    _mqttEntities.consommationChauffage.set("state_class", "total_increasing");
    _mqttEntities.consommationChauffage.set("unit_of_measurement", "kWh");
    _mqtt.registerEntity(*device, _mqttEntities.consommationChauffage, true);

    _mqttEntities.consommationECS.id = "consommationECS";
    _mqttEntities.consommationECS.name = "Consommation ECS";
    _mqttEntities.consommationECS.component = "sensor";
    _mqttEntities.consommationECS.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "chaudiere", "consommationECS"}), 0, true);
    _mqttEntities.consommationECS.set("device_class", "energy");
    _mqttEntities.consommationECS.set("state_class", "total_increasing");
    _mqttEntities.consommationECS.set("unit_of_measurement", "kWh");
    _mqtt.registerEntity(*device, _mqttEntities.consommationECS, true);

    _mqttEntities.modeECS.id = "modeECS";
    _mqttEntities.modeECS.name = "Mode ECS";
    _mqttEntities.modeECS.component = "select";
    _mqttEntities.modeECS.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "chaudiere", "modeECS"}), 0, true);
    _mqttEntities.modeECS.commandTopic = MqttTopic(MqttManager::compose({device->baseTopic, "chaudiere", "modeECS", "set"}), 0, true);
    _mqttEntities.modeECS.set("icon", "mdi:tune-variant");
    _mqttEntities.modeECS.set("entity_category", "config");
    _mqttEntities.modeECS.setRaw("options", R"(["Max","Eco","Eco Horaires","Eco+", "Eco+ Horaires", "Stop"])");
    _mqtt.registerEntity(*device, _mqttEntities.modeECS, true);

    if (modeEcsCommandCb) {
        _mqtt.onCommand(_mqttEntities.modeECS, [modeEcsCommandCb](const String& payload) {
            modeEcsCommandCb(payload);
        });
    }

    _mqttEntities.pression.id = "pression";
    _mqttEntities.pression.name = "Pression";
    _mqttEntities.pression.component = "sensor";
    _mqttEntities.pression.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "chaudiere", "pression"}), 0, true);
    _mqttEntities.pression.set("device_class", "pressure");
    _mqttEntities.pression.set("unit_of_measurement", "bar");
    _mqtt.registerEntity(*device, _mqttEntities.pression, true);
}

void Chaudiere::publishMqtt() {
    if (!isnan(getTemperatureECS())) {
        _mqtt.publishState(_mqttEntities.tempECS, getTemperatureECS());
    }
    if (!isnan(getTemperatureCDC())) {
        _mqtt.publishState(_mqttEntities.tempCDC, getTemperatureCDC());
    }
    if (!isnan(getTemperatureExterieure())) {
        _mqtt.publishState(_mqttEntities.tempExterieure, getTemperatureExterieure());
    }

    if (getConsommationChauffage() >= 0) {
        if (_lastPubConsommationChauffage != getConsommationChauffage()) {
            _lastPubConsommationChauffage = getConsommationChauffage();
            _mqtt.publishState(_mqttEntities.consommationChauffage, getConsommationChauffage());
        }
    }
    if (getConsommationECS() >= 0) {
        if (_lastPubConsommationECS != getConsommationECS()) {
            _lastPubConsommationECS = getConsommationECS();
            _mqtt.publishState(_mqttEntities.consommationECS, getConsommationECS());
        }
    }

    publishModeEcs();

    if (!isnan(getPression())) {
        _mqtt.publishState(_mqttEntities.pression, getPression());
    }
}

void Chaudiere::publishModeEcs() {
    if (getModeECS() != MODE_ECS::INCONNU) {
        _mqtt.publishState(_mqttEntities.modeECS, getNomModeECS().c_str());
    }
}

void Chaudiere::setTemperatureExterieure(float temperature) {
    _temperatureExterieure = temperature;
}

void Chaudiere::setTemperatureECS(float temperature) {
    _temperatureECS = temperature;
}

void Chaudiere::setTemperatureCDC(float temperature) {
    _temperatureCDC = temperature;
}

void Chaudiere::setConsommationECS(int16_t consommation) {
    _consommationGazECS = consommation;
}

void Chaudiere::setConsommationChauffage(int16_t consommation) {
    _consommationGazChauffage = consommation;
}

String Chaudiere::getNomModeECS() const {
    switch (getModeECS()) {
        case MODE_ECS::STOP:
            return "Stop";
        case MODE_ECS::MAX:
            return "Max";
        case MODE_ECS::ECO:
            return "Eco";
        case MODE_ECS::ECO_HORAIRES:
            return "Eco Horaires";
        case MODE_ECS::ECOPLUS:
            return "Eco+";
        case MODE_ECS::ECOPLUS_HORAIRES:
            return "Eco+ Horaires";
        default:
            break;
    }

    return "Inconnu";
}

bool Chaudiere::setModeECS(MODE_ECS modeECS) {
    _modeECS = modeECS;
    return true;
}

bool Chaudiere::setModeECS(const String& modeECS) {
    if (modeECS.equalsIgnoreCase("Max")) {
        setModeECS(MODE_ECS::MAX);
    } else if (modeECS.equalsIgnoreCase("Eco")) {
        setModeECS(MODE_ECS::ECO);
    } else if (modeECS.equalsIgnoreCase("Eco+")) {
        setModeECS(MODE_ECS::ECOPLUS);
    } else if (modeECS.equalsIgnoreCase("Eco Horaires")) {
        setModeECS(MODE_ECS::ECO_HORAIRES);
    } else if (modeECS.equalsIgnoreCase("Eco+ Horaires")) {
        setModeECS(MODE_ECS::ECOPLUS_HORAIRES);
    } else if (modeECS.equalsIgnoreCase("Stop")) {
        setModeECS(MODE_ECS::STOP);
    } else {
        return false;
    }

    return true;
}

void Chaudiere::setPression(float pression) {
    _pression = pression;
}
