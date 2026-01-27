#include "Connect.h"
#include "../Buffer.h"
#include <cstring>
#include <math.h>

void Connect::loadConfig() {
    bool checkMigration = false;

    getPreferences().begin("connectCfg", false);

    if(! getPreferences().isKey("idAssociation")) {
        checkMigration = true;
    }

    setIdAssociation(getPreferences().getUChar("idAssociation", 0xFF));

    getPreferences().end();
    delay(100);
    
    // Migration
    if(checkMigration && getPreferences().begin("net-conf", false)) {
        if(getPreferences().isKey("con_id")) {
            setIdAssociation(getPreferences().getUChar("con_id", 0xFF));
        }
        getPreferences().end();
        checkMigration = false;
    }
}

void Connect::saveConfig() {   
    getPreferences().begin("connectCfg", false);
    getPreferences().putUChar("idAssociation", getIdAssociation());
    getPreferences().end();
}

bool Connect::envoyerZone(Zone& zone) {
    if(! estAssocie()) {
        return false;
    }

    if(isnan(zone.getTemperatureConfort()) || isnan(zone.getTemperatureReduit()) || isnan(zone.getTemperatureHorsGel()) || zone.getMode() == Zone::MODE_ZONE::INCONNU || zone.getNumeroZone() == 0) {
        error("[CONNECT] Impossible d'envoyer la zone %d, configuration incomplète.", zone.getNumeroZone());
        return false;
    }

    info("[CONNECT] Envoi de la zone %d.", zone.getNumeroZone());

    if(zone.getIdZone() == ID_ZONE_1 && getConfig().useSatelliteVirtualZ1()) {
        return true;
    } else if(zone.getIdZone() == ID_ZONE_2 && getConfig().useSatelliteVirtualZ2()) {
        return true;
    } else if(zone.getIdZone() == ID_ZONE_3 && getConfig().useSatelliteVirtualZ3()) {
        return true;
    }
    
    struct {
        temperature8 temperatureConfort;    // Début 5°C -> 0 = 50 = 5°C - MAX 30°C
        temperature8 temperatureReduit;     // Début 5°C -> 0 = 50 = 5°C - MAX Confort
        temperature8 temperatureHorsGel;     // Début 5°C -> 0 = 50 = 5°C - MAX Hors gel
        uint8_t mode = 0x00; 
        byte modeOptions = 0b00000100;
        /*
        Mode Option structure bits
            inconnu1: 1 bit,
            boost: 1 bit,
            inconnu2: 2 bits,
            inconnu3: 2 bits,
            derogation: 1 bit,
            confort: 1 bit
        */
        byte inconnu1 = 0x00;
    } payload;
    
    if(zone.boostActif()) {
        payload.temperatureConfort = zone.getTemperatureConfort() + zone.getTemperatureBoost();
    } else {
        payload.temperatureConfort = zone.getTemperatureConfort();
    }
    payload.temperatureReduit = zone.getTemperatureReduit();
    payload.temperatureHorsGel = zone.getTemperatureHorsGel();
    payload.mode = zone.getMode();
    payload.modeOptions = zone.getModeOptions();
    
    byte buff[RADIOLIB_SX126X_MAX_PACKET_LENGTH];
    size_t length = 0;
    int16_t err;

    uint8_t retry = 0;
    do {
        err = this->radio().sendInit(
            this->getId(), 
            ID_CHAUDIERE, 
            this->getIdAssociation(),
            this->incrementIdMessage(),
            zone.getIdZone(), 
            0xA154,
            0x0018,
            0xa154,
            0x0003,
            (byte*)&payload,
            sizeof(payload),
            buff,
            length
        );

        if(err != RADIOLIB_ERR_NONE) {
            delay(100);
            continue;
        }
        
        return true;
    } while(retry++ < 1);

    return false;
}

