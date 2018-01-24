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
 

// See:
// http://www.ermicro.com/blog/?p=744
// http://playground.arduino.cc/Code/ATMELTWI


#include "firefly_display.h"

// Constants for I2C (see: http://www.atmel.com/webdoc/AVRLibcReferenceManual/group__util__twi.html)
#include <util/twi.h>

// Font Data
#include "firefly_fonts.h"

// Add attributes (i.e. packed) to an enum
// https://stackoverflow.com/questions/2791739/c-packing-a-typedef-enum
#ifdef __GNUC__
#define attribute(x) __attribute__((x));
#else
#define attribute(x)
#endif


// We use this to make sure the packing worked
// https://www.pixelbeat.org/programming/gcc/static_assert.html
#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
/* These can't be used after statements in c89. */
#ifdef __COUNTER__
  #define STATIC_ASSERT(e,m) \
    ;enum { ASSERT_CONCAT(static_assert_, __COUNTER__) = 1/(int)(!!(e)) }
#else
  /* This can't be used twice on the same line so ensure if using in headers
   * that the headers are not included twice (by wrapping in #ifndef...#endif)
   * Note it doesn't cause an issue when used on same line of separate modules
   * compiled with gcc -combine -fwhole-program.  */
  #define STATIC_ASSERT(e,m) \
    ;enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(int)(!!(e)) }
#endif

// SSD1306 Command Codes
// See: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
enum DisplayCommand {

    // Set Contrast  Control (1 byte operand follows)
    DisplayCommandSetContrast             = 0x81,        // Reset: 0x7f
    
    // Set Entire Display On
    //DisplayCommandSetFollowsRAM         = 0xa4,        // Reset
    //DisplayCommandSetIgnoresRAM         = 0xa5,
    
    // Set Normal/Inverse Display
    DisplayCommandSetNormal               = 0xa6,        // Reset
    DisplayCommandSetInvert               = 0xa7,

    // Set Display On/Off
    DisplayCommandOff                     = 0xae,        // Reset
    DisplayCommandOn                      = 0xaf,
    
    // Set memory address mode (1 byte operand follows; see below)
    DisplayCommandMemoryAddressMode               = 0x20,
    DisplayCommandMemoryAddressModeHorizontal     = 0x00,
    DisplayCommandMemoryAddressModeVertical       = 0x01,    
    DisplayCommandMemoryAddressModePage           = 0x02,       // Reset

    // Set Column Address (3 byte operation follows; start and end; Reset: 0-127)
    DisplayCommandSetColumnAddress                = 0x21,
    
    // Set Page Address (3 byte operation follows; start and end; Reset: 0-7)
    DisplayCommandSetPageAddress                  = 0x22,

    // Set Segment Re-map
    DisplayCommandSegmentRemapNone            = 0xa0,    // Reset
    DisplayCommandSegmentRemap                = 0xa1,

    // Set COM Output Scan Direction
    DisplayCommandCOMScanIncrement            = 0xc0,    // Reset
    DisplayCommandCOMScanDecrement            = 0xc8,

    // Set Charge Pump Settings (See SSD1306 Application Notes; Section 2.1)
    // NOTE: A DisplayCommandOn (0xaf) must be issued afterward to activate
    DisplayCommandSetChargePump               = 0x8d,
    DisplayCommandSetChargePumpDisabled       = 0x10,    // Reset
    DisplayCommandSetChargePumpEnabled        = 0x14,

    // Set Pre-charge Period (1 byte operand follows; Reset 0x22)
    // A[3:0] - Phase 1 period of up to 15 DCLK clocks; 0 is invalid
    // A[7:4] - Phase 2 period of up to 15 DCLK clocks; 0 is invalid
    DisplayCommandSetPrechargePeriod          = 0xd9,

    // Set the V_COMH Deselecty Level (1 byte follows; Reset: 0x20)
    // A[6:4] - 0x00 = 0.65 x Vcc; 0x20 = 0.77 x Vcc ; 0x30 = 0.83 x Vcc 
    DisplayCommandSetVCOMHDeselectLevel       = 0xdb,

} attribute(packed);
typedef enum DisplayCommand DisplayCommand;

STATIC_ASSERT( sizeof ( DisplayCommand ) == 1, "DisplayCommand is incorrect width");


// Display Context
typedef struct DisplayContext {
    // We need 16 bytes per page + the 0x40 control byte per chunk
    uint8_t buffer[17];
    uint8_t length;
    uint8_t address;
} DisplayContext;


