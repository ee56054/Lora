#include "lora_security.h"
#include <cstring>
#include <chrono>
#include <random>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <sstream>

/* Factory default 128-bit key matching STM32 LoraStm32 */
static const uint8_t FACTORY_DEFAULT_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

static uint8_t s_current_key[16];
static uint8_t s_round_keys[176];
static uint32_t s_tx_nonce = 0;
static bool s_initialized = false;
static std::mutex s_sec_mutex;

/* --- Standard AES-128 S-Box and Rcon Tables --- */
static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

static inline uint8_t xtime(uint8_t x) {
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

static void aes_key_expansion(const uint8_t key[16], uint8_t round_keys[176]) {
    memcpy(round_keys, key, 16);
    uint8_t temp[4];
    uint32_t i = 16;
    uint32_t rcon_idx = 1;

    while (i < 176) {
        temp[0] = round_keys[i - 4];
        temp[1] = round_keys[i - 3];
        temp[2] = round_keys[i - 2];
        temp[3] = round_keys[i - 1];

        if (i % 16 == 0) {
            uint8_t k = temp[0];
            temp[0] = sbox[temp[1]] ^ rcon[rcon_idx++];
            temp[1] = sbox[temp[2]];
            temp[2] = sbox[temp[3]];
            temp[3] = sbox[k];
        }

        round_keys[i + 0] = round_keys[i - 16] ^ temp[0];
        round_keys[i + 1] = round_keys[i - 15] ^ temp[1];
        round_keys[i + 2] = round_keys[i - 14] ^ temp[2];
        round_keys[i + 3] = round_keys[i - 13] ^ temp[3];
        i += 4;
    }
}

static void aes_encrypt_block(const uint8_t in[16], uint8_t out[16], const uint8_t round_keys[176]) {
    uint8_t state[4][4];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            state[r][c] = in[r + 4 * c] ^ round_keys[r + 4 * c];
        }
    }

    for (int round = 1; round <= 10; round++) {
        /* SubBytes */
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                state[r][c] = sbox[state[r][c]];
            }
        }

        /* ShiftRows */
        uint8_t t;
        t = state[1][0]; state[1][0] = state[1][1]; state[1][1] = state[1][2]; state[1][2] = state[1][3]; state[1][3] = t;
        t = state[2][0]; state[2][0] = state[2][2]; state[2][2] = t;
        t = state[2][1]; state[2][1] = state[2][3]; state[2][3] = t;
        t = state[3][3]; state[3][3] = state[3][2]; state[3][2] = state[3][1]; state[3][1] = state[3][0]; state[3][0] = t;

        /* MixColumns (rounds 1..9) */
        if (round < 10) {
            for (int c = 0; c < 4; c++) {
                uint8_t a = state[0][c], b = state[1][c], d = state[2][c], e = state[3][c];
                uint8_t h = a ^ b ^ d ^ e;
                state[0][c] ^= h ^ xtime(a ^ b);
                state[1][c] ^= h ^ xtime(b ^ d);
                state[2][c] ^= h ^ xtime(d ^ e);
                state[3][c] ^= h ^ xtime(e ^ a);
            }
        }

        /* AddRoundKey */
        const uint8_t *rk = &round_keys[round * 16];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                state[r][c] ^= rk[r + 4 * c];
            }
        }
    }

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            out[r + 4 * c] = state[r][c];
        }
    }
}

/* --- CRC16-CCITT for Message Integrity Code (MIC) --- */
static uint16_t compute_mic_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

/* --- AES-128-CTR Stream Cryptor --- */
static void aes128_ctr_crypt(const uint8_t *input, uint8_t *output, uint16_t length, uint32_t nonce) {
    uint8_t iv_block[16];
    uint8_t keystream[16];

    /* Format IV Block matching STM32: [Nonce: 4B] [Tag: 4B] [Block Counter: 8B] */
    iv_block[0] = (uint8_t)(nonce >> 24);
    iv_block[1] = (uint8_t)(nonce >> 16);
    iv_block[2] = (uint8_t)(nonce >> 8);
    iv_block[3] = (uint8_t)(nonce >> 0);
    iv_block[4] = 0xAA;
    iv_block[5] = 0x55;
    iv_block[6] = 0xAA;
    iv_block[7] = 0x55;
    memset(&iv_block[8], 0, 8);

    uint64_t block_counter = 0;
    uint16_t offset = 0;

    while (offset < length) {
        /* Write block counter into last 8 bytes of IV (big-endian) */
        for (int b = 0; b < 8; b++) {
            iv_block[15 - b] = (uint8_t)(block_counter >> (b * 8));
        }

        /* Generate 16 bytes of keystream */
        aes_encrypt_block(iv_block, keystream, s_round_keys);

        uint16_t chunk = (length - offset > 16) ? 16 : (length - offset);
        for (uint16_t j = 0; j < chunk; j++) {
            output[offset + j] = input[offset + j] ^ keystream[j];
        }

        offset += chunk;
        block_counter++;
    }
}

/* --- Public API Implementation --- */

void lora_security_init(const uint8_t key[16]) {
    std::lock_guard<std::mutex> lock(s_sec_mutex);
    const uint8_t *k = key ? key : FACTORY_DEFAULT_KEY;
    memcpy(s_current_key, k, 16);
    aes_key_expansion(k, s_round_keys);

    /* Initialize pseudo-random Nonce using chrono high_resolution_clock */
    uint64_t now_ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    s_tx_nonce = (uint32_t)(now_ns ^ (now_ns >> 32));
    if (s_tx_nonce == 0) {
        s_tx_nonce = 0x56789ABC;
    }

    s_initialized = true;
    std::cout << "[LoRa Security] Engine initialized (AES-128-CTR + MIC, Nonce: 0x"
              << std::hex << s_tx_nonce << std::dec << ")" << std::endl;
}

