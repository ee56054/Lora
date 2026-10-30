#include "mqtt_bridge.h"
#include "config.h"
#include "modbus_app.h"
#include <nlohmann/json.hpp>

#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cerrno>

using json = nlohmann::json;

static std::atomic<bool> g_mqtt_running{false};
static std::atomic<bool> g_mqtt_reload_requested{false};
static std::atomic<bool> g_mqtt_connected{false};

static int g_mqtt_sock = -1;
static std::mutex g_sock_write_mutex;

static std::mutex g_status_mutex;
static uint64_t g_tx_count = 0;
static uint64_t g_rx_count = 0;
static std::string g_last_error = "";
static std::string g_last_connected_time = "Never";
static std::deque<MqttLogEntry> g_recent_logs;
static const size_t MAX_MQTT_LOGS = 30;

struct MqttIncomingCmd {
  std::string topic;
  std::string payload;
};

static std::mutex g_cmd_mutex;
static std::condition_variable g_cmd_cv;
static std::deque<MqttIncomingCmd> g_cmd_queue;

static std::string get_time_str() {
  time_t now = time(nullptr);
  char buf[64];
  struct tm *tm_info = localtime(&now);
  if (tm_info) {
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    return std::string(buf);
  }
  return "00:00:00";
}

static std::string get_datetime_str() {
  time_t now = time(nullptr);
  char buf[64];
  struct tm *tm_info = localtime(&now);
  if (tm_info) {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buf);
  }
  return "Unknown";
}

static void add_mqtt_log(const std::string &dir, const std::string &topic, const std::string &payload) {
  std::lock_guard<std::mutex> lock(g_status_mutex);
  if (dir == "TX") {
    g_tx_count++;
  } else if (dir == "RX") {
    g_rx_count++;
  }
  MqttLogEntry entry;
  entry.timestamp = get_time_str();
  entry.direction = dir;
  entry.topic = topic;
  entry.payload = payload;
  g_recent_logs.push_front(entry);
  if (g_recent_logs.size() > MAX_MQTT_LOGS) {
    g_recent_logs.pop_back();
  }
}

static void set_mqtt_error(const std::string &err) {
  std::lock_guard<std::mutex> lock(g_status_mutex);
  g_last_error = err;
}

void clear_mqtt_bridge_logs() {
  std::lock_guard<std::mutex> lock(g_status_mutex);
  g_recent_logs.clear();
}

MqttBridgeStatus get_mqtt_bridge_status() {
  std::lock_guard<std::mutex> lock(g_status_mutex);
  MqttBridgeStatus st;
  st.enabled = g_config.mqtt_enabled;
  st.connected = g_mqtt_connected.load();
  st.broker_host = g_config.mqtt_host;
  st.broker_port = g_config.mqtt_port;
  st.client_id = g_config.mqtt_client_id;
  st.topic_prefix = g_config.mqtt_topic_prefix;
  st.tx_count = g_tx_count;
  st.rx_count = g_rx_count;
  st.last_error = g_last_error;
  st.last_connected_time = g_last_connected_time;
  st.recent_logs.assign(g_recent_logs.begin(), g_recent_logs.end());
  return st;
}

// --- Low-level MQTT v3.1.1 Packet Helpers ---

static void encode_remaining_length(std::vector<uint8_t> &buf, size_t length) {
  do {
    uint8_t encoded_byte = length % 128;
    length /= 128;
    if (length > 0) {
      encoded_byte |= 0x80;
    }
    buf.push_back(encoded_byte);
  } while (length > 0);
}

static void encode_utf8_string(std::vector<uint8_t> &buf, const std::string &s) {
  uint16_t len = (uint16_t)s.size();
  buf.push_back((uint8_t)((len >> 8) & 0xFF));
  buf.push_back((uint8_t)(len & 0xFF));
  buf.insert(buf.end(), s.begin(), s.end());
}

static bool send_all_bytes(int sock, const uint8_t *data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send(sock, data + sent, len - sent, MSG_NOSIGNAL);
    if (n <= 0) {
      if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
        usleep(5000);
        continue;
      }
      return false;
    }
    sent += (size_t)n;
  }
  return true;
}

static std::string normalize_prefix(const std::string &prefix) {
  std::string p = prefix;
  while (!p.empty() && p.back() == '/') {
    p.pop_back();
  }
  if (p.empty()) {
    p = "lora/modbus";
  }
  return p;
}

