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

#include "ethers.h"

#include <string.h>

#include "./keccak256.h"
//#include "./sha2.h"

#include "./types.h"
#include "./uECC.h"


static uint32_t readbe(uint8_t *data, uint16_t offset, uint16_t length) {
    data += offset;
    uint32_t result = 0;
    for (uint16_t i = 0; i < length; i++) {
        result <<= 8;
        result += *(data++);
    }
    return result;
}

static bool _setField(Transaction *transaction, uint8_t *data, uint16_t offset, uint16_t length, uint8_t *index) {

    switch (*index) {

        // nonce
        case 0:
            // Nonce is allowed to be 32 bytes, but if it is non-practical to be over 4B
            if (length > 4) { return false; }
            transaction->nonce = readbe(data, offset, length);
            break;

        // gasPrice
        case 1:
            // Gas Price is allowed to be 32 bytes, but if it is non-practical to be over 1T
            if (length > 5) { return false; }
            if (length > 2) {
                transaction->gasPrice = readbe(data, offset, length - 2);
                transaction->gasPriceLow = readbe(data, offset + length - 2, 2);
            } else {
                transaction->gasPrice = 0;
                transaction->gasPriceLow = readbe(data, offset, length);
            }
            break;

        // gasLimit
        case 2:
            // Gas limit is allowed to be 32 bytes, but if it is non-practical to be over 4B
            if (length > 4) { return false; }
            transaction->gasLimit = readbe(data, offset, length);
            break;

        // to
        case 3:
            if (length != 0 && length != 20) { return false; }
            transaction->address = &data[offset];
            transaction->hasAddress = (length == 20);
            break;

        // value
        case 4:
            if (length > 32) { return false; }
            transaction->value = &data[offset];
            transaction->valueLength = length;
            break;

        // data
        case 5:
            transaction->data = &data[offset];
            transaction->dataLength = length;
            break;

        // v, r, s
        case 6:
            if (length == 1) {
                int16_t v = (((int16_t)(data[offset])) - 35) / 2;
                if (v < 0) { v = 0; }
                transaction->chainId = v;
            } else {
                transaction->chainId = 0;
            }
            break;

        case 7:
        case 8:
            return true;

        // Transactions only have 9 fields
        default:
            return false;
    }

    (*index)++;

    return true;
}

static bool _decode(Transaction *transaction, uint8_t *data, uint16_t length, uint16_t offset, uint16_t *consumed, uint8_t *index) {
    if (length == 0) { return false; }

    if (data[offset] >= 0xf8) {
        // Array with extra length prefix

        if (*index != 255) { return false; }
        *index = 0;

        uint16_t ll = (data[offset] - 0xf7);
        if (offset + 1 + ll > length) { return false; }

        uint16_t l = readbe(data, offset + 1, ll);
        if (offset + 1 + ll + l > length) { return false; }

        uint16_t childOffset = offset + 1 + ll;
        while (childOffset < offset + 1 + ll + l) {
            uint16_t childConsumed = 0;
            bool success = _decode(transaction, data, length, childOffset, &childConsumed, index);
            if (!success) { return false; }

            childOffset += childConsumed;
            if (childOffset > offset + 1 + ll + l) { return false; }
        }

        *consumed = 1 + ll + l;
        return true;

    } else if (data[offset] >= 0xc0) {
        // Short-ish array

         if (*index != 255) { return false; }
         *index = 0;

        uint16_t l = (data[offset] - 0xc0);
        if (offset + 1 + l > length) { return false; }

        uint16_t childOffset = offset + 1;
        while (childOffset < offset + 1 + l) {
            uint16_t childConsumed = 0;
            bool success = _decode(transaction, data, length, childOffset, &childConsumed, index);
            if (!success) { return false; }

            childOffset += childConsumed;
            if (childOffset > offset + 1 + l) { return false; }
        }

        *consumed = 1 + l;
        return true;

    } else if (data[offset] >= 0xb8) {
        if (*index == 255) { return false; }

        uint16_t ll = (data[offset] - 0xb7);
        if (offset + 1 + ll > length) { return false; }

        uint16_t l = readbe(data, offset + 1, ll);
        if (offset + 1 + ll + l > length) { return false; }

        bool success = _setField(transaction, data, offset + 1 + ll, l, index);
        if (!success) { return false; }
        *consumed = 1 + ll + l;

        return true;

    } else if (data[offset] >= 0x80) {
        if (*index == 255) { return false; }

        uint16_t l = (data[offset] - 0x80);
        if (offset + 1 + l> length) { return false; }

        bool success = _setField(transaction, data, offset + 1, l, index);
        if (!success) { return false; }
        *consumed = 1 + l;

        return true;
    }

    if (*index == 255) { return false; }

    bool success = _setField(transaction, data, offset, 1, index);
    if (!success) { return false; }
    *consumed = 1;
    return true;
}