bool Connect::recupererInformations() {
    if(! estAssocie()) {
        return false;
    }

    struct {
        FrisquetRadio::RadioTrameHeader header;
        uint8_t longueurDonnees;
        temperature16 temperatureECS;
        temperature16 temperatureCDC;
        temperature16 temperatureDepartZ1;
        temperature16 temperatureDepartZ2;
        temperature16 temperatureDepartZ3;
        temperature16 temperatureInconnue1;
        temperature16 temperatureInconnue2;
        temperature16 temperatureInconnue3;
        temperature16 temperatureInconnue4;
        temperature16 temperatureInconnue5;
        pression16 pression;
        byte i1[1] = {0};
        byte modeECS;
        temperature16 temperatureECSInstant;
        byte i2[10] = {0};
        temperature16 temperatureAmbianteZ1;
        temperature16 temperatureAmbianteZ2;
        temperature16 temperatureAmbianteZ3;
        byte i3[6] = {0};
        temperature16 temperatureConsigneZ1;
        temperature16 temperatureConsigneZ2;
        temperature16 temperatureConsigneZ3;
        temperature16 temperatureExterieure;
    } buff;

    size_t length;
    int16_t err;

    uint8_t retry = 0;
    do {
        length = sizeof(buff);
        err = this->radio().sendAsk(
            this->getId(), 
            ID_CHAUDIERE, 
            this->getIdAssociation(),
            this->incrementIdMessage(),
            0x01,
            0x79E0 + (ID_CHAUDIERE == 0x84 ? 0xC8 : 0x00),
            0x001C,
            (byte*)&buff,
            length
        );

        if(err != RADIOLIB_ERR_NONE) {
            delay(100);
            continue;
        }

        if(getZone1().getSource() == Zone::SOURCE::CONNECT) {
            getZone1().setTemperatureAmbiante(buff.temperatureAmbianteZ1.toFloat());
            getZone1().setTemperatureConsigne(buff.temperatureConsigneZ1.toFloat());
        }
        getZone1().setTemperatureDepart(buff.temperatureDepartZ1.toFloat());

        if(getZone2().getSource() == Zone::SOURCE::CONNECT) {
            getZone2().setTemperatureAmbiante(buff.temperatureAmbianteZ2.toFloat());
            getZone2().setTemperatureConsigne(buff.temperatureConsigneZ2.toFloat());
        }
        getZone2().setTemperatureDepart(buff.temperatureDepartZ2.toFloat());

        if(getZone3().getSource() == Zone::SOURCE::CONNECT) {
            getZone3().setTemperatureAmbiante(buff.temperatureAmbianteZ3.toFloat());
            getZone3().setTemperatureConsigne(buff.temperatureConsigneZ3.toFloat());
        }
        getZone3().setTemperatureDepart(buff.temperatureDepartZ3.toFloat());

        setTemperatureExterieure(buff.temperatureExterieure.toFloat());
        setTemperatureECS(buff.temperatureECS.toFloat());
        setTemperatureCDC(buff.temperatureCDC.toFloat());

        setPression(buff.pression.toFloat());
        return true;
    } while(retry++ < 1);

    return false;
}

bool Connect::recupererConsommation() {
    if(! estAssocie()) {
        return false;
    }

    struct {
        FrisquetRadio::RadioTrameHeader header;
        uint8_t longueurDonnees;
        byte i1[18] = {0};
        fword consommationECS;
        fword consommationChauffage;
        byte i2[34] = {0};
    } buff;
    

    size_t length;
    int16_t err;

    uint8_t retry = 0;
    do {
        length = sizeof(buff);
        err = this->radio().sendAsk(
            this->getId(), 
            ID_CHAUDIERE, 
            this->getIdAssociation(),
            this->incrementIdMessage(),
            0x01,
            0x7A18 + (ID_CHAUDIERE == 0x84 ? 0xC8 : 0x00),
            0x001C,
            (byte*)&buff,
            length
        );

        if(err != RADIOLIB_ERR_NONE) {
            delay(100);
            continue;
        }
        
        setConsommationChauffage(buff.consommationChauffage.toInt16());
        setConsommationECS(buff.consommationECS.toInt16());
        
        return true;
    } while(retry++ < 1);

    return false;
}

void Connect::setTemperatureExterieure(float temperature) {
    _temperatureExterieure = temperature;
}
void Connect::setTemperatureECS(float temperature) {
    _temperatureECS = temperature;
}
void Connect::setTemperatureCDC(float temperature) {
    _temperatureCDC = temperature;
}

float Connect::getTemperatureExterieure() {
    return _temperatureExterieure;
}
float Connect::getTemperatureECS() {
    return _temperatureECS;
}
float Connect::getTemperatureCDC() {
    return _temperatureCDC;
}

