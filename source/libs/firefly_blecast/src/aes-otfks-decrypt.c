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
 * aes-otfks-decrypt.c
 *
 * AES-128 decryption with on-the-fly calculation of key schedule.
 ****************************************************************************/

#include "aes.h"

// XOR the specified round key into the AES block.
static inline void aes_add_round_key(uint8_t p_block[AES_BLOCK_SIZE], const uint8_t p_round_key[AES_BLOCK_SIZE]) {
    uint_fast8_t i;

    for (i = 0; i < AES_BLOCK_SIZE; ++i) {
        p_block[i] ^= p_round_key[i];
    }
}

/* Hopefully the compiler reduces this to a single rotate instruction.
 * However in testing with gcc on x86-64, it didn't happen. But it is target-
 * and compiler-specific.
 *
 * Alternatively for a particular platform:
 *     - Use an intrinsic 8-bit rotate function provided by the compiler.
 *     - Use inline assembler.
 *
 * TODO: Examine code produced on the target platform.
 */
static inline uint8_t aes_rotate_left_uint8(uint8_t a, uint_fast8_t num_bits)
{
    return ((a << num_bits) | (a >> (8u - num_bits)));
}

#define AES_REDUCE_BYTE         0x1Bu
#define AES_2_INVERSE           141u

#if 0

/* This is probably the most straight-forward expression of the algorithm.
 * This seems more likely to have variable timing, although inspection
 * of compiled code would be needed to confirm it.
 * It is more likely to have variable timing when no optimisations are
 * enabled. */
static inline uint8_t aes_mul2(uint8_t a)
{
    uint8_t result;

    result = a << 1u;
    if (a & 0x80u)
        result ^= AES_REDUCE_BYTE;
    return result;
}

static inline uint8_t aes_div2(uint8_t a)
{
    uint8_t result;

    result = a >> 1u;
    if (a & 1u)
        result ^= AES_2_INVERSE;
    return result;
}

#elif 0

/* This hopefully has fixed timing, although inspection
 * of compiled code would be needed to confirm it. */
static inline uint8_t aes_mul2(uint8_t a)
{
    static const uint8_t reduce[2] = { 0, AES_REDUCE_BYTE };

    return (a << 1u) ^ reduce[a >= 0x80u];
}

static inline uint8_t aes_div2(uint8_t a)
{
    static const uint8_t reduce[2] = { 0, AES_2_INVERSE };

    return (a >> 1u) ^ reduce[a & 1u];
}

#else

/* This hopefully has fixed timing, although inspection
 * of compiled code would be needed to confirm it. */
static inline uint8_t aes_mul2(uint8_t a)
{
    return (a << 1u) ^ ((-(a >= 0x80u)) & AES_REDUCE_BYTE);
}

static inline uint8_t aes_div2(uint8_t a)
{
    return (a >> 1u) ^ ((-(a & 1u)) & AES_2_INVERSE);
}

#endif


uint8_t aes_mul(uint8_t a, uint8_t b)
{
    uint8_t         result = 0;
    uint_fast8_t    i;
    for (i = 0; i < 8u; i++)
    {
#if 0
        /* This code variant is less likely to have constant execution time,
         * and thus more likely to be vulnerable to timing attacks. */
        if (b & 1)
        {
            result ^= a;
        }
#else
        result ^= (-(b & 1u)) & a;
#endif
        a = aes_mul2(a);
        b >>= 1;
    }
    return result;
}


#define CHAIN_LEN           11u

/* Calculation of inverse in GF(2^8), by exponentiation to power 254.
 * Use minimal addition chain to raise to the power of 254, which requires
 * 11 multiplies.
 * There are many addition chains of length 11 for 254. This one was picked
 * because it has the most multiplies by the previous value, and least
 * references to earlier history, which in theory could minimise the size of
 * prev_values[]. However, in the end we do the simplest possible
 * implementation of the algorithm to minimise code size (because aes_inv() is
 * used to achieve smallest possible S-box implementation), so it doesn't
 * really matter which addition chain we pick.
 */
uint8_t aes_inv(uint8_t a)
{
    static const uint8_t addition_chain_idx[CHAIN_LEN] = { 0, 1, 1, 3, 4, 3, 6, 7, 3, 9, 1 };
    uint_fast8_t    i;
    uint8_t         prev_values[CHAIN_LEN];

    for (i = 0; i < CHAIN_LEN; i++)
    {
        prev_values[i] = a;
        a = aes_mul(a, prev_values[addition_chain_idx[i]]);
    }
    return a;
}

uint8_t aes_sbox(uint8_t a) {
    uint8_t x;

    a = aes_inv(a);

    x = aes_rotate_left_uint8(a, 1u);
    x ^= aes_rotate_left_uint8(x, 1u);
    x ^= aes_rotate_left_uint8(x, 2u);

    return a ^ x ^ 0x63u;
}

