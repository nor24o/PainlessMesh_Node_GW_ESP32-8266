// ID 164407141
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

// Define Probe struct
struct Probe {
  float temperature;
  float humidity;
};

// IO Struct
struct IO {
  float input;   // Input variable
  float output;  // Output variable (current state)
};

// Message struct
struct Message {
  uint32_t toId;
  uint32_t initiatorId;
  int propagatorCount;
  uint32_t propagators[MAX_PROPAGATORS];
  int numProbes;
  Probe probes[MAX_PROBES];
  int numIOs;
  IO ios[MAX_IOS];
};

Message msg;        // For propagated messages
Message local_msg;  // For local data

// Serialize the message into a JSON string
String serializeMessage(const Message &msg) {
  StaticJsonDocument<1024> doc;

  doc["ID"] = msg.initiatorId;
  doc["toID"] = msg.toId;

  JsonObject propagation = doc.createNestedObject("Propagation");
  for (int i = 0; i < msg.propagatorCount; i++) {
    propagation[String(i)] = msg.propagators[i];
  }

  JsonObject probes = doc.createNestedObject("Probes");
  JsonObject temperatures = probes.createNestedObject("Temperature");
  JsonObject humidities = probes.createNestedObject("Humidity");
  for (int i = 0; i < msg.numProbes; i++) {
    temperatures[String(i)] = msg.probes[i].temperature;
    humidities[String(i)] = msg.probes[i].humidity;
  }

  JsonObject ios = doc.createNestedObject("IOs");
  for (int i = 0; i < msg.numIOs; i++) {
    JsonObject io = ios.createNestedObject(String(i));  // Use the index as the key
    io["I"] = msg.ios[i].input;
    io["O"] = msg.ios[i].output;
  }

  String output;
  serializeJson(doc, output);
  return output;
}

// Deserialize a JSON string into the message struct
bool deserializeMessage(const String &data, Message &msg) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return false;
  }

  msg.initiatorId = doc["ID"] | 0;
  msg.toId = doc["toID"] | 0;

  if (doc.containsKey("Propagation")) {
    JsonObject propagation = doc["Propagation"];
    msg.propagatorCount = propagation.size();
    int idx = 0;
    for (JsonPair kv : propagation) {
      msg.propagators[idx++] = kv.value().as<uint32_t>();
    }
  } else {
    msg.propagatorCount = 0;
  }

  if (doc.containsKey("Probes")) {
    JsonObject probes = doc["Probes"];
    if (probes.containsKey("Temperature") && probes.containsKey("Humidity")) {
      JsonObject temperatures = probes["Temperature"];
      JsonObject humidities = probes["Humidity"];
      msg.numProbes = temperatures.size();
      int idx = 0;
      for (JsonPair kv : temperatures) {
        msg.probes[idx].temperature = kv.value().as<float>();
        msg.probes[idx].humidity = humidities[String(kv.key().c_str())];
        idx++;
      }
    } else {
      msg.numProbes = 0;
    }
  } else {
    msg.numProbes = 0;
  }

  if (doc.containsKey("IOs")) {
    JsonObject ios = doc["IOs"];
    msg.numIOs = ios.size();
        for (JsonPair kv : ios) {
          int idx = String(kv.key().c_str()).toInt(); // Use the key as the index
          if (idx >= 0 && idx < MAX_IOS) { // Ensure the index is within bounds
            JsonObject io = kv.value().as<JsonObject>();
            msg.ios[idx].input = io["I"] | 0.0;
            msg.ios[idx].output = io["O"] | 0.0;
          }
        }
  } else {
    msg.numIOs = 0;
  }

  return true;
}

// Callback for received messages
void receivedCallback(uint32_t from, String &data) {
  Serial.print("RAW: ");
  Serial.println(data);

  if (deserializeMessage(data, msg)) {
    if (msg.initiatorId == mesh.getNodeId()) {
      Serial.println("Message from initiator itself, ignoring.");
      return;
    }

    Serial.printf("Message received from node %u\n", from);

    if (msg.toId == mesh.getNodeId()) {
      Serial.println("Message is for this node.");
      
     
      for (int i = 0; i < MAX_IOS; i++) {
        Serial.printf("Updated IO[%d] to new input: %.2f\n", i, msg.ios[i].input);
        local_msg.ios[i].input = msg.ios[i].input;
      }
      return;
    }

    bool alreadyPropagated = false;
    for (int i = 0; i < msg.propagatorCount; i++) {
      if (msg.propagators[i] == mesh.getNodeId()) {
        alreadyPropagated = true;
        break;
      }
    }

    if (!alreadyPropagated) {
      msg.propagators[msg.propagatorCount++] = mesh.getNodeId();
      String updatedData = serializeMessage(msg);
      mesh.sendBroadcast(updatedData);
      //Serial.println("Message propagated: " + updatedData);
      Serial.println("Message propagated: ");
    } else {
      Serial.println("Message already propagated, ignoring.");
    }
  } else {
    Serial.println("Failed to parse received message.");
  }
}

void sendMessage() {
  local_msg.initiatorId = mesh.getNodeId();
  local_msg.toId = 112233;  // Replace with the target node ID
  local_msg.propagatorCount = 0;
  local_msg.numProbes = 1;
  local_msg.numIOs = 3;

    for (int i = 0; i < local_msg.numProbes; i++) {
    //local_msg.ios[i].input = random(0, 10);  // Example input values
    local_msg.probes[i].temperature = random(10, 20); // Example output values
    local_msg.probes[i].humidity = random(10, 20); // Example output values
  }

  for (int i = 0; i < local_msg.numIOs; i++) {
    //local_msg.ios[i].input = random(0, 10);  // Example input values
    local_msg.ios[i].output = random(10, 20); // Example output values
  }

  String data = serializeMessage(local_msg);
  mesh.sendBroadcast(data);
  //Serial.println("Message sent: " + data);
  Serial.println("Message sent: ");
}

void setup() {
  Serial.begin(115200);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 6);
  mesh.onReceive(receivedCallback);
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  pinMode(2, OUTPUT);
}

void loop() {
  mesh.update();

  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 6000) {
    lastSendTime = millis();
    sendMessage();
  }
  digitalWrite(2, local_msg.ios[2].input);
}