void Connect::setConsommationECS(int16_t consommation) {
    _consommationGazECS = consommation;
}
void Connect::setConsommationChauffage(int16_t consommation) {
    _consommationGazChauffage = consommation;
}
int16_t Connect::getConsommationChauffage() {
    return _consommationGazChauffage;
}
int16_t Connect::getConsommationECS() {
    return _consommationGazECS;
}

bool Connect::recupererModeECS() {
    if(! estAssocie()) {
        return false;
    }


    /*
    7E 80 AA 16 05 10 A0 F0 00 0D 1A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 11 // Eco horloge
    7E 80 AA 1A 05 10 A0 F0 00 0D 1A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 09 // Eco
    80 7E AA 03 05 10 A0 F0 00 0D 1A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 09 // Eco
    7E 80 AA 1D 05 10 A0 F0 00 0D 1A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 // max
    7E 80 AA 22 05 10 A0 F0 00 0D 1A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 19 // eco+
    */
    // Demande récupération courte : 80 7E AA 03 01 03 A0 FC 00 01
    struct {
        FrisquetRadio::RadioTrameHeader header;
        uint8_t longueurDonnees;
        byte i1;
        byte modeECS;
    } buff;
    

    size_t length = 0;
    int16_t err;

    uint8_t retry = 0;
    do {
        //length = sizeof(buff);
        err = this->radio().sendAsk(
            this->getId(), 
            ID_CHAUDIERE, 
            this->getIdAssociation(),
            this->incrementIdMessage(),
            0x01,
            0xA0FC,
            0x0001,
            (byte*)&buff,
            length
        );

        if(err != RADIOLIB_ERR_NONE) {
            delay(10);
            continue;
        }
        
        uint8_t raw = buff.modeECS;
        uint8_t masked = raw & 0x7F;
        info("[CONNECT] modeECS reçu brut=0x%02X, masqué=0x%02X", raw, masked);
        setModeECS((MODE_ECS)masked);

        return true;
    } while(retry++ < 1);

    return false;
}

Connect::MODE_ECS Connect::getModeECS() {
    return _modeECS;
}
String Connect::getNomModeECS() {
    switch(getModeECS()) {
        case MODE_ECS::STOP:
            return "Stop";
            break;
        case MODE_ECS::MAX:
            return "Max";
            break;
        case MODE_ECS::ECO:
            return "Eco";
            break;
        case MODE_ECS::ECO_HORAIRES:
            return "Eco Horaires";
            break;
        case MODE_ECS::ECOPLUS:
            return "Eco+";
            break;
        case MODE_ECS::ECOPLUS_HORAIRES:
            return "Eco+ Horaires";
            break;
    }

    return "Inconnu";
}
bool Connect::setModeECS(MODE_ECS modeECS) {
    _modeECS = modeECS;
    return true;
}
bool Connect::setModeECS(const String& modeECS) {
    if (modeECS.equalsIgnoreCase("Max")) {
        this->setModeECS(MODE_ECS::MAX);
    } else if (modeECS.equalsIgnoreCase("Eco")) {
        this->setModeECS(MODE_ECS::ECO);
    } else if (modeECS.equalsIgnoreCase("Eco+")) {
        this->setModeECS(MODE_ECS::ECOPLUS);
    } else if (modeECS.equalsIgnoreCase("Eco Horaires")) {
        this->setModeECS(MODE_ECS::ECO_HORAIRES);
    } else if (modeECS.equalsIgnoreCase("Eco+ Horaires")) {
        this->setModeECS(MODE_ECS::ECOPLUS_HORAIRES);
    } else if (modeECS.equalsIgnoreCase("Stop")) {
        this->setModeECS(MODE_ECS::STOP);
    } else {
        return false;
    }

    return true;
}

bool Connect::envoyerModeECS() {
    if(! estAssocie()) {
        return false;
    }

    info("[CONNECT] Envoi du mode  ECS.");
    
    struct {
        uint8_t i1 = 0x00;
        uint8_t modeECS = 0x00; 
    } payload;
    
    payload.modeECS = getModeECS();
    
    byte buff[RADIOLIB_SX126X_MAX_PACKET_LENGTH];
    size_t length = 0;
    int16_t err;

    uint8_t retry = 0;
    do {
        err = this->radio().sendInit(
            this->getId(), 
            ID_CHAUDIERE, 
            this->getIdAssociation(),
            this->incrementIdMessage(),
            0x01, 
            0xA0FC,
            0x0001,
            0xA0FC,
            0x0001,
            (byte*)&payload,
            sizeof(payload),
            buff,
            length
        );

        if(err != RADIOLIB_ERR_NONE) {
            delay(100);
            continue;
        }
        
        info("[CONNECT] Envoi réussie.");
        return true;
    } while(retry++ < 1);

    info("[CONNECT] Échec de l'envoi.");
    return false;
}