static std::string build_full_topic(const std::string &subtopic) {
  std::string prefix = normalize_prefix(g_config.mqtt_topic_prefix);
  if (subtopic.empty()) {
    return prefix;
  }
  if (subtopic[0] == '/') {
    return subtopic.substr(1);
  }
  if (subtopic.find(prefix + "/") == 0) {
    return subtopic;
  }
  return prefix + "/" + subtopic;
}

static bool mqtt_send_publish_raw(const std::string &full_topic, const std::string &payload, bool retain) {
  std::lock_guard<std::mutex> lock(g_sock_write_mutex);
  if (!g_mqtt_connected.load() || g_mqtt_sock < 0) {
    return false;
  }

  std::vector<uint8_t> var_and_payload;
  encode_utf8_string(var_and_payload, full_topic);
  var_and_payload.insert(var_and_payload.end(), payload.begin(), payload.end());

  std::vector<uint8_t> packet;
  uint8_t header = 0x30 | (retain ? 0x01 : 0x00); // PUBLISH QoS 0
  packet.push_back(header);
  encode_remaining_length(packet, var_and_payload.size());
  packet.insert(packet.end(), var_and_payload.begin(), var_and_payload.end());

  if (!send_all_bytes(g_mqtt_sock, packet.data(), packet.size())) {
    return false;
  }

  add_mqtt_log("TX", full_topic, payload);
  return true;
}

bool mqtt_publish(const std::string &subtopic, const std::string &payload, bool retain) {
  if (!g_config.mqtt_enabled || !g_mqtt_connected.load()) {
    return false;
  }
  std::string full_topic = build_full_topic(subtopic);
  return mqtt_send_publish_raw(full_topic, payload, retain);
}

void mqtt_publish_telemetry(int slave_id, const ModbusTelemetry &telem) {
  if (!g_config.mqtt_enabled || !g_mqtt_connected.load()) {
    return;
  }

  uint32_t hw_id = 0;
  if (telem.hw_id_high >= 0 && telem.hw_id_low >= 0) {
    hw_id = ((uint32_t)telem.hw_id_high << 16) | (uint32_t)telem.hw_id_low;
  }
  uint32_t tx_cnt = 0;
  if (telem.tx_count_high >= 0 && telem.tx_count_low >= 0) {
    tx_cnt = ((uint32_t)telem.tx_count_high << 16) | (uint32_t)telem.tx_count_low;
  }

  json j = {
    {"slave_id", slave_id},
    {"valve1", telem.valve1},
    {"valve2", telem.valve2},
    {"valve1_status", telem.valve1_status},
    {"valve2_status", telem.valve2_status},
    {"sensor1", telem.sensor1},
    {"sensor2", telem.sensor2},
    {"hw_id", hw_id},
    {"tx_count_high", telem.tx_count_high},
    {"tx_count_low", telem.tx_count_low},
    {"tx_count", tx_cnt},
    {"valve1_reg", telem.valve1_reg},
    {"valve2_reg", telem.valve2_reg},
    {"sensor1_reg", telem.sensor1_reg},
    {"sensor2_reg", telem.sensor2_reg},
    {"slave_id_reg", telem.slave_id_reg},
    {"last_update", telem.last_update}
  };

  std::string dump = j.dump();
  mqtt_publish("telemetry/" + std::to_string(slave_id), dump, false);
  mqtt_publish("telemetry", dump, false);
}

void mqtt_publish_modbus_event(const std::string &event_type, int slave_id, const std::string &json_payload) {
  if (!g_config.mqtt_enabled || !g_mqtt_connected.load()) {
    return;
  }
  mqtt_publish("event/" + event_type + "/" + std::to_string(slave_id), json_payload, false);
}

// --- Command Execution Worker (MQTT -> Modbus) ---

static int parse_slave_id(const json &j, int default_id) {
  if (j.contains("slave_id")) {
    if (j["slave_id"].is_number_integer()) {
      return j["slave_id"].get<int>();
    }
    if (j["slave_id"].is_string()) {
      try {
        return std::stoi(j["slave_id"].get<std::string>());
      } catch (...) {}
    }
  }
  return default_id;
}