uint8_t aes_sbox_inv(uint8_t a)
{
    uint8_t x;

    x = aes_rotate_left_uint8(a, 1u);
    a = aes_rotate_left_uint8(x, 2u);
    x ^= a;
    a = aes_rotate_left_uint8(a, 3u);

    return aes_inv(a ^ x ^ 0x05u);
}

void aes_sbox_inv_apply_block(uint8_t p_block[AES_BLOCK_SIZE])
{
    uint_fast8_t    i;

    for (i = 0; i < AES_BLOCK_SIZE; ++i)
    {
        p_block[i] = aes_sbox_inv(p_block[i]);
    }
}


void aes_sbox_apply_block(uint8_t p_block[AES_BLOCK_SIZE])
{
    uint_fast8_t    i;

    for (i = 0; i < AES_BLOCK_SIZE; ++i)
    {
        p_block[i] = aes_sbox(p_block[i]);
    }
}

//#include "aes-key-schedule-round.h"
/* This is used for aes128_otfks_encrypt(), on-the-fly key schedule encryption.
 * It is also used by aes128_otfks_decrypt_start_key() to calculate the
 * starting key state for decryption with on-the-fly key schedule calculation.
 * rcon for the round must be provided, out of the sequence:
 *     1, 2, 4, 8, 16, 32, 64, 128, 27, 54
 * Subsequent values can be calculated with aes_mul2().
 */
void aes128_key_schedule_round(uint8_t p_key[AES128_KEY_SIZE], uint8_t rcon)
{
    uint_fast8_t    round;
    uint8_t       * p_key_0 = p_key;
    uint8_t       * p_key_m1 = p_key + AES128_KEY_SIZE - AES_KEY_SCHEDULE_WORD_SIZE;

    /* Rotate previous word and apply S-box. Also XOR Rcon for first byte. */
    p_key_0[0] ^= aes_sbox(p_key_m1[1]) ^ rcon;
    p_key_0[1] ^= aes_sbox(p_key_m1[2]);
    p_key_0[2] ^= aes_sbox(p_key_m1[3]);
    p_key_0[3] ^= aes_sbox(p_key_m1[0]);

    for (round = 1; round < AES128_KEY_SIZE / AES_KEY_SCHEDULE_WORD_SIZE; ++round)
    {
        p_key_m1 = p_key_0;
        p_key_0 += AES_KEY_SCHEDULE_WORD_SIZE;

        /* XOR in previous word */
        p_key_0[0] ^= p_key_m1[0];
        p_key_0[1] ^= p_key_m1[1];
        p_key_0[2] ^= p_key_m1[2];
        p_key_0[3] ^= p_key_m1[3];
    }
}

//#include "aes-shift-rows.h"
void aes_shift_rows(uint8_t p_block[AES_BLOCK_SIZE])
{
    uint8_t temp_byte;

    /* First row doesn't shift */

    /* Shift the second row */
    temp_byte = p_block[0 * AES_COLUMN_SIZE + 1u];
    p_block[0  * AES_COLUMN_SIZE + 1u] = p_block[1u * AES_COLUMN_SIZE + 1u];
    p_block[1u * AES_COLUMN_SIZE + 1u] = p_block[2u * AES_COLUMN_SIZE + 1u];
    p_block[2u * AES_COLUMN_SIZE + 1u] = p_block[3u * AES_COLUMN_SIZE + 1u];
    p_block[3u * AES_COLUMN_SIZE + 1u] = temp_byte;

    /* Shift the third row */
    temp_byte = p_block[0 * AES_COLUMN_SIZE + 2u];
    p_block[0  * AES_COLUMN_SIZE + 2u] = p_block[2u * AES_COLUMN_SIZE + 2u];
    p_block[2u * AES_COLUMN_SIZE + 2u] = temp_byte;
    temp_byte = p_block[1u * AES_COLUMN_SIZE + 2u];
    p_block[1u * AES_COLUMN_SIZE + 2u] = p_block[3u * AES_COLUMN_SIZE + 2u];
    p_block[3u * AES_COLUMN_SIZE + 2u] = temp_byte;

    /* Shift the fourth row */
    temp_byte = p_block[3u * AES_COLUMN_SIZE + 3u];
    p_block[3u * AES_COLUMN_SIZE + 3u] = p_block[2u * AES_COLUMN_SIZE + 3u];
    p_block[2u * AES_COLUMN_SIZE + 3u] = p_block[1u * AES_COLUMN_SIZE + 3u];
    p_block[1u * AES_COLUMN_SIZE + 3u] = p_block[0  * AES_COLUMN_SIZE + 3u];
    p_block[0  * AES_COLUMN_SIZE + 3u] = temp_byte;
}