bool Connect::handlePassiveReadResponse(uint16_t adresseMemoire, const byte* buff, size_t length) {
    const uint16_t addrInformations = 0x79E0;
    const uint16_t addrConsommation = 0x7A18;
    const uint16_t addrDate = 0xA02B;
    const uint16_t addrModeEcs = 0xA0FC;

    auto addrIsInformations = adresseMemoire == addrInformations || adresseMemoire == (addrInformations + 0x00C8);
    auto addrIsConsommation = adresseMemoire == addrConsommation || adresseMemoire == (addrConsommation + 0x00C8);
    auto addrIsDate = adresseMemoire == addrDate;

    if (addrIsInformations) {
        debug("[CONNECT] Réception des informations passives du Connect.");
        struct {
            FrisquetRadio::RadioTrameHeader header;
            uint8_t longueurDonnees;
            temperature16 temperatureECS;
            temperature16 temperatureCDC;
            temperature16 temperatureDepartZ1;
            temperature16 temperatureDepartZ2;
            temperature16 temperatureDepartZ3;
            temperature16 temperatureInconnue1;
            temperature16 temperatureInconnue2;
            temperature16 temperatureInconnue3;
            temperature16 temperatureInconnue4;
            temperature16 temperatureInconnue5;
            pression16 pression;
            byte i1[1] = {0};
            byte modeECS;
            temperature16 temperatureECSInstant;
            byte i2[10] = {0};
            temperature16 temperatureAmbianteZ1;
            temperature16 temperatureAmbianteZ2;
            temperature16 temperatureAmbianteZ3;
            byte i3[6] = {0};
            temperature16 temperatureConsigneZ1;
            temperature16 temperatureConsigneZ2;
            temperature16 temperatureConsigneZ3;
            temperature16 temperatureExterieure;
        } resp;

        if (length < sizeof(resp)) {
            return false;
        }
        memcpy(&resp, buff, sizeof(resp));

        if (getZone1().getSource() == Zone::SOURCE::CONNECT) {
            getZone1().setTemperatureAmbiante(resp.temperatureAmbianteZ1.toFloat());
            getZone1().setTemperatureConsigne(resp.temperatureConsigneZ1.toFloat());
        }
        getZone1().setTemperatureDepart(resp.temperatureDepartZ1.toFloat());

        if (getZone2().getSource() == Zone::SOURCE::CONNECT) {
            getZone2().setTemperatureAmbiante(resp.temperatureAmbianteZ2.toFloat());
            getZone2().setTemperatureConsigne(resp.temperatureConsigneZ2.toFloat());
        }
        getZone2().setTemperatureDepart(resp.temperatureDepartZ2.toFloat());

        if (getZone3().getSource() == Zone::SOURCE::CONNECT) {
            getZone3().setTemperatureAmbiante(resp.temperatureAmbianteZ3.toFloat());
            getZone3().setTemperatureConsigne(resp.temperatureConsigneZ3.toFloat());
        }
        getZone3().setTemperatureDepart(resp.temperatureDepartZ3.toFloat());

        setTemperatureExterieure(resp.temperatureExterieure.toFloat());
        setTemperatureECS(resp.temperatureECS.toFloat());
        setTemperatureCDC(resp.temperatureCDC.toFloat());
        setPression(resp.pression.toFloat());

        _lastRecuperationTemperatures = millis();
        publishMqtt();
        _zone1.publishMqtt();
        _zone2.publishMqtt();
        _zone3.publishMqtt();
        return true;
    }

    if (addrIsConsommation) {
        debug("[CONNECT] Réception des consommations passives du Connect.");
        struct {
            FrisquetRadio::RadioTrameHeader header;
            uint8_t longueurDonnees;
            byte i1[18] = {0};
            fword consommationECS;
            fword consommationChauffage;
            byte i2[34] = {0};
        } resp;

        if (length < sizeof(resp)) {
            return false;
        }
        memcpy(&resp, buff, sizeof(resp));

        setConsommationChauffage(resp.consommationChauffage.toInt16());
        setConsommationECS(resp.consommationECS.toInt16());
        _lastRecuperationConsommation = millis();
        publishMqtt();
        return true;
    }

    if(addrIsDate) {
        debug("[CONNECT] Réception de la date passive du Connect.");
        struct {
            FrisquetRadio::RadioTrameHeader header;
            uint8_t longueurDonnees;
            byte date[6] = {0};
            byte i1 = 0;
            byte i2 = 0;
        } resp;

        if (length < sizeof(resp)) {
            return false;
        }
        memcpy(&resp, buff, sizeof(resp));

        Date date = resp.date;
        setDate(date);
        return true;
    }

    if (adresseMemoire == addrModeEcs) {
        struct {
            FrisquetRadio::RadioTrameHeader header;
            uint8_t longueurDonnees;
            byte i1;
            byte modeECS;
        } resp;

        if (length < sizeof(resp)) {
            return false;
        }
        memcpy(&resp, buff, sizeof(resp));

        uint8_t raw = resp.modeECS;
        uint8_t masked = raw & 0x7F;
        info("[CONNECT] modeECS reçu brut=0x%02X, masqué=0x%02X", raw, masked);
        setModeECS((MODE_ECS)masked);
        _lastRecuperationModeECS = millis();
        publishMqtt();
        return true;
    }

    return false;
}