void lora_security_set_key(const uint8_t key[16]) {
    std::lock_guard<std::mutex> lock(s_sec_mutex);
    const uint8_t *k = key ? key : FACTORY_DEFAULT_KEY;
    memcpy(s_current_key, k, 16);
    aes_key_expansion(k, s_round_keys);
    if (!s_initialized) {
        uint64_t now_ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        s_tx_nonce = (uint32_t)(now_ns ^ (now_ns >> 32));
        if (s_tx_nonce == 0) {
            s_tx_nonce = 0x56789ABC;
        }
        s_initialized = true;
    }
}

bool lora_security_set_key_hex(const std::string &hex_key) {
    if (hex_key.length() != 32) {
        return false;
    }
    uint8_t parsed_key[16];
    for (size_t i = 0; i < 16; i++) {
        std::string byte_str = hex_key.substr(i * 2, 2);
        char *endptr = nullptr;
        long val = strtol(byte_str.c_str(), &endptr, 16);
        if (endptr == byte_str.c_str() || *endptr != '\0') {
            return false;
        }
        parsed_key[i] = (uint8_t)val;
    }
    lora_security_set_key(parsed_key);
    return true;
}

std::string lora_security_get_key_hex() {
    std::lock_guard<std::mutex> lock(s_sec_mutex);
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    for (int i = 0; i < 16; i++) {
        ss << std::setw(2) << (int)s_current_key[i];
    }
    return ss.str();
}

bool lora_security_is_encrypted(const uint8_t *packet, uint16_t len) {
    if (packet == nullptr || len < LORA_SEC_OVERHEAD) {
        return false;
    }
    return (packet[0] == LORA_SEC_MAGIC_BYTE);
}

bool lora_security_encrypt(const uint8_t *plaintext, uint16_t plaintext_len,
                           uint8_t *out_frame, uint16_t *out_frame_len) {
    std::lock_guard<std::mutex> lock(s_sec_mutex);
    if (!s_initialized || plaintext == nullptr || out_frame == nullptr || out_frame_len == nullptr || plaintext_len == 0) {
        return false;
    }
    if (plaintext_len > 240) {
        return false;
    }

    /* 1. Compute MIC over unencrypted plaintext (CRC16-CCITT) */
    uint16_t mic = compute_mic_crc16(plaintext, plaintext_len);

    /* 2. Increment 32-bit transmission nonce */
    s_tx_nonce++;
    uint32_t current_nonce = s_tx_nonce;

    /* 3. Assemble Header: [0x53] [Nonce: 4 bytes big-endian] */
    out_frame[0] = LORA_SEC_MAGIC_BYTE;
    out_frame[1] = (uint8_t)(current_nonce >> 24);
    out_frame[2] = (uint8_t)(current_nonce >> 16);
    out_frame[3] = (uint8_t)(current_nonce >> 8);
    out_frame[4] = (uint8_t)(current_nonce >> 0);

    /* 4. Encrypt plaintext payload with AES-128-CTR */
    aes128_ctr_crypt(plaintext, &out_frame[LORA_SEC_HEADER_LEN], plaintext_len, current_nonce);

    /* 5. Append MIC at the end (2 bytes big-endian) */
    uint16_t mic_pos = LORA_SEC_HEADER_LEN + plaintext_len;
    out_frame[mic_pos + 0] = (uint8_t)(mic >> 8);
    out_frame[mic_pos + 1] = (uint8_t)(mic >> 0);

    *out_frame_len = mic_pos + LORA_SEC_MIC_LEN;
    return true;
}

bool lora_security_decrypt(const uint8_t *in_frame, uint16_t in_frame_len,
                           uint8_t *out_plaintext, uint16_t *out_plaintext_len) {
    std::lock_guard<std::mutex> lock(s_sec_mutex);
    if (!s_initialized || in_frame == nullptr || out_plaintext == nullptr || out_plaintext_len == nullptr) {
        return false;
    }
    if (in_frame_len < LORA_SEC_OVERHEAD || in_frame[0] != LORA_SEC_MAGIC_BYTE) {
        return false;
    }

    uint16_t ciphertext_len = in_frame_len - LORA_SEC_OVERHEAD;
    if (ciphertext_len == 0) {
        return false;
    }

    /* 1. Extract 32-bit big-endian Nonce */
    uint32_t rx_nonce = ((uint32_t)in_frame[1] << 24) |
                        ((uint32_t)in_frame[2] << 16) |
                        ((uint32_t)in_frame[3] << 8)  |
                        ((uint32_t)in_frame[4] << 0);

    /* 2. Decrypt ciphertext using AES-128-CTR */
    aes128_ctr_crypt(&in_frame[LORA_SEC_HEADER_LEN], out_plaintext, ciphertext_len, rx_nonce);

    /* 3. Extract received MIC */
    uint16_t mic_pos = LORA_SEC_HEADER_LEN + ciphertext_len;
    uint16_t rx_mic = ((uint16_t)in_frame[mic_pos + 0] << 8) |
                      ((uint16_t)in_frame[mic_pos + 1] << 0);

    /* 4. Authenticate decrypted plaintext by verifying MIC */
    uint16_t computed_mic = compute_mic_crc16(out_plaintext, ciphertext_len);
    if (rx_mic != computed_mic) {
        /* Authentication Failed: Corrupted packet or wrong encryption key */
        memset(out_plaintext, 0, ciphertext_len);
        *out_plaintext_len = 0;
        return false;
    }

    *out_plaintext_len = ciphertext_len;
    return true;
}