static bool parse_bool_or_int(const json &val) {
  if (val.is_boolean()) return val.get<bool>();
  if (val.is_number_integer()) return val.get<int>() != 0;
  if (val.is_string()) {
    std::string s = val.get<std::string>();
    return (s == "1" || s == "true" || s == "TRUE" || s == "on" || s == "ON" || s == "open" || s == "OPEN");
  }
  return false;
}

static void execute_mqtt_modbus_command(const std::string &topic, const std::string &payload) {
  std::string prefix = normalize_prefix(g_config.mqtt_topic_prefix);
  std::string cmd_prefix = prefix + "/cmd";

  std::string action = "";
  if (topic.size() > cmd_prefix.size() && topic.compare(0, cmd_prefix.size() + 1, cmd_prefix + "/") == 0) {
    action = topic.substr(cmd_prefix.size() + 1);
  }

  json req;
  try {
    req = json::parse(payload);
  } catch (const std::exception &e) {
    json err_resp = {
      {"status", "error"},
      {"topic", topic},
      {"error", std::string("Invalid JSON payload: ") + e.what()}
    };
    mqtt_publish("response", err_resp.dump(), false);
    return;
  }

  if (action.empty() && req.contains("action") && req["action"].is_string()) {
    action = req["action"].get<std::string>();
  }

  int slave_id = parse_slave_id(req, g_config.modbus_slave_id);
  std::string req_id = req.value("request_id", "");

  std::cout << "\n[MQTT Bridge] Executing command '" << action << "' for Slave ID " << slave_id
            << " from topic '" << topic << "'" << std::endl;

  json resp;
  resp["action"] = action;
  resp["slave_id"] = slave_id;
  resp["broadcast"] = (slave_id == 0);
  if (!req_id.empty()) {
    resp["request_id"] = req_id;
  }

  if (action == "write_valve") {
    int valve = req.value("valve", 1);
    bool open = false;
    if (req.contains("open")) open = parse_bool_or_int(req["open"]);
    else if (req.contains("state")) open = parse_bool_or_int(req["state"]);
    else if (req.contains("value")) open = parse_bool_or_int(req["value"]);

    if (modbus_write_valve(slave_id, valve, open)) {
      resp["status"] = "ok";
      resp["valve"] = valve;
      resp["open"] = open;
    } else {
      resp["status"] = "error";
      resp["error"] = get_last_modbus_error();
    }
  } else if (action == "write_coil") {
    int address = req.value("address", 0);
    bool state = false;
    if (req.contains("state")) state = parse_bool_or_int(req["state"]);
    else if (req.contains("value")) state = parse_bool_or_int(req["value"]);

    if (modbus_write_coil(slave_id, address, state)) {
      resp["status"] = "ok";
      resp["address"] = address;
      resp["state"] = state;
    } else {
      resp["status"] = "error";
      resp["error"] = get_last_modbus_error();
    }
  } else if (action == "write_register") {
    int address = req.value("address", 0);
    uint16_t value = (uint16_t)req.value("value", 0);

    if (modbus_write_holding_register(slave_id, address, value)) {
      resp["status"] = "ok";
      resp["address"] = address;
      resp["value"] = value;
    } else {
      resp["status"] = "error";
      resp["error"] = get_last_modbus_error();
    }
  } else if (action == "write_multiple_coils") {
    int address = req.value("address", 0);
    if (!req.contains("values") || !req["values"].is_array() || req["values"].empty()) {
      resp["status"] = "error";
      resp["error"] = "Missing or empty 'values' array";
    } else {
      std::vector<uint8_t> vals;
      for (const auto &v : req["values"]) {
        vals.push_back(parse_bool_or_int(v) ? 1 : 0);
      }
      if (modbus_write_multiple_coils(slave_id, address, (int)vals.size(), vals.data())) {
        resp["status"] = "ok";
        resp["address"] = address;
        resp["count"] = vals.size();
      } else {
        resp["status"] = "error";
        resp["error"] = get_last_modbus_error();
      }
    }
  } else if (action == "write_multiple_registers") {
    int address = req.value("address", 0);
    if (!req.contains("values") || !req["values"].is_array() || req["values"].empty()) {
      resp["status"] = "error";
      resp["error"] = "Missing or empty 'values' array";
    } else {
      std::vector<uint16_t> vals;
      for (const auto &v : req["values"]) {
        vals.push_back((uint16_t)v.get<int>());
      }
      if (modbus_write_multiple_holding_registers(slave_id, address, (int)vals.size(), vals.data())) {
        resp["status"] = "ok";
        resp["address"] = address;
        resp["count"] = vals.size();
      } else {
        resp["status"] = "error";
        resp["error"] = get_last_modbus_error();
      }
    }
  } else if (action == "set_slave_id") {
    int current_id = req.value("current_slave_id", slave_id);
    int new_id = req.value("new_slave_id", 1);
    if (modbus_write_remote_slave_id(current_id, new_id)) {
      resp["status"] = "ok";
      resp["current_slave_id"] = current_id;
      resp["new_slave_id"] = new_id;
    } else {
      resp["status"] = "error";
      resp["error"] = get_last_modbus_error();
    }
  } else if (action == "read") {
    std::string type = req.value("type", "holding_register");
    int address = req.value("address", 0);
    bool ok = false;
    int read_val = 0;

    if (type == "coil") {
      bool b = false;
      ok = modbus_read_coil_val(slave_id, address, b);
      read_val = b ? 1 : 0;
    } else if (type == "discrete_input") {
      bool b = false;
      ok = modbus_read_discrete_input_val(slave_id, address, b);
      read_val = b ? 1 : 0;
    } else if (type == "input_register") {
      uint16_t r = 0;
      ok = modbus_read_input_reg_val(slave_id, address, r);
      read_val = r;
    } else if (type == "holding_register") {
      uint16_t r = 0;
      ok = modbus_read_holding_reg_val(slave_id, address, r);
      read_val = r;
    } else {
      resp["status"] = "error";
      resp["error"] = "Invalid read type '" + type + "' (use coil, discrete_input, input_register, or holding_register)";
      mqtt_publish("response", resp.dump(), false);
      return;
    }

    if (ok) {
      resp["status"] = "ok";
      resp["type"] = type;
      resp["address"] = address;
      resp["value"] = read_val;
    } else {
      resp["status"] = "error";
      resp["error"] = get_last_modbus_error();
    }
  } else if (action == "read_all") {
    if (modbus_read_all(slave_id)) {
      ModbusTelemetry telem = get_modbus_telemetry();
      uint32_t hw_id = 0;
      if (telem.hw_id_high >= 0 && telem.hw_id_low >= 0) {
        hw_id = ((uint32_t)telem.hw_id_high << 16) | (uint32_t)telem.hw_id_low;
      }
      uint32_t tx_cnt = 0;
      if (telem.tx_count_high >= 0 && telem.tx_count_low >= 0) {
        tx_cnt = ((uint32_t)telem.tx_count_high << 16) | (uint32_t)telem.tx_count_low;
      }
      resp["status"] = "ok";
      resp["telemetry"] = {
        {"slave_id", telem.slave_id},
        {"valve1", telem.valve1},
        {"valve2", telem.valve2},
        {"valve1_status", telem.valve1_status},
        {"valve2_status", telem.valve2_status},
        {"sensor1", telem.sensor1},
        {"sensor2", telem.sensor2},
        {"hw_id", hw_id},
        {"tx_count_high", telem.tx_count_high},
        {"tx_count_low", telem.tx_count_low},
        {"tx_count", tx_cnt},
        {"valve1_reg", telem.valve1_reg},
        {"valve2_reg", telem.valve2_reg},
        {"sensor1_reg", telem.sensor1_reg},
        {"sensor2_reg", telem.sensor2_reg},
        {"slave_id_reg", telem.slave_id_reg},
        {"last_update", telem.last_update}
      };
    } else {
      resp["status"] = "error";
      resp["error"] = get_last_modbus_error();
    }
  } else if (action == "probe") {
    int timeout_ms = req.value("timeout_ms", 1200);
    ModbusDeviceProbe info;
    bool online = modbus_probe_slave(slave_id, timeout_ms, info);
    resp["status"] = "ok";
    resp["online"] = online;
    if (online) {
      resp["hw_id"] = info.hw_id;
      resp["tx_count"] = info.tx_count;
      resp["valve1"] = info.valve1;
      resp["valve2"] = info.valve2;
      resp["sensor1"] = info.sensor1;
      resp["sensor2"] = info.sensor2;
      resp["slave_id_reg"] = info.slave_id_reg;
    }
  } else {
    resp["status"] = "error";
    resp["error"] = "Unknown command action '" + action + "'. Supported: write_valve, write_coil, write_register, write_multiple_coils, write_multiple_registers, set_slave_id, read, read_all, probe";
  }

  std::string out_str = resp.dump();
  mqtt_publish("response", out_str, false);
  mqtt_publish("response/" + std::to_string(slave_id), out_str, false);
}