bool Connect::onReceive(byte* donnees, size_t length) {
    ReadBuffer readBuffer(donnees, length);

    FrisquetRadio::RadioTrameHeader header;
    if(readBuffer.remainingLength() < sizeof(header)) { return false; }

    readBuffer.getBytes((byte*)&header, sizeof(header));

    bool passive = getConfig().useConnectPassive();
    if(! estAssocie() && !passive) {
        return false;
    }

    if (header.type == FrisquetRadio::MessageType::READ) {
        if (passive && header.idExpediteur == getId() && header.idDestinataire == ID_CHAUDIERE) {
            if(readBuffer.remainingLength() < sizeof(FrisquetRadio::RadioTrameAsk)) { return false; }
            FrisquetRadio::RadioTrameAsk requete;
            readBuffer.getBytes((byte*)&requete, sizeof(requete));

            size_t lengthRx = 0;
            byte buffRx[RADIOLIB_SX126X_MAX_PACKET_LENGTH];
            int16_t err = radio().receiveExpected(
                ID_CHAUDIERE,
                getId(),
                header.idAssociation,
                header.idMessage,
                header.idReception | 0x80,
                header.type,
                buffRx,
                lengthRx,
                15,
                false
            );
            if (err != RADIOLIB_ERR_NONE) {
                return false;
            }

            debug("[CONNECT] Réception réponse passive lecture adresse 0x%04X", requete.adresseMemoire.toUInt16());
            return handlePassiveReadResponse(requete.adresseMemoire.toUInt16(), buffRx, lengthRx);
        }
        return false;
    }

    if(header.type == FrisquetRadio::MessageType::INIT) {
        FrisquetRadio::RadioTrameInit requete;
        if(readBuffer.remainingLength() < sizeof(requete)) { return false; }
        readBuffer.getBytes((byte*)&requete, sizeof(requete));

        if(requete.adresseMemoireEcriture.toUInt16() == 0xA154 && requete.tailleMemoireEcriture.toUInt16() == 0x0018) { // Modification Zone
            info("[CONNECT] Réception trame Zone (idExpediteur=%d idReception(raw)=%d)", header.idExpediteur, header.idReception);

            struct {
                temperature8 temperatureConfort;    // Début 5°C -> 0 = 50 = 5°C - MAX 30°C
                temperature8 temperatureReduit;     // Début 5°C -> 0 = 50 = 5°C - MAX Confort
                temperature8 temperatureHorsGel;     // Début 5°C -> 0 = 50 = 5°C - MAX Hors gel
                uint8_t mode = 0x00; 
                byte modeOptions = 0b00000100;
                byte inconnu1 = 0x00;
                byte dimanche[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                byte lundi[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                byte mardi[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                byte mercredi[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                byte jeudi[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                byte vendredi[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                byte samedi[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            } donneesZone;

            if(readBuffer.remainingLength() < sizeof(donneesZone)) { return false; }
            readBuffer.getBytes((byte*)&donneesZone, sizeof(donneesZone));

            // Le champ `idReception` peut contenir le bit 0x80 (ACK) : Ajout d'un masque
            uint8_t zoneId = header.idReception & 0x7F;
            
            if(getZone(zoneId).getNumeroZone() == 0) {
                error("[CONNECT] Impossible de mettre à jour la zone %d, numéro de zone invalide.", zoneId);
                return false;
            }

            getZone(zoneId).setMode((Zone::MODE_ZONE)donneesZone.mode);
            getZone(zoneId).setModeOptions(donneesZone.modeOptions);
            getZone(zoneId).setTemperatureReduit(donneesZone.temperatureReduit.toFloat());
            getZone(zoneId).setTemperatureHorsGel(donneesZone.temperatureHorsGel.toFloat());
            if(getZone(zoneId).boostActif()) {
                //getZone(zoneId).setTemperatureBoost(donneesZone.temperatureConfort.toFloat());
            } else {
                getZone(zoneId).setTemperatureConfort(donneesZone.temperatureConfort.toFloat());
            }

            //Sauvegarde de la conf de la zone en NVs
            getZone(zoneId).saveConfig();
            info("[CONNECT] Mise à jour zone %d (id %d), publication MQTT locale.", getZone(zoneId).getNumeroZone(), zoneId );
            getZone(zoneId).publishMqtt();

            if (passive) {
                publishMqtt();
                return true;
            }

            uint8_t retry = 0;
            int16_t err;
            
            info("[CONNECT] Envoi accusé de réception");

            do {
                err = radio().sendAnswer(
                    header.idDestinataire, 
                    header.idExpediteur, 
                    header.idAssociation, 
                    header.idMessage, 
                    header.idReception, 
                    header.type,
                    (byte*)&donneesZone,
                    sizeof(donneesZone)
                );
                if(err != RADIOLIB_ERR_NONE) {
                    delay(100);
                    continue;
                }

                publishMqtt();

                return true;
            } while (retry++ < 5);
        }
    }

    return false;
}


void Connect::begin() {

    loadConfig();

    // Initialisation MQTT
  info("[CONNECT][MQTT] Initialisation des entités.");

    // Device commun
  MqttDevice* device = mqtt().getDevice("openFrisquetVisio");
  
  // Entités
    
  // SENSOR: Température ECS
  _mqttEntities.tempECS.id = "temperatureECS";
  _mqttEntities.tempECS.name = "Température ECS";
  _mqttEntities.tempECS.component = "sensor";
  _mqttEntities.tempECS.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "connect", "temperatureECS"}), 0, true);
  _mqttEntities.tempECS.commandTopic = MqttTopic(MqttManager::compose({device->baseTopic, "connect", "temperatureECS", "set"}), 0, true);
  _mqttEntities.tempECS.set("device_class", "temperature");
  _mqttEntities.tempECS.set("state_class", "measurement");
  _mqttEntities.tempECS.set("unit_of_measurement", "°C");
  mqtt().registerEntity(*device, _mqttEntities.tempECS, true);

  // SENSOR: Température CDC
  _mqttEntities.tempCDC.id = "temperatureCDC";
  _mqttEntities.tempCDC.name = "Température CDC";
  _mqttEntities.tempCDC.component = "sensor";
  _mqttEntities.tempCDC.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "connect", "temperatureCDC"}), 0, true);
  _mqttEntities.tempCDC.set("device_class", "temperature");
  _mqttEntities.tempCDC.set("state_class", "measurement");
  _mqttEntities.tempCDC.set("unit_of_measurement", "°C");
  mqtt().registerEntity(*device, _mqttEntities.tempCDC, true);

  // SENSOR: Température extérieure
  _mqttEntities.tempExterieure.id = "temperatureExterieure";
  _mqttEntities.tempExterieure.name = "Température extérieure";
  _mqttEntities.tempExterieure.component = "sensor";
  if(getConfig().useSondeExterieure() && !getConfig().useDS18B20()) {
    _mqttEntities.tempExterieure.component = "number";
  }
  _mqttEntities.tempExterieure.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "sondeExterieure", "temperatureExterieure"}), 0, true);
  _mqttEntities.tempExterieure.set("device_class", "temperature");
  _mqttEntities.tempExterieure.set("state_class", "measurement");
  _mqttEntities.tempExterieure.set("unit_of_measurement", "°C");
  mqtt().registerEntity(*device, _mqttEntities.tempExterieure, true);

  // SENSOR: Consommation chauffage
  _mqttEntities.consommationChauffage.id = "consommationChauffage";
  _mqttEntities.consommationChauffage.name = "Consommation chauffage";
  _mqttEntities.consommationChauffage.component = "sensor";
  _mqttEntities.consommationChauffage.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "connect", "consommationChauffage"}), 0, true);
  _mqttEntities.consommationChauffage.set("device_class", "energy");
  _mqttEntities.consommationChauffage.set("state_class", "total_increasing");
  _mqttEntities.consommationChauffage.set("unit_of_measurement", "kWh");
  mqtt().registerEntity(*device, _mqttEntities.consommationChauffage, true);

  // SENSOR: Consommation ECS
  _mqttEntities.consommationECS.id = "consommationECS";
  _mqttEntities.consommationECS.name = "Consommation ECS";
  _mqttEntities.consommationECS.component = "sensor";
  _mqttEntities.consommationECS.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "connect", "consommationECS"}), 0, true);
  _mqttEntities.consommationECS.set("device_class", "energy");
  _mqttEntities.consommationECS.set("state_class", "total_increasing");
  _mqttEntities.consommationECS.set("unit_of_measurement", "kWh");
  mqtt().registerEntity(*device, _mqttEntities.consommationECS, true);

   // SELECT: Mode ECS
    _mqttEntities.modeECS.id = "modeECS";
    _mqttEntities.modeECS.name = "Mode ECS";
    _mqttEntities.modeECS.component = "select";
    _mqttEntities.modeECS.stateTopic   = MqttTopic(MqttManager::compose({device->baseTopic,"connect", "modeECS"}), 0, true);
    _mqttEntities.modeECS.commandTopic = MqttTopic(MqttManager::compose({device->baseTopic,"connect", "modeECS","set"}), 0, true);
    _mqttEntities.modeECS.set("icon", "mdi:tune-variant");
    _mqttEntities.modeECS.set("entity_category", "config");
    _mqttEntities.modeECS.setRaw("options", R"(["Max","Eco","Eco Horaires","Eco+", "Eco+ Horaires", "Stop"])");
    mqtt().registerEntity(*device, _mqttEntities.modeECS, true);
    mqtt().onCommand(_mqttEntities.modeECS, [&](const String& payload){
        info("[CONNECT] Changement du mode  ECS : %s.", payload.c_str());
        if (getConfig().useConnectPassive()) {
            info("[CONNECT] Mode passif actif, envoi du mode ECS ignoré.");
            return;
        }
        setModeECS(payload);
        if(envoyerModeECS()) {
            mqtt().publishState(_mqttEntities.modeECS, getNomModeECS());
        }
    });

  // SENSOR: Pression
  _mqttEntities.pression.id = "pression";
  _mqttEntities.pression.name = "Pression";
  _mqttEntities.pression.component = "sensor";
  _mqttEntities.pression.stateTopic = MqttTopic(MqttManager::compose({device->baseTopic, "connect", "pression"}), 0, true);
  _mqttEntities.pression.set("device_class", "pressure");  
  _mqttEntities.pression.set("unit_of_measurement", "bar");
  mqtt().registerEntity(*device, _mqttEntities.pression, true);
}

