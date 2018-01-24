/**
  * MIT License
 *
 * Copyright (c) 2017 Richard Moore <me@ricmoo.com>
 * Copyright (c) 2017 Yuet Loo Wong <contact@yuetloo.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __ETHERS_H_
#define __ETHERS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct Transaction {
    uint8_t *rawData;
    uint16_t rawDataLength;

    uint32_t nonce;

    uint32_t gasPrice;
    uint16_t gasPriceLow;

    uint32_t gasLimit;

    uint8_t *address;
    bool hasAddress;

    uint8_t *value;
    uint8_t valueLength;

    uint8_t *data;
    uint16_t dataLength;

    uint8_t chainId;
} Transaction;

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


bool ethers_decodeTransaction(Transaction* transaction, uint8_t * data, uint16_t length);


#define ETHERS_PRIVATEKEY_LENGTH       32
#define ETHERS_PUBLICKEY_LENGTH        64
#define ETHERS_ADDRESS_LENGTH          20


bool ethers_privateKeyToAddress(const uint8_t *privateKey, uint8_t *address);

// "0x" + (40 bytes address) + "\0"
#define ETHERS_CHECKSUM_ADDRESS_LENGTH (2 + 40 + 1)

bool ethers_privateKeyToChecksumAddress(const uint8_t *privateKey, char *address);

// NOTE: As long as the buffer is large enough, this can be called with both
//       parameters as the same buffer (it can compute it in-place)
void ethers_addressToChecksumAddress(const uint8_t *address, char *checksumAddress);

#define ETHERS_KECCAK256_LENGTH        32

void ethers_keccak256(const uint8_t *data, uint16_t length, uint8_t *result);

//void ethers_sha256(uint8_t *data, uint16_t length, uint8_t *result);

uint8_t *ethers_debug();

#define ETHERS_SIGNATURE_LENGTH        64

bool ethers_sign(const uint8_t *privateKey, const uint8_t *digest, uint8_t *result);


uint8_t ethers_getStringLength(uint8_t *value, uint8_t length);
uint8_t ethers_toString(uint8_t *amountWei, uint8_t amountWeiLength, uint8_t skipDecimal, char *result);


uint16_t ethers_getHexStringLength(uint16_t length);
void ethers_toHexString(uint8_t *value, uint16_t length, char *result);


#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif
