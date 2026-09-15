#include "crypto.h"
#include "mbedtls/aes.h"
#include "secrets.h"   //only file that should access directly 
#include "Arduino.h"


void aesCtrCrypt(uint8_t *data, size_t len, uint8_t nonceCounter[16]) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, teamKey, 128);

  size_t ncOffset = 0;
  uint8_t streamBlock[16];
  mbedtls_aes_crypt_ctr(&aes, len, &ncOffset, nonceCounter, streamBlock, data, data);

  mbedtls_aes_free(&aes);
}

void generateNonce(uint8_t nonce[16]) {
  for (int i = 0; i < 12; i++) {
    nonce[i] = random(0, 256);
  }
  static uint32_t counter = 0;
  counter++;
  memcpy(nonce + 12, &counter, 4);
}