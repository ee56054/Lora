#ifndef MQTT_BRIDGE_H
#define MQTT_BRIDGE_H

#include "modbus_app.h"
#include <string>
#include <vector>
#include <cstdint>

struct MqttLogEntry {
  std::string timestamp;
  std::string direction; // "TX" or "RX"
  std::string topic;
  std::string payload;
};

struct MqttBridgeStatus {
  bool enabled = false;
  bool connected = false;
  std::string broker_host = "127.0.0.1";
  int broker_port = 1883;
  std::string client_id = "orangepi_lora_modbus";
  std::string topic_prefix = "lora/modbus";
  uint64_t tx_count = 0;
  uint64_t rx_count = 0;
  std::string last_error = "";
  std::string last_connected_time = "Never";
  std::vector<MqttLogEntry> recent_logs;
};

// Start the background MQTT <-> Modbus bridge threads
void start_mqtt_bridge();

// Stop the background MQTT <-> Modbus bridge threads
void stop_mqtt_bridge();

// Force reconnect when MQTT configuration changes
void reload_mqtt_bridge();

// Publish raw or JSON message to `<topic_prefix>/<subtopic>` (or full topic if subtopic starts with '/')
bool mqtt_publish(const std::string &subtopic, const std::string &payload, bool retain = false);

// Publish current Modbus telemetry snapshot for a slave
void mqtt_publish_telemetry(int slave_id, const ModbusTelemetry &telem);

// Publish a Modbus write/read/probe event notification
void mqtt_publish_modbus_event(const std::string &event_type, int slave_id, const std::string &json_payload);

// Get current MQTT bridge status and recent message log
MqttBridgeStatus get_mqtt_bridge_status();

// Clear recent MQTT message log
void clear_mqtt_bridge_logs();

#endif // MQTT_BRIDGE_H

