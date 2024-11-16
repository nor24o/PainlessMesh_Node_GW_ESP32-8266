//************************************************************
// Simple example using the painlessMesh library to relay messages
// between an MQTT broker and nodes in the mesh network.
//************************************************************

#include <Arduino.h>
#include <painlessMesh.h>
#include <MQTT.h>
#include <WiFiClient.h>

// Mesh Network Configurations
#define MESH_PREFIX "painless-mesh"
#define MESH_PASSWORD "mesh-password"
#define MESH_PORT 5555

// Wi-Fi Station Configurations
#define STATION_SSID "dlink"
#define STATION_PASSWORD "horvat2017"

// MQTT Configurations
#define HOSTNAME "MQTT_Bridge"
IPAddress mqttBroker(192, 168, 1, 21);

IPAddress myIP(0, 0, 0, 0);

painlessMesh mesh;
WiFiClient wifiClient;
MQTTClient client;

// Callback for messages received from mesh nodes
void receivedCallback(uint32_t from, String &msg) {
    int rssi = WiFi.RSSI();
    //Serial.printf("from node %u with RSSI: %d dBm", from, rssi);
    //Serial.printf("Received from %u: %s\n", from, msg.c_str());
    String topic = "painlessMesh/from/" + String(from);
    String topic_rssi = "painlessMesh/from/" + String(from)+"RSSI";
    client.publish(topic.c_str(), msg.c_str());
    client.publish(topic_rssi.c_str(), String(rssi).c_str());
}

// Callback for messages received from the MQTT broker
#include <ArduinoJson.h>

void messageReceived(String &topic, String &payload) {
    Serial.println("Incoming: " + topic + " - Payload: " + payload);

    if (topic == "painlessMesh/to/gateway" && payload == "getNodes") {
        // Handle getNodes request
        auto nodes = mesh.getNodeList(true);  // Get connected nodes
        String str;
        for (auto &&id : nodes)
            str += String(id) + " ";
        client.publish("painlessMesh/from/gateway", str.c_str());
        Serial.println("Node list sent: " + str);

    } else if (topic.startsWith("painlessMesh/to/")) {
        String targetStr = topic.substring(16);  // Extract target node ID or "broadcast"

        if (targetStr == "broadcast") {
            // Broadcast the payload
            mesh.sendBroadcast(payload);
            Serial.println("Broadcast sent: " + payload);

        } else {
            uint32_t target = strtoul(targetStr.c_str(), NULL, 10);

            if (mesh.isConnected(target)) {
                // Parse the MQTT payload into IO states
                StaticJsonDocument<512> doc;
                    doc["ID"] = mesh.getNodeId();
                    doc["toID"] = target;

                JsonObject propagation = doc.createNestedObject("Propagation");
                JsonObject ios = doc.createNestedObject("IOs");

                // Parse payload like "0,1;1,0;2,1"
                int ioIndex;
                int ioState;
                char *token = strtok(const_cast<char *>(payload.c_str()), ";");
                while (token != NULL) {
                    sscanf(token, "%d,%d", &ioIndex, &ioState);  // Extract IO index and state
                    ios[String(ioIndex)] = ioState;
                    token = strtok(NULL, ";");
                }

                // Serialize the JSON structure
                String jsonString;
                serializeJson(doc, jsonString);

                // Send the constructed payload to the specified node
                mesh.sendSingle(target, jsonString);
                Serial.println("Message sent to node " + targetStr + ": " + jsonString);
            } else {
                client.publish("painlessMesh/from/gateway", "Target node not connected!");
            }
        }
    } else {
        client.publish("painlessMesh/from/gateway", "Unknown topic!");
    }
}



// Get the local IP address of the mesh node
IPAddress getlocalIP() {
    return IPAddress(mesh.getStationIP());
}

void setup() {
    Serial.begin(115200);

    // Mesh initialization
    //mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION); // Debug messages
    //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE );
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 6); // Channel set to 6
    mesh.onReceive(receivedCallback);

    mesh.stationManual(STATION_SSID, STATION_PASSWORD);
    mesh.setHostname(HOSTNAME);
    mesh.setRoot(true);
    mesh.setContainsRoot(true);

    // MQTT initialization
    client.begin(mqttBroker.toString().c_str(), wifiClient);
    client.onMessage(messageReceived);
}

void loop() {
    mesh.update();
    client.loop();

    // Reconnect to MQTT broker if disconnected
    if (myIP != getlocalIP()) {
        myIP = getlocalIP();
        Serial.println("My IP is " + myIP.toString());

        Serial.print("\nConnecting to MQTT broker...");
        while (!client.connect("arduino", "hassio", "hassio")) {
            Serial.print(".");
            delay(1000);
        }

        Serial.println("\nConnected to MQTT broker!");
        client.publish("painlessMesh/from/gateway", "Ready!");
        client.subscribe("painlessMesh/to/#");
    }
        // Reconnect to MQTT if necessary
    if (!client.connected()) {
        if (client.connect("arduino", "hassio", "hassio")) {
            client.subscribe("painlessMesh/to/#");
            client.publish("painlessMesh/from/gateway", "Reconnected!");
        }
    }
}