void Connect::loop() {
    uint32_t now = millis();

    if (getConfig().useConnectPassive()) {
        return;
    }

    if(estAssocie()) {
        if (now - _lastRecuperationTemperatures >= 300000 || _lastRecuperationTemperatures == 0) { // 5 minutes
            info("[CONNECT] Récupération des températures...");
            if(recupererInformations()) {
                _lastRecuperationTemperatures = now;
                publishMqtt();
                _zone1.publishMqtt();
                _zone2.publishMqtt();
                _zone3.publishMqtt();
            } else {
                _lastRecuperationTemperatures = now <= 60000 ? 1 : now - 60000;
                error("[CONNECT] Échec de la récupération des températures.");
            }
            delay(100);
            /*info("[CONNECT] Récupération de la pression...");
            if(recupererPression()) {
                publishMqtt();
            } else {
                _lastRecuperationTemperatures = now <= 60000 ? 1 : now - 60000;
                error("[CONNECT] Échec de la récupération des températures.");
            }*/
        }

        if (now - _lastRecuperationConsommation >= 3600000 || _lastRecuperationConsommation == 0) { // 1 heure
            info("[CONNECT] Récupération des consommations...");
            if(recupererConsommation()) {
                _lastRecuperationConsommation = now;
                publishMqtt();
            } else {
                _lastRecuperationConsommation = now <= 60000 ? 1 : now - 60000;
            }
            delay(100);
        }

        if (now - _lastRecuperationModeECS >= 3600000 || _lastRecuperationModeECS == 0) { // 1 heure
            info("[CONNECT] Récupération du mode ECS...");
            if(recupererModeECS()) {
                publishMqtt();
            } else {
                error("[CONNECT] Récupération impossible");
            }
            _lastRecuperationModeECS = now;
            delay(100);
        }

        if (now - _lastEnvoiZone >= 30000 || _lastEnvoiZone == 0) { // 30 secondes
            envoiZones();
        }
    }
}

