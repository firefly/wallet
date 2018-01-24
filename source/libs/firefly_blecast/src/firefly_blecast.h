/**
 * MIT License
 *
 * Copyright (c) 2018 Richard Moore <me@ricmoo.com>
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

#ifndef _FIREFLY_BLECAST_H_
#define _FIREFLY_BLECAST_H_


#include <Arduino.h>

#include <stdint.h>


#define BLECAST_INVALID_ID        0x7f000000

#define BLECAST_MINIMUM_BUFFER    96

typedef struct BLECastMessage {
    // Total payload counts and unique discovered payload counts
    int8_t discoveredPayloadCount;
    int8_t totalPayloadCount;

    // Total number of bytes of the message (-1 if the message is incomplete)
    int16_t size;

    // This is used to store the message as payloads arrive
    uint8_t *data;
    uint16_t maxSize;

    // An ID for this message (BLECAST_INVALID_ID until message is valid)
    uint32_t id;

    // The AES context initialized with the key
    uint8_t aesKey[16];

    // Radio State
    uint8_t radioPinCE;
    uint8_t radioPinCSN;
} BLECastMessage;


#ifdef __cplusplus
extern "C"{
#endif  /* __cplusplus */


bool blecast_init(BLECastMessage *message, uint8_t *key,  uint8_t *data, uint16_t dataLength);

bool blecast_poll(BLECastMessage *message);

void blecast_reset(BLECastMessage *message);

//void blecast_free(BLECastMessage *message);
void blecast_shutdown(BLECastMessage *message);


#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif
