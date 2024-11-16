#include <painlessMesh.h>
#include <ArduinoJson.h>

// Mesh network configuration
#define MESH_PREFIX "painless-mesh"
#define MESH_PASSWORD "mesh-password"
#define MESH_PORT 5555

painlessMesh mesh;

// Maximum propagators, probes, and IOs
#define MAX_PROPAGATORS 10
#define MAX_PROBES 5
#define MAX_IOS 5

struct Probe {
    float temperature;
    float humidity;
};

struct IO {
    uint8_t io_idx;
    float value;
};

struct Message {
    uint32_t initiatorId;
    uint8_t propagatorCount;
    uint32_t propagators[MAX_PROPAGATORS];
    uint8_t numProbes;
    Probe probes[MAX_PROBES];
    uint8_t numIOs;
    IO ios[MAX_IOS];
};

// Deserialize a JSON String into a Message struct
bool deserializeMessage(const String &data, Message &msg) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, data);

    if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        return false;
    }

    // Parse initiator ID
    msg.initiatorId = doc["ID"];

    // Parse propagation
    JsonObject propagation = doc["Propagation"];
    msg.propagatorCount = 0;
    for (JsonPair kv : propagation) {
        if (msg.propagatorCount < MAX_PROPAGATORS) {
            msg.propagators[msg.propagatorCount++] = kv.value().as<uint32_t>();
        }
    }

    // Parse probes
    JsonObject probes = doc["Probes"];
    JsonObject temperatures = probes["Temperature"];
    JsonObject humidities = probes["Humidity"];
    msg.numProbes = 0;
    for (JsonPair kv : temperatures) {
        if (msg.numProbes < MAX_PROBES) {
            msg.probes[msg.numProbes].temperature = kv.value().as<float>();
            msg.probes[msg.numProbes].humidity = humidities[kv.key()].as<float>();
            msg.numProbes++;
        }
    }

    // Parse IOs
    JsonObject ios = doc["IOs"];
    msg.numIOs = 0;
    for (JsonPair kv : ios) {
        if (msg.numIOs < MAX_IOS) {
            msg.ios[msg.numIOs].io_idx = atoi(kv.key().c_str());
            msg.ios[msg.numIOs].value = kv.value().as<float>();
            msg.numIOs++;
        }
    }

    return true;
}

// Serialize a Message struct into a JSON String
String serializeMessage(const Message &msg) {
    StaticJsonDocument<1024> doc;

    // Add initiator ID
    doc["ID"] = msg.initiatorId;

    // Add propagation
    JsonObject propagation = doc.createNestedObject("Propagation");
    for (int i = 0; i < msg.propagatorCount; i++) {
        propagation[String(i)] = msg.propagators[i];
    }

    // Add probes
    JsonObject probes = doc.createNestedObject("Probes");
    JsonObject temperatures = probes.createNestedObject("Temperature");
    JsonObject humidities = probes.createNestedObject("Humidity");
    for (int i = 0; i < msg.numProbes; i++) {
        temperatures[String(i)] = msg.probes[i].temperature;
        humidities[String(i)] = msg.probes[i].humidity;
    }

    // Add IOs
    JsonObject ios = doc.createNestedObject("IOs");
    for (int i = 0; i < msg.numIOs; i++) {
        ios[String(i)] = msg.ios[i].value;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

// Callback for received messages
void receivedCallback(uint32_t from, String &msg) {
    Message receivedMsg;
    //Serial.print("RAW: ");
    //Serial.println(msg);
    if (deserializeMessage(msg, receivedMsg)) {
        int rssi = WiFi.RSSI();
        Serial.println("Gateway received a message:");
        Serial.printf("from node %u with RSSI: %d dBm", from, rssi);
        Serial.printf("  Initiator Node ID: %u\n", receivedMsg.initiatorId);

        Serial.printf("  Propagation:\n");
        for (int i = 0; i < receivedMsg.propagatorCount; i++) {
            Serial.printf("    Propagator %d: Node ID %u\n", i, receivedMsg.propagators[i]);
        }

        Serial.printf("  Probes:\n");
        for (int i = 0; i < receivedMsg.numProbes; i++) {
            Serial.printf("    Probe %d - Temp: %.2f°C, Humidity: %.2f%%\n",
                          i, receivedMsg.probes[i].temperature, receivedMsg.probes[i].humidity);
        }

        Serial.printf("  IOs:\n");
        for (int i = 0; i < receivedMsg.numIOs; i++) {
            Serial.printf("    IO %d - Value: %.2f\n", i, receivedMsg.ios[i].value);
        }
    } else {
        Serial.println("Invalid message received.");
    }
}

// Function to display the mesh node tree
void displayNodeTree() {
    // Get the list of connected nodes
    auto nodeList = mesh.getNodeList(true); // 'true' sorts the nodes by their IDs

    Serial.println("Mesh Network Tree:");
    Serial.printf("Root Node ID: %u\n", mesh.getNodeId());

    // Display the list of nodes
    for (auto &&nodeId : nodeList) {
        Serial.printf("  Connected Node ID: %u\n", nodeId);
    }
}



void setup() {
    Serial.begin(115200);

    // Initialize the mesh network
    mesh.setDebugMsgTypes(ERROR | STARTUP);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(receivedCallback);
}

void loop() {
    mesh.update();
    // Periodically display the node tree
    static unsigned long lastDisplayTime = 0;
    if (millis() - lastDisplayTime > 30000) {  // Every 30 seconds
        lastDisplayTime = millis();
        displayNodeTree();
    }
}
