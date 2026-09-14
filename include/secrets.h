#pragma once

// Team's shared AES-128 key — 16 bytes, same on every team watch.
// Generate 16 random bytes once, hardcode them here, never commit this file.
static uint8_t teamKey[16] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
}; // <-- replace these with 16 real random byte values