bool ethers_decodeTransaction(Transaction* transaction, uint8_t * data, uint16_t length) {
    transaction->rawData = data;
    transaction->rawDataLength = length;

    // @TODO: get length and data from transaction in _decode

    uint8_t index = 255;
    uint16_t consumed = 0;
    bool success = _decode(transaction, data, length, 0, &consumed, &index);
    if (!success || consumed != length || index != 7) { return false; }
    return true;
}

bool ethers_privateKeyToAddress(const uint8_t *privateKey, uint8_t *address) {
    uint8_t publicKey[64];

    bool success = uECC_compute_public_key(privateKey, publicKey, uECC_secp256k1());
    if (!success) { return false; }

    uint8_t hashed[32];
    ethers_keccak256(publicKey, 64, hashed);

    memcpy(address, &hashed[12], 20);

    return true;
}

static char getHexNibble(uint8_t value) {
    value &= 0x0f;
    if (value <= 9) { return '0' + value; }
    return 'a' + (value - 10);
}

void ethers_addressToChecksumAddress(const uint8_t *address, char *checksumAddress) {
    uint8_t end = ETHERS_CHECKSUM_ADDRESS_LENGTH - 1;

    // Null Termination
    checksumAddress[end--] = 0;

    // Expand the address into a lowercase-ascii-nibble representation
    // We go backwads, so don't clobber the address
    for (int8_t i = 20 - 1; i >= 0; i--) {
        checksumAddress[end--] = getHexNibble(address[i]);
        checksumAddress[end--] = getHexNibble(address[i] >> 4);
    }

    // "0x" prefix
    checksumAddress[end--] = 'x';
    checksumAddress[end--] = '0';

    // Compute the hash of the address
    uint8_t hashed[32];
    ethers_keccak256(&checksumAddress[2], 40, hashed);

    // Do the checksum
    for (uint8_t i = 0; i < 40; i += 2) {
        if (checksumAddress[2 + i] >= 'a' && (hashed[i >> 1] >> 4) >= 8) {
            checksumAddress[2 + i] -= 0x20;
        }
        if (checksumAddress[2 + i + 1] >= 'a' && (hashed[i >> 1] & 0x0f) >= 8) {
            checksumAddress[2 + i + 1] -= 0x20;
        }
    }
}

bool ethers_privateKeyToChecksumAddress(const uint8_t *privateKey, char *address) {

    // Place the address (as bytes) into the address for now (scratch)
    bool success = ethers_privateKeyToAddress(privateKey, (uint8_t*)address);
    if (!success) { return false; }

    ethers_addressToChecksumAddress(address, address);

    return true;
}
/*
uint8_t *ethers_debug() {
    return uECC_debug();
}
*/
void ethers_keccak256(const uint8_t *data, uint16_t length, uint8_t *result) {

    SHA3_CTX context;
    keccak_init(&context);
    keccak_update(&context, (const unsigned char*)data, (size_t)length);
    keccak_final(&context, (unsigned char*)result);

    // Clear out the contents of what we hashed (in case it was secret)
    memset((char*)&context, 0, sizeof(SHA3_CTX));
}

/*
void ethers_sha256(uint8_t *data, uint16_t length, uint8_t *result) {

    SHA256_CTX context;
    sha256_Init(&context);
    sha256_Update(&context, (const unsigned char*)data, (size_t)length);
    sha256_Final(&context, (unsigned char*)result);

    // Clear out the contents of what we hashed (in case it was secret)
    memset((char*)&context, 0, sizeof(SHA256_CTX));
}
*/