void Connect::envoiZones() {
    if(estAssocie()) {
        if(getConfig().useZone1() && getZone1().getSource() == Zone::SOURCE::CONNECT && _zone1.getLastChange() > _zone1.getLastEnvoi()) {
            info("[CONNECT] Envoi de la zone 1.");
            if(envoyerZone(_zone1)) {
                info("[CONNECT] Envoi réussi !");
                _zone1.publishMqtt();
                _zone1.refreshLastEnvoi();
                _envoiZ1 = false;
            }
        }
        if(getConfig().useZone2() && getZone2().getSource() == Zone::SOURCE::CONNECT && _zone2.getLastChange() > _zone2.getLastEnvoi()) {
            info("[CONNECT] Envoi de la zone 2 (id=%d numero=%d).", _zone2.getIdZone(), _zone2.getNumeroZone());
            if(envoyerZone(_zone2)) {
                info("[CONNECT] Envoi réussi !");
                _zone2.publishMqtt();
                _zone2.refreshLastEnvoi();
                _envoiZ2 = false;
            }
        }
        if(getConfig().useZone3() && getZone3().getSource() == Zone::SOURCE::CONNECT && _zone3.getLastChange() > _zone3.getLastEnvoi()) {
            info("[CONNECT] Envoi de la zone 3 (id=%d numero=%d).", _zone3.getIdZone(), _zone3.getNumeroZone());
            if(envoyerZone(_zone3)) {
                info("[CONNECT] Envoi réussi !");
                _zone3.publishMqtt();
                _zone3.refreshLastEnvoi();
                _envoiZ3 = false;
            }
        }
        _lastEnvoiZone = millis();
    }
}