typedef enum ControlType {
    ControlTypeStart,
    ControlTypeData,
    ControlTypeStop,
} ControlType;

// We are trying a higher I2C frequency of 400kHz; if this is a bugging out your display, 100kHz is safe
#define F_I2C 400000L


static void i2c_init(DisplayContext *context) {
    
    // Enable the I2C bus on the chip pins
    digitalWrite(SDA, 1);
    digitalWrite(SCL, 1);

    // Set Pre-Scalar bits in the Status register and the Bit-Rate generator
    TWSR &= ~(TWPS0 | TWPS1);
    TWBR = ((F_CPU / F_I2C) - 16) / 2;

    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);

    // Set the address
    TWAR = (context->address << 1);
}


static uint8_t i2c_setControl(ControlType controlType) {
    switch(controlType) {
        case ControlTypeStart:
            TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
            break;
        case ControlTypeData:
            TWCR = (1 << TWINT) | (1 << TWEN);
            break;
        case ControlTypeStop:
            TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
            // Stop does not wait 
            return 0;
    }
    
    while (!(TWCR & (1 << TWINT)));

    // Mask out the prescaler and reserved bits
    return (TWSR & 0xF8);
}

static void i2c_begin(DisplayContext *context) {
    context->length = 0;
}

static void i2c_push(DisplayContext *context, uint8_t data) {
    context->buffer[context->length++] = data;
}

static void i2c_flush(DisplayContext *context) {
    uint8_t status;
 
    // Start
    status = i2c_setControl(ControlTypeStart);
    
    // i.e. status == TW_START

    // Set the slave for write
    TWDR = (context->address << 1) | TW_WRITE;
    i2c_setControl(ControlTypeData);

    // i.e. status == TW_MT_SLA_ACK
    
    // Write data
    for (uint8_t i = 0; i < context->length; i++) {
        TWDR = context->buffer[i];
        status = i2c_setControl(ControlTypeData);

        // i.e. status == TW_MT_DATA_ACK
    }

    // Stop
    status = i2c_setControl(ControlTypeStop);
    
    // i.e. status == 0 (returned explicitly in i2cSetControl)
    
    context->length = 0;
}

static void ssd1306_command(DisplayContext *context, uint8_t command) {
    i2c_begin(context);
    i2c_push(context, 0x00);
    i2c_push(context, command);
    i2c_flush(context);
}

static void display_begin(DisplayContext *context, uint8_t address, uint8_t dim) {
    context->address = address;

    ssd1306_command(context, DisplayCommandSetContrast);
    ssd1306_command(context, dim ? 0x00: 0xcf);

    context->length = 0;
}

// This should be called against the same DisplayContext 16 times in a row
static void display_chunk(DisplayContext *context, uint8_t data) {
    if (context->length == 0) {
        i2c_begin(context);        
        i2c_push(context, 0x40);
    }
    
    i2c_push(context, data);

    if (context->length == 17) {
        i2c_flush(context);
        i2c_begin(context);        
    }
}

static void display_chunks(DisplayContext *context, uint8_t data, uint16_t count) {
    while (count-- != 0) { display_chunk(context, data); }
}

static void display_bigChar(DisplayContext *context, uint8_t charIndex, bool topAlign, bool topHalf) {
    //uint32_t *bigChar = ;
    //getBigFont(charIndex, bigChar);
    
    for (uint8_t x = 0; x < 8; x++) {
        uint16_t col = 0;
        for (uint8_t y = 0; y < 12; y++) {
            //uint32_t c4 = bigChar[y / 4];
            uint32_t c4 = pgm_read_dword(&font_big[3 * charIndex + y / 4]);
            c4 >>= (y % 4) * 8;
            if (c4 & (1 << x)) {
                col |= (1 << y);
            }
        }
        
        if (!topAlign) { col <<= 4; }
        
        if (!topHalf) { col = (col >> 8); }

        display_chunk(context, col & 0xff);
    }
}

static void display_smallChar(DisplayContext *context, uint8_t charIndex) {
    uint32_t smallChar = pgm_read_dword(&font_small[charIndex]);
    for (int8_t c = 24; c >= 0; c -= 8) {
        display_chunk(context, (smallChar >> c) & 0xff);
    }
}