static void mqtt_cmd_worker_loop() {
  while (g_mqtt_running.load()) {
    MqttIncomingCmd cmd;
    {
      std::unique_lock<std::mutex> lock(g_cmd_mutex);
      g_cmd_cv.wait_for(lock, std::chrono::milliseconds(250), [] {
        return !g_cmd_queue.empty() || !g_mqtt_running.load();
      });
      if (!g_mqtt_running.load()) break;
      if (g_cmd_queue.empty()) continue;
      cmd = g_cmd_queue.front();
      g_cmd_queue.pop_front();
    }
    execute_mqtt_modbus_command(cmd.topic, cmd.payload);
  }
}

// --- MQTT Socket Connection & Packet Loop ---

static int connect_tcp_socket(const std::string &host, int port) {
  struct addrinfo hints{};
  struct addrinfo *res = nullptr;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(port);
  int err = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
  if (err != 0 || res == nullptr) {
    set_mqtt_error("DNS/address resolution failed for " + host + ":" + port_str);
    return -1;
  }

  int sock = -1;
  for (struct addrinfo *rp = res; rp != nullptr; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock < 0) continue;

    // Set 3-second connect/read/write timeout
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
      break; // Connected
    }
    close(sock);
    sock = -1;
  }

  freeaddrinfo(res);
  if (sock < 0) {
    set_mqtt_error("Unable to connect to MQTT broker at " + host + ":" + port_str + " (" + strerror(errno) + ")");
  }
  return sock;
}

