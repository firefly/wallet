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
 
/**
 *  A custom video driver for the firefly.
 *
 *  The main reason for this driver is for saving memory. The official
 *  driver creates a large video buffer, which consumes 50% (1024 bytes)
 *  on the ATmega328P.
 *
 *  By contrast, this driver consumes zero bytes of memory beyond its call-stack,
 *  and instead constructs the page byte values from the value of the computed
 *  (x, y) for the objects passed in.
 *
 *  See: https://github.com/adafruit/Adafruit_SSD1306
 *
 */


#include "Arduino.h"

#include <ethers.h>
#include <firefly_qrcode.h>


#define TX_OPTIONS_GAS_LOW                (0x01 << 0)
#define TX_OPTIONS_GAS_MEDIUM             (0x10 << 0)
#define TX_OPTIONS_GAS_HIGH               (0x11 << 0)

#define TX_OPTIONS_EXPENSIVE              (1 << 1)

#define SSD1306_LCDWIDTH                  128
#define SSD1306_LCDHEIGHT                  64




#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


// Configure the display
void display_init(uint8_t address);

// Displays 1 centered QR code (if right is NULL) or 2 QR codes side-by-side.
void display_qrcodes(uint8_t address, QRCode *leftOrCenter, QRCode *right);

// Displays an hour glass in the center of the screen
void display_hourglass(uint8_t address);

// Displays a transaction, with the address above the value
void display_transaction(uint8_t address, Transaction *transaction, char *value);

void display_message(uint8_t address, bool binary, const uint8_t *message, uint8_t length);

void display_clear(uint8_t address);

void display_invert(uint8_t address, bool invert);

void display_debug_int(uint8_t address, uint32_t value);
void display_debug_char(uint8_t address, char chr);
void display_debug_buffer(uint8_t address, uint8_t *value, uint16_t length);
void display_debug_text(uint8_t address, const char *text);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

