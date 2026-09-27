#ifndef LORA_SECURITY_H
#define LORA_SECURITY_H

#include <cstdint>
#include <cstddef>
#include <string>

#define LORA_SEC_MAGIC_BYTE     0x53  // ASCII 'S' (Secure LoRa Frame)
#define LORA_SEC_HEADER_LEN     5     // 1 byte Magic (0x53) + 4 bytes Packet Nonce / Counter
#define LORA_SEC_MIC_LEN        2     // 2 bytes Message Integrity Code (CRC16-CCITT)
#define LORA_SEC_OVERHEAD       (LORA_SEC_HEADER_LEN + LORA_SEC_MIC_LEN) // 7 bytes total
#define LORA_SEC_KEY_LEN        16    // 128-bit AES Key (16 bytes)

/**
 * @brief Initialize LoRa Security Engine with AES-128 pre-shared key
 * @param key 16-byte AES key (if nullptr, factory default key is used)
 */
void lora_security_init(const uint8_t key[16]);

/**
 * @brief Update the 128-bit AES pre-shared key
 * @param key 16-byte AES key
 */
void lora_security_set_key(const uint8_t key[16]);

/**
 * @brief Update the 128-bit AES key from a 32-character hexadecimal string
 * @param hex_key 32-character hex string (e.g. "2B7E151628AED2A6ABF7158809CF4F3C")
 * @return true if valid hex string, false otherwise
 */
bool lora_security_set_key_hex(const std::string &hex_key);

/**
 * @brief Convert 16-byte key to 32-character hexadecimal string
 */
std::string lora_security_get_key_hex();

/**
 * @brief Check if an incoming raw LoRa buffer is formatted as an encrypted secure frame
 * @param packet Pointer to received packet buffer
 * @param len Total length of received packet
 * @return true if packet begins with LORA_SEC_MAGIC_BYTE (0x53) and has valid minimum length
 */
bool lora_security_is_encrypted(const uint8_t *packet, uint16_t len);

/**
 * @brief Encrypt plaintext payload into a secure LoRa frame with Nonce and MIC
 *
 * Frame Format: [0x53] [Nonce: 4 bytes] [Ciphertext: N bytes] [MIC: 2 bytes]
 *
 * @param plaintext Pointer to plaintext data
 * @param plaintext_len Length of plaintext data
 * @param out_frame Output buffer for secure frame (must be at least plaintext_len + 7 bytes)
 * @param out_frame_len Pointer to store generated frame length
 * @return true on success, false on error
 */
bool lora_security_encrypt(const uint8_t *plaintext, uint16_t plaintext_len,
                           uint8_t *out_frame, uint16_t *out_frame_len);

/**
 * @brief Decrypt secure LoRa frame back into plaintext and verify MIC authenticity
 * @param in_frame Pointer to received encrypted frame
 * @param in_frame_len Length of received frame
 * @param out_plaintext Output buffer for decrypted plaintext (must be at least in_frame_len - 7 bytes)
 * @param out_plaintext_len Pointer to store decrypted plaintext length
 * @return true if decrypted successfully and MIC matched; false if corrupted or wrong key
 */
bool lora_security_decrypt(const uint8_t *in_frame, uint16_t in_frame_len,
                           uint8_t *out_plaintext, uint16_t *out_plaintext_len);

#endif // LORA_SECURITY_H