static bool send_mqtt_connect(int sock) {
  std::string client_id = g_config.mqtt_client_id.empty() ? "orangepi_lora_modbus" : g_config.mqtt_client_id;
  std::string username = g_config.mqtt_username;
  std::string password = g_config.mqtt_password;
  std::string will_topic = build_full_topic("status");
  std::string will_msg = "{\"online\":false}";

  uint8_t connect_flags = 0x02; // Clean Session
  connect_flags |= 0x04;        // Will Flag
  connect_flags |= 0x20;        // Will Retain

  if (!username.empty()) {
    connect_flags |= 0x80;
    if (!password.empty()) {
      connect_flags |= 0x40;
    }
  }

  std::vector<uint8_t> var_header_and_payload;
  // Protocol Name "MQTT"
  encode_utf8_string(var_header_and_payload, "MQTT");
  // Protocol Level 4 (v3.1.1)
  var_header_and_payload.push_back(0x04);
  // Connect Flags
  var_header_and_payload.push_back(connect_flags);
  // Keep Alive = 30 seconds
  var_header_and_payload.push_back(0x00);
  var_header_and_payload.push_back(30);

  // Payload: Client ID, Will Topic, Will Message, [Username], [Password]
  encode_utf8_string(var_header_and_payload, client_id);
  encode_utf8_string(var_header_and_payload, will_topic);
  encode_utf8_string(var_header_and_payload, will_msg);
  if (!username.empty()) {
    encode_utf8_string(var_header_and_payload, username);
    if (!password.empty()) {
      encode_utf8_string(var_header_and_payload, password);
    }
  }

  std::vector<uint8_t> packet;
  packet.push_back(0x10); // CONNECT
  encode_remaining_length(packet, var_header_and_payload.size());
  packet.insert(packet.end(), var_header_and_payload.begin(), var_header_and_payload.end());

  if (!send_all_bytes(sock, packet.data(), packet.size())) {
    set_mqtt_error("Failed to send MQTT CONNECT packet");
    return false;
  }

  // Wait for CONNACK (4 bytes: 0x20, 0x02, flags, rc)
  uint8_t connack[4] = {0};
  ssize_t r = recv(sock, connack, 4, MSG_WAITALL);
  if (r != 4 || (connack[0] & 0xF0) != 0x20) {
    set_mqtt_error("Did not receive valid MQTT CONNACK from broker");
    return false;
  }
  if (connack[3] != 0) {
    set_mqtt_error("MQTT broker rejected connection (CONNACK return code " + std::to_string((int)connack[3]) + ")");
    return false;
  }
  return true;
}