void Connect::publishMqtt() {
    if( !isnan(getTemperatureECS())) {
        mqtt().publishState(*mqtt().getDevice("openFrisquetVisio")->getEntity("temperatureECS"), getTemperatureECS());
    }
    if( !isnan(getTemperatureCDC())) {
        mqtt().publishState(*mqtt().getDevice("openFrisquetVisio")->getEntity("temperatureCDC"), getTemperatureCDC());
    }
    if( !isnan(getTemperatureExterieure())) {
        mqtt().publishState(*mqtt().getDevice("openFrisquetVisio")->getEntity("temperatureExterieure"), getTemperatureExterieure());
    }

    if( getConsommationChauffage() >= 0) {
        static float lastConsommationChauffage = -1;
        if(lastConsommationChauffage != getConsommationChauffage()) {
            lastConsommationChauffage = getConsommationChauffage();
            mqtt().publishState(*mqtt().getDevice("openFrisquetVisio")->getEntity("consommationChauffage"), getConsommationChauffage());
        }
    }
    if( getConsommationECS() >= 0 && getConsommationECS()) {
        static float lastConsommationECS = -1;
        if(lastConsommationECS != getConsommationECS()) {
            lastConsommationECS = getConsommationECS();
            mqtt().publishState(*mqtt().getDevice("openFrisquetVisio")->getEntity("consommationECS"), getConsommationECS());
        }
    }

    if(getModeECS() != MODE_ECS::INCONNU) {
        mqtt().publishState(*mqtt().getDevice("openFrisquetVisio")->getEntity("modeECS"), getNomModeECS().c_str());
    }
    
    if( !isnan(getPression())) {
        mqtt().publishState(*mqtt().getDevice("openFrisquetVisio")->getEntity("pression"), getPression());
    }
}

void Connect::setPression(float pression) {
    _pression = pression;
}

float Connect::getPression() {
    return _pression;
}
