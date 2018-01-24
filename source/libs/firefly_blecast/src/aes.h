/**
 * MIT License
 *
 * Copyright (c) 2014 Craig McQueen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *  See: https://github.com/cmcqueen/aes-min
 */

/*****************************************************************************
 * aes.h
 *
 * Minimal byte-oriented AES-128 encryption/decryption implementation suitable
 * for small microprocessors.
 ****************************************************************************/

#ifndef AES_H
#define AES_H

/*****************************************************************************
 * Includes
 ****************************************************************************/

#include <stdint.h>

/*****************************************************************************
 * Defines
 ****************************************************************************/

#define AES_BLOCK_SIZE              16u
#define AES_COLUMN_SIZE             4u
#define AES_NUM_COLUMNS             4u

#define AES_KEY_SCHEDULE_WORD_SIZE  4u

#define AES128_NUM_ROUNDS           10u
#define AES128_KEY_SIZE             16u
#define AES128_KEY_SCHEDULE_SIZE    (AES_BLOCK_SIZE * (AES128_NUM_ROUNDS + 1u))

/*****************************************************************************
 * Function prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"{
#endif  /* __cplusplus */

//void aes128_encrypt(uint8_t p_block[AES_BLOCK_SIZE], const uint8_t p_key_schedule[AES128_KEY_SCHEDULE_SIZE]);
//void aes128_decrypt(uint8_t p_block[AES_BLOCK_SIZE], const uint8_t p_key_schedule[AES128_KEY_SCHEDULE_SIZE]);

//void aes128_key_schedule(uint8_t p_key_schedule[AES128_KEY_SCHEDULE_SIZE], const uint8_t p_key[AES128_KEY_SIZE]);

void aes128_otfks_encrypt(uint8_t p_block[AES_BLOCK_SIZE], uint8_t p_key[AES128_KEY_SIZE]);
void aes128_otfks_decrypt(uint8_t p_block[AES_BLOCK_SIZE], uint8_t p_decrypt_start_key[AES128_KEY_SIZE]);

void aes128_otfks_decrypt_start_key(uint8_t p_key[AES128_KEY_SIZE]);

#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif /* !defined(AES_H) */