bool ethers_sign(const uint8_t *privateKey, const uint8_t *digest, uint8_t *result) {

    // Sign the digest
    int success = uECC_sign(
        (const uint8_t*)(privateKey),
        (const uint8_t*)(digest),
        32,
        (uint8_t*)result,
        uECC_secp256k1()
    );

    return (success == 1);
}

uint8_t ethers_getStringLength(uint8_t *value, uint8_t length) {
    // There is probably a better way to do this, but I just used the following
    // Python function:
    //
    // def check(a): return filter(lambda x: (x[1] < x[2]), a)
    //
    // And passed in:
    //
    // check([(i, ((i * 100) / Q) + 1, len(str(int('f' * i, 16)))) for i in xrange(1, 64)])
    //
    // for various values of Q until I go something that worked; Q = 83 yeild only one value,
    // 22 bytes (44 nibbles) being over-estimated by 1 byte, for the worst case.

    // Add 1 for the null-termination (200 because we are measuring nibbles)
    return 2 + ((uint16_t)length * 200) / 83;
}

// Perform in-place division by 10
static uint8_t idiv10(uint8_t *numerator, uint8_t *lengthPtr) {
    uint8_t quotient[*lengthPtr];
    uint8_t quotientOffset = 0;

    // Divide by 10
    size_t length = *lengthPtr;
    for (size_t i = 0; i < length; ++i) {

        // How many input bytes to work with
        size_t j = i + 1 + (*lengthPtr) - length;
        if ((*lengthPtr) < j) { break; }

        // The next digit in the output (from numerator[0:j])
        unsigned int value = readbe(numerator, 0, j);
        quotient[quotientOffset++] = value / 10;

        // Carry down the remainder
        uint8_t numeratorOffset = 0;
        numerator[numeratorOffset++] = value % 10;

        for (uint8_t k = j; k < *lengthPtr; k++) {
            numerator[numeratorOffset++] = numerator[k];
        }

        *lengthPtr = numeratorOffset;
    }

    // Calculate the remainder
    unsigned int remainder = readbe(numerator, 0, *lengthPtr);

    // Find the first no`n-zero (so we can skip them during the copy)
    uint8_t firstNonZero = 0;
    while (firstNonZero < quotientOffset && quotient[firstNonZero] == 0) {
        firstNonZero++;
    }

    // Copy the quotient to the value (stripping leading zeros)
    for (uint8_t i = firstNonZero; i < quotientOffset; i++) {
        numerator[i - firstNonZero] = quotient[i];
    }

    // New length
    *lengthPtr = quotientOffset - firstNonZero;

    return remainder;
}

uint8_t ethers_toString(uint8_t *amountWei, uint8_t amountWeiLength, uint8_t skip, char *result) {

    // The actual offset into the result string we are appending to
    uint8_t offset = 0;

    uint8_t scratch[amountWeiLength];
    memcpy(scratch, amountWei, amountWeiLength);

    // The digit place we are into the base-10 number
    uint8_t place = 0;

    // Whether we have hit any non-zero value yet (so we can strip trailing zeros)
    bool nonZero = false;

    do {
        unsigned int remainder = idiv10(scratch, &amountWeiLength);

        // Only add characters if we are after truncation and not a trailing zero
        if (place >= skip && (nonZero || remainder != 0 || place >= 17)) {
            if (place == 18) { result[offset++] = '.'; }
            result[offset++] = '0' + remainder;
            nonZero = true;
        }

        place++;
    } while (amountWeiLength && !(amountWeiLength == 1 && scratch[0] == 0));

    // Make sure we have at least 1 whole digit (with a decimal point)
    while (place <= 18) {
        if (place >= skip && (nonZero || place >= 17)) {
            if (place == 18) { result[offset++] = '.'; }
            result[offset++] = '0';
        }
        place++;
    }

    // Reverse the digits
    for (uint8_t i = 0; i < offset / 2; i++) {
        char tmp = result[i];
        result[i] = result[offset - i - 1];
        result[offset - i - 1] = tmp;
    }

    // Null termination
    result[offset++] = 0;

    return offset - 1;
}