static bool send_mqtt_subscribe(int sock, uint16_t packet_id, const std::string &topic_filter) {
  std::vector<uint8_t> body;
  body.push_back((uint8_t)((packet_id >> 8) & 0xFF));
  body.push_back((uint8_t)(packet_id & 0xFF));
  encode_utf8_string(body, topic_filter);
  body.push_back(0x00); // Max QoS 0

  std::vector<uint8_t> packet;
  packet.push_back(0x82); // SUBSCRIBE
  encode_remaining_length(packet, body.size());
  packet.insert(packet.end(), body.begin(), body.end());

  std::lock_guard<std::mutex> lock(g_sock_write_mutex);
  return send_all_bytes(sock, packet.data(), packet.size());
}

static bool send_mqtt_pingreq(int sock) {
  uint8_t pkt[2] = {0xC0, 0x00};
  std::lock_guard<std::mutex> lock(g_sock_write_mutex);
  return send_all_bytes(sock, pkt, 2);
}

static bool send_mqtt_puback(int sock, uint16_t packet_id) {
  uint8_t pkt[4] = {
    0x40, 0x02,
    (uint8_t)((packet_id >> 8) & 0xFF),
    (uint8_t)(packet_id & 0xFF)
  };
  std::lock_guard<std::mutex> lock(g_sock_write_mutex);
  return send_all_bytes(sock, pkt, 4);
}

static void send_mqtt_disconnect(int sock) {
  uint8_t pkt[2] = {0xE0, 0x00};
  std::lock_guard<std::mutex> lock(g_sock_write_mutex);
  send_all_bytes(sock, pkt, 2);
}

static bool read_exact_bytes(int sock, uint8_t *buf, size_t count) {
  size_t got = 0;
  while (got < count) {
    ssize_t n = recv(sock, buf + got, count - got, 0);
    if (n <= 0) {
      if (n < 0 && errno == EINTR) continue;
      return false;
    }
    got += (size_t)n;
  }
  return true;
}

