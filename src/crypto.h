#pragma once
#include <Arduino.h>

// Encrypts/decrypts a buffer in place using AES-128-CTR.
// Same function does both — see explanation in crypto.cpp.
void aesCtrCrypt(uint8_t *data, size_t len, uint8_t nonceCounter[16]);

// Prints the active key in hexadecimal for local debugging.
void printKeyDebug();

// Fills a 16-byte buffer with a fresh, unique nonce for one packet.
void generateNonce(uint8_t nonce[16]);