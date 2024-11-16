//986968584
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

// Define Message struct
struct Probe {
  float temperature;
  float humidity;
};

// IO Struct
struct IO {
  int io_idx;    // Index of the IO
  float input;   // Input variable
  float output;  // Output variable (current state)
};

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
    JsonObject io = ios.createNestedObject(String(i));
    io["input"] = msg.ios[i].input;
    io["output"] = msg.ios[i].output;
  }

  String output;
  serializeJson(doc, output);
  return output;
}

bool deserializeMessage(const String &data, Message &msg) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.c_str());
    return false;
  }

  msg.initiatorId = doc["ID"];
  msg.toId = doc["toID"];

  JsonObject propagation = doc["Propagation"];
  msg.propagatorCount = propagation.size();
  int idx = 0;
  for (JsonPair kv : propagation) {
    msg.propagators[idx++] = kv.value().as<uint32_t>();
  }

  JsonObject probes = doc["Probes"];
  JsonObject temperatures = probes["Temperature"];
  JsonObject humidities = probes["Humidity"];
  msg.numProbes = temperatures.size();
  idx = 0;
  for (JsonPair kv : temperatures) {
    msg.probes[idx].temperature = kv.value().as<float>();
    msg.probes[idx].humidity = humidities[String(kv.key().c_str())];
    idx++;
  }

  JsonObject ios = doc["IOs"];
  msg.numIOs = ios.size();
  idx = 0;
  for (JsonPair kv : ios) {
    JsonObject io = kv.value().as<JsonObject>();
    msg.ios[idx].io_idx = idx;
    msg.ios[idx].input = io["input"];
    msg.ios[idx].output = io["output"];
    idx++;
  }

  return true;
}

void printMessage(const Message &msg) {
  Serial.println("---- Message Details ----");
  Serial.print("To ID: ");
  Serial.println(msg.toId);

  Serial.print("Initiator ID: ");
  Serial.println(msg.initiatorId);

  Serial.print("Propagator Count: ");
  Serial.println(msg.propagatorCount);

  Serial.println("Propagators: ");
  for (int i = 0; i < msg.propagatorCount; i++) {
    Serial.print("  Propagator ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(msg.propagators[i]);
  }

  Serial.print("Number of Probes: ");
  Serial.println(msg.numProbes);

  Serial.println("Probes: ");
  for (int i = 0; i < msg.numProbes; i++) {
    Serial.print("  Probe ");
    Serial.print(i + 1);
    Serial.print(" - Temperature: ");
    Serial.print(msg.probes[i].temperature);
    Serial.print(" C, Humidity: ");
    Serial.print(msg.probes[i].humidity);
    Serial.println(" %");
  }

  Serial.print("Number of IOs: ");
  Serial.println(msg.numIOs);

  printIOs(local_msg);
  
  Serial.println("--------------------------");
}

// Print the current IO states
void printIOs(const Message &msg) {
  Serial.println("---- IO States ----");
  for (int i = 0; i < msg.numIOs; i++) {
    Serial.printf("IO %d - Input: %.2f, Output: %.2f\n",
                  msg.ios[i].io_idx, msg.ios[i].input, msg.ios[i].output);
  }
  Serial.println("-------------------");
}

// Update IOs based on local logic or received data
void updateIOs(Message &msg) {
  for (int i = 0; i < msg.numIOs; i++) {
    // Example logic: Output mirrors input
    msg.ios[i].output = msg.ios[i].input;
  }
}

void clearMessage(Message &msg) {
  msg.toId = 0;
  msg.initiatorId = 0;
  msg.propagatorCount = 0;
  msg.numProbes = 0;
  msg.numIOs = 0;
}

void sendMessage() {
  local_msg.initiatorId = mesh.getNodeId();
  local_msg.toId = 112233;
  local_msg.propagatorCount = 0;
  local_msg.numProbes = 2;
  local_msg.numIOs = MAX_IOS;

  local_msg.probes[0] = { 25.3, 60.5 };
  local_msg.probes[1] = { 24.7, 58.1 };

  //local_msg.ios[0] = {0, 1.0};
  //local_msg.ios[1] = {1, 0.0};
  //local_msg.ios[2] = {2, 0.5};

  // Update local IOs
  for (int i = 0; i < local_msg.numIOs; i++) {
    local_msg.ios[i].io_idx = i;
    local_msg.ios[i].input = i * 1.1;  // Example input
    local_msg.ios[i].output = 0.0;     // Initially zero
  }
  updateIOs(local_msg);

  String data = serializeMessage(local_msg);
  mesh.sendBroadcast(data);
  Serial.println("Message sent: " + data);
}

void receivedCallback(uint32_t from, String &data) {
  if (deserializeMessage(data, msg)) {
    if (msg.initiatorId == mesh.getNodeId()) {
      Serial.println("Message from initiator itself, ignoring.");
      return;
    }

    Serial.printf("Message received from node %u\n", from);

    // If the message is addressed to this node, update the local IOs
    if (msg.toId == mesh.getNodeId()) {
      Serial.println("Message is for this node.");

      // Update the local IOs with data from the received message
      for (int i = 0; i < msg.numIOs; i++) {
        local_msg.ios[i].input = msg.ios[i].input;
      }
      updateIOs(local_msg);
      // Display the updated local IOs
      printIOs(local_msg);

      Serial.println("Updated local IOs:");
      clearMessage(msg);
      return;
    }

    // Check if the message has already been propagated by this node
    bool alreadyPropagated = false;
    for (int i = 0; i < msg.propagatorCount; i++) {
      if (msg.propagators[i] == mesh.getNodeId()) {
        alreadyPropagated = true;
        break;
      }
    }

    if (!alreadyPropagated) {
      // Add the current node to the propagation list
      msg.propagators[msg.propagatorCount++] = mesh.getNodeId();

      // Serialize and propagate the updated message
      String updatedData = serializeMessage(msg);
      mesh.sendBroadcast(updatedData);
      Serial.println("Message propagated: " + updatedData);
    } else {
      Serial.println("Message already propagated, ignoring.");
    }

    clearMessage(msg);
  } else {
    Serial.println("Failed to parse received message.");
  }
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
  if (millis() - lastSendTime > 20000) {
    lastSendTime = millis();
    sendMessage();
  }

  static unsigned long lastDisplayTime = 0;
  if (millis() - lastDisplayTime > 15000) {
    lastDisplayTime = millis();
    Serial.println("Local values:");
    printMessage(local_msg);
  }
}