void aes_shift_rows_inv(uint8_t p_block[AES_BLOCK_SIZE])
{
    uint8_t temp_byte;

    /* First row doesn't shift */

    /* Shift the second row */
    temp_byte = p_block[3u * AES_COLUMN_SIZE + 1u];
    p_block[3u * AES_COLUMN_SIZE + 1u] = p_block[2u * AES_COLUMN_SIZE + 1u];
    p_block[2u * AES_COLUMN_SIZE + 1u] = p_block[1u * AES_COLUMN_SIZE + 1u];
    p_block[1u * AES_COLUMN_SIZE + 1u] = p_block[0  * AES_COLUMN_SIZE + 1u];
    p_block[0  * AES_COLUMN_SIZE + 1u] = temp_byte;

    /* Shift the third row */
    temp_byte = p_block[0 * AES_COLUMN_SIZE + 2u];
    p_block[0  * AES_COLUMN_SIZE + 2u] = p_block[2u * AES_COLUMN_SIZE + 2u];
    p_block[2u * AES_COLUMN_SIZE + 2u] = temp_byte;
    temp_byte = p_block[1u * AES_COLUMN_SIZE + 2u];
    p_block[1u * AES_COLUMN_SIZE + 2u] = p_block[3u * AES_COLUMN_SIZE + 2u];
    p_block[3u * AES_COLUMN_SIZE + 2u] = temp_byte;

    /* Shift the fourth row */
    temp_byte = p_block[0 * AES_COLUMN_SIZE + 3u];
    p_block[0  * AES_COLUMN_SIZE + 3u] = p_block[1u * AES_COLUMN_SIZE + 3u];
    p_block[1u * AES_COLUMN_SIZE + 3u] = p_block[2u * AES_COLUMN_SIZE + 3u];
    p_block[2u * AES_COLUMN_SIZE + 3u] = p_block[3u * AES_COLUMN_SIZE + 3u];
    p_block[3u * AES_COLUMN_SIZE + 3u] = temp_byte;
}

void aes_mix_columns(uint8_t p_block[AES_BLOCK_SIZE])
{
    uint8_t         temp_column[AES_COLUMN_SIZE];
    uint_fast8_t    i;
    uint_fast8_t    j;
    uint8_t         byte_value;
    uint8_t         byte_value_2;

    for (i = 0; i < AES_NUM_COLUMNS; i++)
    {
        memset(temp_column, 0, AES_COLUMN_SIZE);
        for (j = 0; j < AES_COLUMN_SIZE; j++)
        {
            byte_value = p_block[i * AES_COLUMN_SIZE + j];
            byte_value_2 = aes_mul2(byte_value);
            temp_column[(j + 0 ) % AES_COLUMN_SIZE] ^= byte_value_2;
            temp_column[(j + 1u) % AES_COLUMN_SIZE] ^= byte_value;
            temp_column[(j + 2u) % AES_COLUMN_SIZE] ^= byte_value;
            temp_column[(j + 3u) % AES_COLUMN_SIZE] ^= byte_value ^ byte_value_2;
        }
        memcpy(&p_block[i * AES_COLUMN_SIZE], temp_column, AES_COLUMN_SIZE);
    }
}

/* 14 = 1110b
 *  9 = 1001b
 * 13 = 1101b
 * 11 = 1011b
 */
void aes_mix_columns_inv(uint8_t p_block[AES_BLOCK_SIZE])
{
    uint8_t         temp_column[AES_COLUMN_SIZE];
    uint_fast8_t    i;
    uint_fast8_t    j;
    uint8_t         byte_value;
    uint8_t         byte_value_2;
    uint8_t         byte_value_4;
    uint8_t         byte_value_8;

    for (i = 0; i < AES_NUM_COLUMNS; i++)
    {
        memset(temp_column, 0, AES_COLUMN_SIZE);
        for (j = 0; j < AES_COLUMN_SIZE; j++)
        {
            byte_value = p_block[i * AES_COLUMN_SIZE + j];
            byte_value_2 = aes_mul2(byte_value);
            byte_value_4 = aes_mul2(byte_value_2);
            byte_value_8 = aes_mul2(byte_value_4);
            temp_column[(j + 0 ) % AES_COLUMN_SIZE] ^= byte_value_8 ^ byte_value_4 ^ byte_value_2;  // 14 = 1110b
            temp_column[(j + 1u) % AES_COLUMN_SIZE] ^= byte_value_8 ^ byte_value;                   //  9 = 1001b
            temp_column[(j + 2u) % AES_COLUMN_SIZE] ^= byte_value_8 ^ byte_value_4 ^ byte_value;    // 13 = 1101b
            temp_column[(j + 3u) % AES_COLUMN_SIZE] ^= byte_value_8 ^ byte_value_2 ^ byte_value;    // 11 = 1011b
        }
        memcpy(&p_block[i * AES_COLUMN_SIZE], temp_column, AES_COLUMN_SIZE);
    }
}