static void display_tinyChar(DisplayContext *context, uint8_t charIndex) {
    // Space
    if (charIndex == 32) {
        display_chunks(context, 0, 3);
        return;
    }

    if (charIndex == 10 || charIndex == 13) {
        // We put a NL/CR symbol in the last index
        charIndex = 127;
    } else if (charIndex < 32 || charIndex > 126) {
        // Non-printable character; solid box
        display_chunks(context, (31 << 1), 3);
        return;
    }


    // We have a glyph, so adjust to our font table
    charIndex -= 33;
    
    uint32_t tinyChar = 0;
    uint16_t startBitIndex = charIndex * 15;
    for (uint8_t i = 0; i < 3; i++) {
        tinyChar <<= 8;
        tinyChar |= pgm_read_byte(&font_tiny[(startBitIndex >> 3) + i]);
    }

    tinyChar >>= (9 - (startBitIndex % 8));
    
    for (int8_t c = 0; c < 15; c += 5) {
        uint8_t chunk = ((tinyChar >> c) << 1) & 0x3e;
        
        // Add decents on certain characters
        if (c == 0 && charIndex == ('p' - 33)) {
            chunk |= 0x40;
        } else if (c == 5 && (charIndex == ('g' - 33) || charIndex == ('j' - 33) || charIndex == ('y' - 33))) {
            chunk |= 0x40;          
        } else if (c == 10 && (charIndex == ('q' - 33) || charIndex == (127 - 33))) {
            chunk |= 0x40;          
        }
        
        display_chunk(context, chunk);
    }
}

void display_debug_int(uint8_t address, uint32_t value) {
    DisplayContext context;
    display_begin(&context, address, 0);

    uint8_t offset = 0;
    uint8_t base10[6];
    while (value) {
        base10[offset++] = value % 10;
        value /= 10;
    }

    if (offset == 0) {
        base10[0] = 0;
        offset++;
    }

    for (int8_t i = offset - 1; i >= 0; i--) {
        display_smallChar(&context, base10[i]);
        display_chunk(&context, 0);
    }

    while (context.length != 0) {
        display_chunk(&context, 0);
    }
}

void display_debug_char(uint8_t address, char chr) {
    DisplayContext context;
    display_begin(&context, address, 0);

    display_tinyChar(&context, (uint8_t)chr);
    display_chunk(&context, 0);

    while (context.length != 0) {
        display_chunk(&context, 0);
    }
}

void display_debug_text(uint8_t address, const char *text) {
    DisplayContext context;
    display_begin(&context, address, 0);

    while (1) {
        uint8_t chr = (*text++);
        if (!chr) { break; }
        display_tinyChar(&context, (uint8_t)chr);
        display_chunk(&context, 0);
    }

    while (context.length != 0) {
        display_chunk(&context, 0);
    }
}

static uint8_t getHexNibble(uint8_t value) {
    value &= 0x0f;
    if (value <= 9) { return '0' + value; }
    return 'a' + value - 10;
}

void display_debug_buffer(uint8_t address, uint8_t *value, uint16_t length) {
    DisplayContext context;
    display_begin(&context, address, 0);

    display_chunks(&context, 0, 16);

    for (int8_t i = 0; i < length; i++) {
        display_tinyChar(&context, getHexNibble(value[i] >> 4));
        display_chunk(&context, 0);
        display_tinyChar(&context, getHexNibble(value[i]));
        display_chunks(&context, 0, 4);
    }

    while (context.length != 0) {
        display_chunk(&context, 0);
    }

    display_chunks(&context, 0, 16);
}


void display_qrcode(DisplayContext *context, QRCode *qrcode, uint8_t py) {
     for (uint8_t x = 0; x < 64; x++) {
          uint8_t by = py * 8;
          uint8_t col = 0;

          for (uint8_t y = 0; y < 8; y++) {
              if (x < 3 || x >= 64 - 3 || by + y < 3 || by + y >= 64 -3 || !qrcode_getModule(qrcode, (x - 3) / 2, (by + y - 3) / 2)) {
                  col |= (1 << y);
               }
          }
          display_chunk(context, col);
     }
}

void display_qrcodes(uint8_t address, QRCode *leftOrCenter, QRCode *right) {
    DisplayContext context;
    display_begin(&context, address, 1);

    QRCode *qrcode = NULL;

    for (uint8_t py = 0; py < 8; py++) {
         if (right == NULL) {
             display_chunks(&context, 0, 32);
         }
         
         display_qrcode(&context, leftOrCenter, py);
         
         if (right == NULL) {
             display_chunks(&context, 0, 32);
         } else {
             display_qrcode(&context, right, py);
         }
    }
}

