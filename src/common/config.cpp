#include "config.h"
#include <fstream>
#include <iostream>
#include <unordered_map>

LoraConfig g_config;

// --- String mapping helpers ---

static std::string sf_to_str(sx126x_lora_sf_t sf) {
    switch (sf) {
        case SX126X_LORA_SF5: return "SF5";
        case SX126X_LORA_SF6: return "SF6";
        case SX126X_LORA_SF7: return "SF7";
        case SX126X_LORA_SF8: return "SF8";
        case SX126X_LORA_SF9: return "SF9";
        case SX126X_LORA_SF10: return "SF10";
        case SX126X_LORA_SF11: return "SF11";
        case SX126X_LORA_SF12: return "SF12";
        default: return "SF9";
    }
}

static sx126x_lora_sf_t str_to_sf(const std::string& s) {
    if (s == "SF5") return SX126X_LORA_SF5;
    if (s == "SF6") return SX126X_LORA_SF6;
    if (s == "SF7") return SX126X_LORA_SF7;
    if (s == "SF8") return SX126X_LORA_SF8;
    if (s == "SF9") return SX126X_LORA_SF9;
    if (s == "SF10") return SX126X_LORA_SF10;
    if (s == "SF11") return SX126X_LORA_SF11;
    if (s == "SF12") return SX126X_LORA_SF12;
    return SX126X_LORA_SF9;
}

static std::string bw_to_str(sx126x_lora_bw_t bw) {
    switch (bw) {
        case SX126X_LORA_BW_007: return "7.81";
        case SX126X_LORA_BW_010: return "10.42";
        case SX126X_LORA_BW_015: return "15.63";
        case SX126X_LORA_BW_020: return "20.83";
        case SX126X_LORA_BW_031: return "31.25";
        case SX126X_LORA_BW_041: return "41.67";
        case SX126X_LORA_BW_062: return "62.5";
        case SX126X_LORA_BW_125: return "125";
        case SX126X_LORA_BW_250: return "250";
        case SX126X_LORA_BW_500: return "500";
        default: return "125";
    }
}

static sx126x_lora_bw_t str_to_bw(const std::string& s) {
    if (s == "7.81") return SX126X_LORA_BW_007;
    if (s == "10.42") return SX126X_LORA_BW_010;
    if (s == "15.63") return SX126X_LORA_BW_015;
    if (s == "20.83") return SX126X_LORA_BW_020;
    if (s == "31.25") return SX126X_LORA_BW_031;
    if (s == "41.67") return SX126X_LORA_BW_041;
    if (s == "62.5") return SX126X_LORA_BW_062;
    if (s == "125") return SX126X_LORA_BW_125;
    if (s == "250") return SX126X_LORA_BW_250;
    if (s == "500") return SX126X_LORA_BW_500;
    return SX126X_LORA_BW_125;
}

static std::string cr_to_str(sx126x_lora_cr_t cr) {
    switch (cr) {
        case SX126X_LORA_CR_4_5: return "4/5";
        case SX126X_LORA_CR_4_6: return "4/6";
        case SX126X_LORA_CR_4_7: return "4/7";
        case SX126X_LORA_CR_4_8: return "4/8";
        default: return "4/6";
    }
}

static sx126x_lora_cr_t str_to_cr(const std::string& s) {
    if (s == "4/5") return SX126X_LORA_CR_4_5;
    if (s == "4/6") return SX126X_LORA_CR_4_6;
    if (s == "4/7") return SX126X_LORA_CR_4_7;
    if (s == "4/8") return SX126X_LORA_CR_4_8;
    return SX126X_LORA_CR_4_6;
}

bool LoraConfig::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file for reading: " << filepath << std::endl;
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("frequency")) frequency = j["frequency"].get<uint32_t>();
        if (j.contains("tx_power")) tx_power = j["tx_power"].get<int8_t>();
        if (j.contains("spreading_factor")) spreading_factor = str_to_sf(j["spreading_factor"].get<std::string>());
        if (j.contains("bandwidth")) bandwidth = str_to_bw(j["bandwidth"].get<std::string>());
        if (j.contains("coding_rate")) coding_rate = str_to_cr(j["coding_rate"].get<std::string>());
        if (j.contains("preamble_length")) preamble_length = j["preamble_length"].get<uint16_t>();
        if (j.contains("rx_timeout")) rx_timeout = j["rx_timeout"].get<uint32_t>();

        return true;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

bool LoraConfig::save_to_file(const std::string& filepath) const {
    nlohmann::json j;
    j["frequency"] = frequency;
    j["tx_power"] = tx_power;
    j["spreading_factor"] = sf_to_str(spreading_factor);
    j["bandwidth"] = bw_to_str(bandwidth);
    j["coding_rate"] = cr_to_str(coding_rate);
    j["preamble_length"] = preamble_length;
    j["rx_timeout"] = rx_timeout;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file for writing: " << filepath << std::endl;
        return false;
    }

    file << j.dump(4); // 4 spaces indent
    return true;
}