/*****************************************************************************
 * Defines
 ****************************************************************************/

#define AES_KEY_SCHEDULE_FIRST_RCON     1u
#define AES128_KEY_SCHEDULE_LAST_RCON   54u

/*****************************************************************************
 * Functions
 ****************************************************************************/

/* This is used for aes128_otfks_decrypt(), on-the-fly key schedule decryption.
 * rcon for the round must be provided, out of the sequence:
 *     54, 27, 128, 64, 32, 16, 8, 4, 2, 1
 * Subsequent values can be calculated with aes_div2().
 */
static void aes128_key_schedule_inv_round(uint8_t p_key[AES128_KEY_SIZE], uint8_t rcon)
{
    uint_fast8_t    round;
    uint8_t       * p_key_0 = p_key + AES128_KEY_SIZE - AES_KEY_SCHEDULE_WORD_SIZE;
    uint8_t       * p_key_m1 = p_key_0 - AES_KEY_SCHEDULE_WORD_SIZE;

    for (round = 1; round < AES128_KEY_SIZE / AES_KEY_SCHEDULE_WORD_SIZE; ++round)
    {
        /* XOR in previous word */
        p_key_0[0] ^= p_key_m1[0];
        p_key_0[1] ^= p_key_m1[1];
        p_key_0[2] ^= p_key_m1[2];
        p_key_0[3] ^= p_key_m1[3];

        p_key_0 = p_key_m1;
        p_key_m1 -= AES_KEY_SCHEDULE_WORD_SIZE;
    }

    /* Rotate previous word and apply S-box. Also XOR Rcon for first byte. */
    p_key_m1 = p_key + AES128_KEY_SIZE - AES_KEY_SCHEDULE_WORD_SIZE;
    p_key_0[0] ^= aes_sbox(p_key_m1[1]) ^ rcon;
    p_key_0[1] ^= aes_sbox(p_key_m1[2]);
    p_key_0[2] ^= aes_sbox(p_key_m1[3]);
    p_key_0[3] ^= aes_sbox(p_key_m1[0]);
}

/* Calculate the starting key state needed for decryption with on-the-fly key
 * schedule calculation. The starting decryption key state is the last 16 bytes
 * of the AES-128 key schedule.
 * The decryption start key calculation is done in-place in the buffer p_key[].
 * So p_key points to a 16-byte buffer containing the AES-128 key. On exit, it
 * contains the decryption start key state suitable for aes128_otfks_decrypt().
 */
void aes128_otfks_decrypt_start_key(uint8_t p_key[AES128_KEY_SIZE])
{
    uint_fast8_t    round;
    uint8_t         rcon = AES_KEY_SCHEDULE_FIRST_RCON;

    for (round = 0; round < AES128_NUM_ROUNDS; ++round)
    {
        aes128_key_schedule_round(p_key, rcon);

        /* Next rcon */
        rcon = aes_mul2(rcon);
    }
}

/* AES-128 decryption with on-the-fly key schedule calculation.
 *
 * p_block points to a 16-byte buffer of encrypted data to decrypt. Decryption
 * is done in-place in that buffer.
 * p_key must initially point to a starting key state for decryption, which is
 * the last 16 bytes of the AES-128 key schedule. It can be calculated from the
 * AES-128 16-byte key by aes128_otfks_decrypt_start_key(). Key schedule is
 * calculated on-the-fly in that buffer, so the buffer must re-initialised for
 * subsequent decryption operations.
 */
void aes128_otfks_decrypt(uint8_t p_block[AES_BLOCK_SIZE], uint8_t p_key[AES128_KEY_SIZE])
{
    uint_fast8_t    round;
    uint8_t         rcon = AES128_KEY_SCHEDULE_LAST_RCON;

    aes_add_round_key(p_block, p_key);
    aes_shift_rows_inv(p_block);
    aes_sbox_inv_apply_block(p_block);
    for (round = AES128_NUM_ROUNDS - 1u; round >= 1; --round)
    {
        aes128_key_schedule_inv_round(p_key, rcon);
        aes_add_round_key(p_block, p_key);
        aes_mix_columns_inv(p_block);
        aes_shift_rows_inv(p_block);
        aes_sbox_inv_apply_block(p_block);

        /* Previous rcon */
        rcon = aes_div2(rcon);
    }
    aes128_key_schedule_inv_round(p_key, rcon);
    aes_add_round_key(p_block, p_key);
}