void display_hourglass(uint8_t address) {
    DisplayContext context;
    display_begin(&context, address, 0);

    display_chunks(&context, 0, 3 * 128);
    
    for (int8_t top = 1; top >= 0; top--) {
        display_chunks(&context, 0, 64 - 8 / 2);
        display_bigChar(&context, 18, true, top);
        display_chunks(&context, 0, 64 - 8 / 2);
    }
    
    display_chunks(&context, 0, 3 * 128);
}

static void display_transaction_size(DisplayContext *context, uint16_t dataSize) {
    const uint8_t width = 128 - 32;
    
    if (dataSize == 0) {
         display_chunks(context, 0, width);
         return;
    }
    
    uint8_t offset = 0;
    uint8_t base10[6];
    while (dataSize) {
        base10[offset++] = dataSize % 10;
        dataSize /= 10;
    }

    uint8_t len = (offset + 6);
    uint8_t w = (len * 5 - 1);
    uint8_t padding = (width - w) / 2;

    display_chunks(context, 0, padding);
        
    for (int8_t i = offset - 1; i >= 0; i--) {
        display_smallChar(context, base10[i]);
        display_chunk(context, 0);
    }

    display_chunks(context, 0, 5);
    
    for (uint8_t i = 10; i < 15; i++) {
        display_smallChar(context, i);
        display_chunk(context, 0);
    }

    display_chunks(context, 0, width - w - padding - 1);
}

static void display_transaction_address(DisplayContext *context, uint8_t *address) {

    // Show the transaction "to address"
    for (int8_t top = 1; top >= 0; top--) {
        display_chunks(context, 0, 4);

        display_bigChar(context, 0, true, top);
        display_chunk(context, 0);
        display_bigChar(context, 16, true, top);
        display_chunk(context, 0);
    
        for (uint8_t i = 0; i < 5; i++) {   
            uint8_t c = i;
            if (c >= 3) { c = 20 - 2 + (i - 3); }
                 
            display_bigChar(context, address[c] >> 4, true, top);
            display_chunk(context, 0);

            display_bigChar(context, address[c] & 0x0f, true, top);
            display_chunk(context, 0);
            
            if (i == 2) {
                for (uint8_t j = 0; j < 3; j++) {
                    display_chunks(context, (top ? 0: 0x0c), 2);
                    display_chunks(context, 0, (j == 2) ? 1: 2);
                }
            }
        }
    
        display_chunks(context, 0, 128 - (9 * 12) - 11 - 4);
    }
}

static void display_transaction_value(DisplayContext *context, char* value) {
    uint8_t len = strlen(value);

    // String too long; should not happen, but if the transaction is for over
    // 10 billion ether, at least show as much of the value as possible.
    // @TODO: We should throw an error and crash? If they attempt 100 billion
    // ether, we will only display 10 billion... :s
    if (len > 14) { len = 14; }

    uint8_t width = (len - 1) * 8 + 6 + (len - 1) ;
    uint8_t padding = (128 - width) / 2;

    for (int8_t top = 1; top >= 0; top--) {
        display_chunks(context, 0, padding);

        for (uint8_t i = 0; i < len; i++) {
            char c = value[i];
            if (c == '.') {
                display_chunks(context, 0, 2);
                display_chunks(context, (top ? 0: 0xc0), 2);
                display_chunks(context, 0, 2);
            } else {
                display_bigChar(context, c - '0', false, top);
            }
            display_chunks(context, 0, 1);
        }
        display_chunks(context, 0, 128 - width - padding - 1);
    }
}

static void display_transaction_ok(DisplayContext *context) {
    uint8_t len = 3;
    uint8_t width = 4 * len + (len - 1);
    uint8_t padding = (128 - width) / 2;

    display_chunks(context, 0, padding);
    
    for (uint8_t i = 15; i < 18; i++) {
        display_smallChar(context, i);
        display_chunk(context, 0);
    }
    
    display_chunks(context, 0, 128 - padding);
}

void display_transaction(uint8_t address, Transaction *transaction, char* value) {
    DisplayContext context;
    display_begin(&context, address, 0);

    // The top strip (indicates gas limit, data byte count and gas price)
    
    // @TODO Contract? High gas limit?
    display_chunks(&context, 0, 16);

    // Data size (e.g. "354 bytes") 
    display_transaction_size(&context, transaction->dataLength);
   
    // @TODO Gas price is low or high?
    display_chunks(&context, 0, 16);

    // Gap
    display_chunks(&context, 0, 128);

    // Transaction to address
    display_transaction_address(&context, transaction->address);

    // Transaction value
    display_transaction_value(&context, value);

    // Gap
    display_chunks(&context, 0, 128);

    // "ok?"
    display_transaction_ok(&context);
}