static void mqtt_network_loop() {
  while (g_mqtt_running.load()) {
    if (!g_config.mqtt_enabled) {
      g_mqtt_connected.store(false);
      usleep(500000);
      continue;
    }

    g_mqtt_reload_requested.store(false);
    std::string host = g_config.mqtt_host;
    int port = g_config.mqtt_port;

    int sock = connect_tcp_socket(host, port);
    if (sock < 0) {
      g_mqtt_connected.store(false);
      for (int i = 0; i < 30 && g_mqtt_running.load() && !g_mqtt_reload_requested.load(); ++i) {
        usleep(100000); // Wait 3 seconds before retry
      }
      continue;
    }

    if (!send_mqtt_connect(sock)) {
      close(sock);
      g_mqtt_connected.store(false);
      for (int i = 0; i < 30 && g_mqtt_running.load() && !g_mqtt_reload_requested.load(); ++i) {
        usleep(100000);
      }
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(g_sock_write_mutex);
      g_mqtt_sock = sock;
      g_mqtt_connected.store(true);
    }
    {
      std::lock_guard<std::mutex> lock(g_status_mutex);
      g_last_error = "";
      g_last_connected_time = get_datetime_str();
    }

    std::string cmd_wildcard = build_full_topic("cmd/#");
    std::string cmd_root = build_full_topic("cmd");
    send_mqtt_subscribe(sock, 1, cmd_wildcard);
    send_mqtt_subscribe(sock, 2, cmd_root);

    // Publish online status & initial telemetry snapshot
    json online_msg = {
      {"online", true},
      {"client_id", g_config.mqtt_client_id},
      {"topic_prefix", normalize_prefix(g_config.mqtt_topic_prefix)},
      {"timestamp", get_datetime_str()}
    };
    mqtt_publish("status", online_msg.dump(), true);

    std::cout << "[MQTT Bridge] Connected to " << host << ":" << port
              << " | Subscribed to '" << cmd_wildcard << "'" << std::endl;

    time_t last_ping = time(nullptr);
    bool conn_alive = true;

    while (g_mqtt_running.load() && !g_mqtt_reload_requested.load() && conn_alive && g_config.mqtt_enabled) {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(sock, &rfds);

      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 250000; // 250ms select poll

      int sel = select(sock + 1, &rfds, nullptr, nullptr, &tv);
      if (sel < 0) {
        if (errno == EINTR) continue;
        set_mqtt_error("MQTT socket select error: " + std::string(strerror(errno)));
        conn_alive = false;
        break;
      }

      if (sel > 0 && FD_ISSET(sock, &rfds)) {
        uint8_t fixed_hdr = 0;
        ssize_t n = recv(sock, &fixed_hdr, 1, 0);
        if (n <= 0) {
          set_mqtt_error("MQTT broker closed connection");
          conn_alive = false;
          break;
        }

        // Decode Remaining Length
        size_t multiplier = 1;
        size_t rem_len = 0;
        uint8_t encoded_byte = 0;
        do {
          if (!read_exact_bytes(sock, &encoded_byte, 1)) {
            conn_alive = false;
            break;
          }
          rem_len += (encoded_byte & 127) * multiplier;
          multiplier *= 128;
          if (multiplier > 128 * 128 * 128) {
            conn_alive = false;
            break;
          }
        } while ((encoded_byte & 128) != 0);

        if (!conn_alive) break;

        std::vector<uint8_t> body(rem_len);
        if (rem_len > 0 && !read_exact_bytes(sock, body.data(), rem_len)) {
          set_mqtt_error("Failed reading MQTT packet payload");
          conn_alive = false;
          break;
        }

        uint8_t pkt_type = fixed_hdr & 0xF0;
        if (pkt_type == 0x30 && body.size() >= 2) {
          // PUBLISH packet received
          uint8_t qos = (fixed_hdr >> 1) & 0x03;
          uint16_t topic_len = ((uint16_t)body[0] << 8) | body[1];
          size_t offset = 2;
          if (offset + topic_len <= body.size()) {
            std::string rx_topic(reinterpret_cast<const char *>(body.data() + offset), topic_len);
            offset += topic_len;

            if (qos > 0) {
              if (offset + 2 <= body.size()) {
                uint16_t pkt_id = ((uint16_t)body[offset] << 8) | body[offset + 1];
                offset += 2;
                if (qos == 1) {
                  send_mqtt_puback(sock, pkt_id);
                }
              }
            }

            std::string rx_payload;
            if (offset < body.size()) {
              rx_payload.assign(reinterpret_cast<const char *>(body.data() + offset), body.size() - offset);
            }

            add_mqtt_log("RX", rx_topic, rx_payload);

            // Queue command for execution on the Modbus worker thread
            {
              std::lock_guard<std::mutex> cmd_lock(g_cmd_mutex);
              g_cmd_queue.push_back({rx_topic, rx_payload});
            }
            g_cmd_cv.notify_one();
          }
        }
      }

      time_t now = time(nullptr);
      if (now - last_ping >= 15) {
        if (!send_mqtt_pingreq(sock)) {
          set_mqtt_error("Failed to send MQTT PINGREQ keepalive");
          conn_alive = false;
          break;
        }
        last_ping = now;
      }
    }

    if (conn_alive) {
      json offline_msg = {{"online", false}, {"timestamp", get_datetime_str()}};
      mqtt_send_publish_raw(build_full_topic("status"), offline_msg.dump(), true);
      send_mqtt_disconnect(sock);
    }

    {
      std::lock_guard<std::mutex> lock(g_sock_write_mutex);
      g_mqtt_connected.store(false);
      close(sock);
      g_mqtt_sock = -1;
    }

    if (g_mqtt_running.load() && !g_mqtt_reload_requested.load()) {
      usleep(1000000); // 1s before reconnect
    }
  }
}

void start_mqtt_bridge() {
  if (g_mqtt_running.exchange(true)) {
    return; // Already running
  }
  std::thread net_thread(mqtt_network_loop);
  net_thread.detach();

  std::thread worker_thread(mqtt_cmd_worker_loop);
  worker_thread.detach();
}

void stop_mqtt_bridge() {
  g_mqtt_running.store(false);
  g_cmd_cv.notify_all();
}

void reload_mqtt_bridge() {
  g_mqtt_reload_requested.store(true);
}