uint16_t display_text(DisplayContext *context, const char *message) {

    uint16_t count = 0;
    while (*message) {
        display_tinyChar(context, *message++);
        display_chunk(context, 0);
        count += 4;
    }

    while (context->length != 0) {
        display_chunk(context, 0);
        count++;
    }

    return count;
}

void display_message(uint8_t address, bool binary, const uint8_t *message, uint8_t length) {
    DisplayContext context;
    display_begin(&context, address, 0);

    // The top strip (indicates "SIGN", data byte count and binary/ascii)
    
    // "SIGN" in the top left
    display_text(&context, (const char*)"SIGN");

    // Data size (e.g. "354 bytes") 
    display_transaction_size(&context, length);

    if (binary) {
        // "HEX" in the top left
        display_text(&context, (const char*)" HEX");

    } else {
        // "Text" in the top left
        display_text(&context, (const char*)"Text");
    }

    // Gap
    display_chunks(&context, 0, 128);

    uint16_t count = 0;
    if (binary) {
        uint8_t index = 0;
        while (1) {
            uint8_t c = (uint8_t)(message[index++]);
            if (!c) { break; }
            display_tinyChar(&context, getHexNibble(c >> 4));
            display_chunk(&context, 0);
            display_tinyChar(&context, getHexNibble(c));
            count += 7;
            if (index % 12) {
                display_chunks(&context, 0, 4);
                count += 4;
            }
        }
    } else {
        count = display_text(&context, (const char*)message);
    }
    
    display_chunks(&context, 0, (4 * 128) - count);

    // Gap
    display_chunks(&context, 0, 128);

    // "ok?"
    display_transaction_ok(&context);
}

void display_init(uint8_t address) {
    DisplayContext context;
    context.address = address;

    i2c_init(&context);

    // Initialization sequence
    ssd1306_command(&context, DisplayCommandOff);

    // Horizontal drawing mode
    ssd1306_command(&context, DisplayCommandMemoryAddressMode);
    ssd1306_command(&context, DisplayCommandMemoryAddressModeHorizontal);

    // Draw from left-to-right
    ssd1306_command(&context, DisplayCommandSegmentRemap);

    // Draw top-to-bottom
    ssd1306_command(&context, DisplayCommandCOMScanDecrement);

    // Setting the follow provide slightly better brightness range

    // Set VCOMH Deselect Level
    // 0x00 => 0.65 * Vcc
    // 0x20 => 0.77 * Vcc (RESET)
    // 0x30 => 0.83 * Vcc
    ssd1306_command(&context, DisplayCommandSetVCOMHDeselectLevel);
    ssd1306_command(&context, 0x00);

    // Set Pre-charge Period (Reset: 0x22)
    // A[3:0] => Phase 1 period of up to 15 DCLK clocks 0 is invalid entry
    // A[7:4] => Phase 2 period of up to 15 DCLK clocks 0 is invalid entry
    ssd1306_command(&context, DisplayCommandSetPrechargePeriod);
    ssd1306_command(&context, 0xF1);

    // Enable the Charge-Pump
    ssd1306_command(&context, DisplayCommandSetChargePump);
    ssd1306_command(&context, DisplayCommandSetChargePumpEnabled);

    // Turn on the screen
    ssd1306_command(&context, DisplayCommandOn);

    // Make sure we clear any invert
    ssd1306_command(&context, DisplayCommandSetNormal);

    delay(5);
    
    display_clear(address);
}

void display_invert(uint8_t address, bool invert) {
    DisplayContext context;
    display_begin(&context, address, 0);
    
    ssd1306_command(&context, invert ? DisplayCommandSetInvert: DisplayCommandSetNormal);
}


void display_clear(uint8_t address) {
    DisplayContext context;
    context.address = address;

    ssd1306_command(&context, DisplayCommandSetColumnAddress);
    ssd1306_command(&context, 0);
    ssd1306_command(&context, 127);
    
    ssd1306_command(&context, DisplayCommandSetPageAddress);
    ssd1306_command(&context, 0);
    ssd1306_command(&context, 7);
    
    display_chunks(&context, 0, 128 * 8);
}

