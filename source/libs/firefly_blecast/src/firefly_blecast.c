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

// nRF24L01
// See: http://www.nordicsemi.com/eng/nordic/download_resource/8041/1/46023864/2730

// SPI
// See: http://www.avrbeginners.net/architecture/spi/spi.html


#include "firefly_blecast.h"

#include "aes.h"


// We use this to pack enums into uint8_t
// https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#Common-Type-Attributes
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


enum RadioRegister {
    RadioRegisterConfig                    = 0x00,
    RadioRegisterAutoAck                   = 0x01,
    RadioRegisterEnabledReceivePipes       = 0x02,
    RadioRegisterAddressWidth              = 0x03,
    RadioRegisterRetries                   = 0x04,
    RadioRegisterChannel                   = 0x05,
    RadioRegisterRadioConfig               = 0x06,
    RadioRegisterStatus                    = 0x07,

    // Receive Pipes
    RadioRegisterReceiveAddressPipe0       = 0x0a,
    RadioRegisterReceiveAddressPipe1       = 0x0b,
    RadioRegisterReceivePayloadWidthPipe0  = 0x11,
    RadioRegisterReceivePayloadWidthPipe1  = 0x12,

    RadioRegisterFIFOStatus                = 0x17,
} attribute(packed);
typedef enum RadioRegister RadioRegister;


enum RadioCommand {
    RadioCommandReadRegister       = 0x00,
    RadioCommandWriteRegister      = 0x20,
    RadioCommandReadPayload        = 0x61,
    RadioCommandFlushReceive       = 0xe2,
} attribute(packed);
typedef enum RadioCommand RadioCommand;


enum SPIControl {
    SPIControlEnableInterrupt          = (1 << 7),
    SPIControlEnabled                  = (1 << 6),
    SPIControlOrderLSBFirst            = (1 << 5),
    SPIControlMaster                   = (1 << 4),

    SPIControlClockPolarityIdleHigh    = (1 << 3),
    SPIControlAlphaSampleTrailing      = (1 << 2),

    // Modes (combines Clock Polarity and Clock Alpha)
    SPIControlMode0                    = (0 << 2),
    SPIControlMode1                    = (1 << 2),
    SPIControlMode2                    = (2 << 2),
    SPIControlMode3                    = (3 << 2),

    // Clock Rate Select
    SPIControlClockRateFosc4           = (0 << 0),
    SPIControlClockRateFosc16          = (1 << 0),
    SPIControlClockRateFosc64          = (2 << 0),
    SPIControlClockRateFosc128         = (3 << 0),
} attribute(packed);
typedef enum SPIControl SPIControl;


enum SPIStatus {
    SPIStatusInterruptFlag             = (1 << 7),
    SPIStatusWriteCollision            = (1 << 6),
    SPIStatusDoubleSpeed               = (1 << 0),
} attribute(packed);
typedef enum SPIStatus SPIStatus;

STATIC_ASSERT( sizeof ( RadioRegister ) == 1, "RadioRegister is incorrect width");
STATIC_ASSERT( sizeof ( RadioCommand ) == 1, "RadioCommand is incorrect width");
STATIC_ASSERT( sizeof ( SPIControl ) == 1, "SPIControl is incorrect width");
STATIC_ASSERT( sizeof ( SPIStatus ) == 1, "SPIStatus is incorrect width");


const uint8_t RadioRegisterMask = 0x1f;


// The size required to read a BLECast packet (1 byte PDU type and 16 bytes address)
#define BLECAST_PACKET_SIZE    (1 + 16)

// The size of a BLE packet
#define BLE_PACKET_SIZE        (32)


#define NEXT_CHANNEL      (0x7f)

#define RADIO_SPEED       (10000000)



/**
 *  Cyclic Redundancy Check - 24 bit (CRC24)
 *
 *  See: http://sunsite.icm.edu.pl/gnupg/rfc2440-6.html
 */

#define CRC24_INIT      0xb704ce
#define CRC24_POLY      0x1864cfb

static uint32_t computeCrc24(uint8_t *data, uint16_t length) {
    uint32_t crc = CRC24_INIT;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= ((uint32_t)data[i]) << 16;

        for (uint8_t j = 0; j < 8; j++) {
            crc <<= 1;
            if (crc & 0x1000000) {
                crc ^= CRC24_POLY;
            }
        }
    }

    return crc & 0xffffff;
}

// Reverse the bits in a byte (BLE bytes are backward)
static uint8_t reverse(uint8_t v) {

    uint8_t result = 0;

    if (v & (1 << 0)) { result |= (1 << 7); }
    if (v & (1 << 1)) { result |= (1 << 6); }
    if (v & (1 << 2)) { result |= (1 << 5); }
    if (v & (1 << 3)) { result |= (1 << 4); }
    if (v & (1 << 4)) { result |= (1 << 3); }
    if (v & (1 << 5)) { result |= (1 << 2); }
    if (v & (1 << 6)) { result |= (1 << 1); }
    if (v & (1 << 7)) { result |= (1 << 0); }

    return result;
}

// Returns ((clockRateFosc << 1) | SPIStatusDoubleSpeed)
static uint8_t spi_getClockRate(uint32_t clock) {
    if (clock > F_CPU / 2) {
        return (SPIControlClockRateFosc4 << 1) | SPIStatusDoubleSpeed;
    } else if (clock > F_CPU / 4) {
        return (SPIControlClockRateFosc4 << 1);
    } else if (clock > F_CPU / 8) {
        return (SPIControlClockRateFosc16 << 1) | SPIStatusDoubleSpeed;
    } else if (clock > F_CPU / 16) {
        return (SPIControlClockRateFosc16 << 1);
    } else if (clock > F_CPU / 32) {
        return (SPIControlClockRateFosc64 << 1) | SPIStatusDoubleSpeed;
    } else if (clock > F_CPU / 64) {
        return (SPIControlClockRateFosc64 << 1);
    }
    return (SPIControlClockRateFosc128 << 1);
}


static void spi_init(BLECastMessage *message) {

    // Slave Select should be set as an output to prevent slave mode
    // See: https://www.arduino.cc/en/Reference/SPI
    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);

    // Get the clock rate for the SPI Control Register
    uint8_t spcr = spi_getClockRate(RADIO_SPEED);

    // Set the Double Speed in the SPI Status Register
    uint8_t spsr = spcr & SPIStatusDoubleSpeed;

    // Strip off the SPIStatusDoubleSpeed
    spcr >>= 1;
    spcr |= SPIControlEnabled | SPIControlMaster | SPIControlMode0;

    // Set the SPI Control Register (enabled, master, mode 0, CLOCK_RATE)
    SPCR = spcr;

    // Set the SPI Status Register (possibly SPI double speed)
    SPSR = spsr;

    // SPI Clock and SPI Master-Out-Slave-In should be outputs
    pinMode(SCK, OUTPUT);
    pinMode(MOSI, OUTPUT);
}

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    //asm volatile("nop"); // @TODO: The official SPI lib does this; do some benchmarks
    while (!(SPSR & SPIStatusInterruptFlag));
    return SPDR;
}

static void spi_shutdown() {
    SPCR &= ~SPIControlEnabled;
}

static void radio_ce(BLECastMessage *message, uint8_t enable) {
    digitalWrite(message->radioPinCE, enable ? HIGH: LOW);
}

static void radio_csn(BLECastMessage *message, uint8_t enable) {
    digitalWrite(message->radioPinCSN, enable ? HIGH: LOW);
    delayMicroseconds(5);
}

static void radio_beginTransaction(BLECastMessage *message) {
    radio_csn(message, 0);
}

static void radio_endTransaction(BLECastMessage *message) {
    radio_csn(message, 1);
}

static void radio_writeRegister(BLECastMessage *message, RadioRegister reg, const uint8_t *buffer, uint8_t lengthOrValue) {
    radio_beginTransaction(message);

    uint8_t status = spi_transfer(RadioCommandWriteRegister | (RadioRegisterMask & reg));

    if (buffer == NULL) {
        spi_transfer(lengthOrValue);
    } else {
        while(lengthOrValue--) {
            spi_transfer(*buffer++);
        }
    }

    radio_endTransaction(message);
}

static uint8_t radio_readRegister(BLECastMessage *message, RadioRegister reg, uint8_t *buffer, uint8_t length) {
    radio_beginTransaction(message);

    uint8_t status = spi_transfer(RadioCommandReadRegister | (RadioRegisterMask & reg));

    if (buffer == NULL) {
        status = spi_transfer(0xff);
    } else {
        while(length--) {
            *buffer++ = spi_transfer(0xff);
        }
    }

    radio_endTransaction(message);

    return status;
}

static uint8_t radio_getChannel(BLECastMessage *message) {
    switch (radio_readRegister(message, RadioRegisterChannel, NULL, 0)) {
        case 26: return 1;
        case 80: return 2;
        default: break;
    }

    return 0;
}

static void radio_setChannel(BLECastMessage *message, uint8_t channel) {

    if (channel == NEXT_CHANNEL) {
        channel = radio_getChannel(message) + 1; //getFrequencyIndex(radio_getChannel(message)) + 1;
    }

    if (channel > 2) { channel = 0; }

    // Advertising frequencies (the nRF2401 adds 2400 to these values)
    //static const uint8_t frequencies[] = { 2, 26, 80 };
    //uin8_t frequency = 2 + channel * 24 + (channel & 0x02) * 15;
    uint8_t frequency = 2;
    switch(channel) {
        case 2: frequency += 54; // Falls through!
        case 1: frequency += 24;
    }

    radio_writeRegister(message, RadioRegisterChannel, NULL, frequency);
}

static void radio_init(BLECastMessage *message) {

    pinMode(message->radioPinCE, OUTPUT);
    pinMode(message->radioPinCSN, OUTPUT);

    spi_init(message);

    radio_ce(message, 0);
    radio_csn(message, 1);

    delay(5);

    // CONFIG
    // Configuration Register
    radio_writeRegister(message, RadioRegisterConfig, NULL, 0x00);

    // EN_AA
    // Enable "Auto Acknowledgement" (disable for nRF24L01 compatibility)
    radio_writeRegister(message, RadioRegisterAutoAck, NULL, 0x00);

    // SETUP_RETR
    // Setup of Automatic Retransmission
    radio_writeRegister(message, RadioRegisterRetries, NULL, 0x00);

    // RF_SETUP
    // [3]   => 0b = 1Mbps, 1b = 2Mbps
    // [0]   => Low Noise Amplifier (@TODO: should we use this to save power?)
    radio_writeRegister(message, RadioRegisterRadioConfig, NULL, 0x00);

    // Reset the RX buffer
    radio_writeRegister(message, RadioRegisterStatus, NULL, 0x70);

    // Set the channel
    radio_setChannel(message, 0);

    // Flush the RX buffer
    radio_beginTransaction(message);
    spi_transfer(RadioCommandFlushReceive);
    radio_endTransaction(message);

    // CONFIG
    // Configuration Register
    // 0x02 => Power up
    // 0x01 => Receive Mode
    radio_writeRegister(message, RadioRegisterConfig, NULL, 0x03);

    // Let things settle
    delay(5);

    // SETUP_AW
    // Setup of Address Widths (common for all data pipes)
    // 0x01 => 3 bytes
    // 0x02 => 4 bytes
    // 0x03 => 5 bytes
    radio_writeRegister(message, RadioRegisterAddressWidth, NULL, 0x02);

    // Access Address; see Core Specification B.2.1.2 (reverseBits(0x8E89BED6))

    // RX_ADDR_P1
    // Receive address data pipe 1 (LSB first)
    uint8_t address[] = { 0x71, 0x91, 0x7D, 0x6b };
    radio_writeRegister(message, RadioRegisterReceiveAddressPipe1, address, 4);
    radio_writeRegister(message, RadioRegisterReceivePayloadWidthPipe1, NULL, BLE_PACKET_SIZE);

    // EN_RXADDR
    // Enabled RX Addresses
    // 0x01 => Pipe 0
    // 0x02 => Pipe 1
    radio_writeRegister(message, RadioRegisterEnabledReceivePipes, NULL, 0x02);

    delay(5);
}

static void radio_shutdown(BLECastMessage *message) {
    radio_ce(message, 0);
    radio_writeRegister(message, RadioRegisterConfig, NULL, 0x01);
    spi_shutdown();
}

static void radio_startListening(BLECastMessage *message) {
    radio_writeRegister(message, RadioRegisterConfig, NULL, 0x03); /// @TODO: needed?

    // STATUS
    // Status Register
    // 0x40 => Receive Data Ready (write 1 to clear)
    // 0x20 => Sent Data Complete (write 1 to clear) // @TODO: needed?
    // 0x10 => Max retransmits (write 1 to clear; must clear to continue) // @TODO: needed?
    radio_writeRegister(message, RadioRegisterStatus, NULL, 0x70);
    radio_ce(message, 1);

    delay(10);
}

static void radio_stopListening(BLECastMessage *message) {
    radio_ce(message, 0);
    delay(5);
    radio_writeRegister(message, RadioRegisterConfig, NULL, 0x0);
}

static uint8_t radio_available(BLECastMessage *message) {
    // FIFO_STATUS
    // FIFO Status Register
    // 0x02 => RX full
    // 0x01 => RX empty
    return !(radio_readRegister(message, RadioRegisterFIFOStatus, NULL, 0) & 0x01);
}

// Reads 17 bytes; 1 byte PDU type and the 16 byte BLE address (where BLECast transmits data)
static void radio_read_packet(BLECastMessage *message, uint8_t *buffer) {
    radio_beginTransaction(message);

    uint8_t status = spi_transfer(RadioCommandReadPayload);

    // Read the first byte (for the BLE PDU_TYPE)
    *buffer++ = spi_transfer(0xff);

    // Skip 12 bytes (for BLECast stuff we do not need this BLE metadata)
    for (uint8_t i = 0; i < 12; i++) {
        spi_transfer(0xff);
    }

    // The 16 bytes of BLECast data (BLE address)
    for (uint8_t i = 0; i < 16; i++) {
        *buffer++ = spi_transfer(0xff);
    }

    // Finish off the 32 byte of BLE_PACKET_SIZE (3 bytes remain)
    for (uint8_t i = 0; i < 3; i++) {
        spi_transfer(0xff);
    }

    radio_endTransaction(message);

    // Clear the data so we can get more
    radio_writeRegister(message, RadioRegisterStatus, NULL, 0x70);
}


static void _blecast_init(BLECastMessage *message) {
    message->totalPayloadCount = -1;
    message->discoveredPayloadCount = 0;
    message->size = -1;
    message->id = BLECAST_INVALID_ID;
    memset(message->data, 0, message->maxSize);
}


bool blecast_init(BLECastMessage *message, uint8_t *key, uint8_t *data, uint16_t dataLength) {
    message->data = data;
    message->maxSize = dataLength;

    _blecast_init(message);

    memcpy(message->aesKey, key, 16);

    radio_init(message);
}

void blecast_shutdown(BLECastMessage *message) {
    radio_shutdown(message);
}

void blecast_reset(BLECastMessage *message) {
    _blecast_init(message);
}


void blecast_dump(BLECastMessage *message) {
/*
    Serial.print("Message count=");
    Serial.print(message->totalPayloadCount);
    Serial.print(", discovered=");
    Serial.print(message->discoveredPayloadCount);
    Serial.print(", size=");
    Serial.print(message->size);
    Serial.print("\n");
    for (uint8_t i = 0; i < (message->maxSize / 13); i++) {
        uint8_t *data = &(message->data[i * 13]);
        if (data[0] == 0) { continue; }
        Serial.print("  Payload ");
        Serial.print(i);
        Serial.print(": ");
        for (uint8_t i = 1; i < 13; i++) {
            if (data[i] < 0x10) { Serial.print("0"); }
            Serial.print((uint8_t)(data[i]), HEX); Serial.print(" ");
        }
        Serial.println("");
    }
    */
}



bool blecast_addPayload(BLECastMessage *message, uint8_t *data) {
    // Already done
    if (message->size >= 0) { return false; }

    // Decrypt the payload
    aes128_otfks_decrypt_start_key(message->aesKey);
    aes128_otfks_decrypt(data, message->aesKey);

    // This is the CRC to match
    uint32_t payloadCrc = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];

    // De-noise the payload data
    for (uint32_t i = 3; i < 16; i++) {
        data[i] ^= (payloadCrc >> (uint32_t)(i - 3));
    }

    // Compute the CRC (while removing the noise applied during shrink-wrapping)
    uint32_t computedPayloadCrc = computeCrc24(&data[3], 13);

    // Check the CRC (either not for us of data transmission error)
    if (payloadCrc != computedPayloadCrc) { return false; }

    // Done with the CRC; strip it
    data += 3;

    // The index byte; [terminal1] [partial1] [index6]
    uint8_t index = data[0];
    data++;

    // This message is too big! Reset and hope things are better in the future
    uint8_t blockIndex = (index & 0x3f);
    if (blockIndex * 13 > message->maxSize) {
        _blecast_init(message);
        return false;
    }

    // Already have this block
    if (message->data[blockIndex * 13]) {
        return false;
    }

    // Add the data (mark it as found and keep the partial and final bits)
    message->data[blockIndex * 13] = index | 0x01;
    memcpy(&message->data[blockIndex * 13 + 1], data, 12);

    message->discoveredPayloadCount++;

    // Last payload for this message
    if ((index & 0x80) && message->totalPayloadCount == -1) {
        message->totalPayloadCount = blockIndex + 1;
    }

    // This can happen when switching between messages; stray payloads
    // got picked up from a previous message without the terminal.
    if (message->totalPayloadCount != -1 && message->totalPayloadCount < message->discoveredPayloadCount) {
        _blecast_init(message);
        return false;
    }


    // Message complete!
    if (message->totalPayloadCount == message->discoveredPayloadCount) {

        // Conpute size

        // A partial payload (the length is in the last payload content slot)
        uint8_t lastIndex = 13 * (message->totalPayloadCount - 1);
        if (message->data[lastIndex] & 0x40) {
            message->size = (message->totalPayloadCount - 1) * 12 + message->data[lastIndex + 12];

        } else {
            message->size = message->totalPayloadCount * 12;
        }

        // If the message is more than 1 block, if contains an additional message CRC prefix
        //uint8_t *outputPtr = output;
        if (message->size > 12) {

            uint32_t messageCrc = ((uint32_t)(message->data[1]) << 16) | ((uint32_t)(message->data[2]) << 8) | (message->data[3]);

            for (uint8_t i = 0; i < message->totalPayloadCount; i++) {
                uint8_t targetOffset = i * 12, sourceOffset = 1 + i * 13;
                for (uint8_t j = 0; j < 12; j++) {
                    message->data[targetOffset + j] = message->data[sourceOffset + j];
                }
            }

            message->size -= 3;

            for (uint8_t i = 0; i < message->size; i++) {
                message->data[i] = message->data[i + 3];
            }

            uint32_t computedMessageCrc = computeCrc24(message->data, message->size);

            if (computedMessageCrc != messageCrc) {
                _blecast_init(message);
                return false;
            }

            message->id = messageCrc;

        } else {
            for (uint8_t i = 0; i < 12; i++) {
                message->data[i] = message->data[i + 1];
            }

            message->id = payloadCrc;
        }

        blecast_dump(message);

        return true;
    }

    return false;
}

// Pre-computed whiten mask for each channel (byte[0] and byte[13:13 + 16]
const uint8_t whitenMask[] PROGMEM = {
//const uint8_t whitenMask[] = {
    // Channel 37
    0x8d, 0x77, 0xf8, 0xe3, 0x46, 0xe9, 0xab, 0xd0, 0x9e,
    0x53, 0x33, 0xd8, 0xba, 0x98, 0x08, 0x24, 0xcb,

    // Channel 38
    0xd6, 0x4e, 0xcd, 0x60, 0xeb, 0x62, 0x22, 0x90, 0x2c,
    0xef, 0xf0, 0xc7, 0x8d, 0xd2, 0x57, 0xa1, 0x3d,

    // Channel 39
    0x1f, 0x59, 0xde, 0xe1, 0x8f, 0x1b, 0xa5, 0xaf, 0x42,
    0x7b, 0x4e, 0xcd, 0x60, 0xeb, 0x62, 0x22, 0x90
};


bool blecast_poll(BLECastMessage *message) {

    // Already done this message
    if (message->size >= 0) { return false; }

    bool success = false;

    radio_startListening(message);

    uint8_t buffer[BLECAST_PACKET_SIZE];

    while (radio_available(message)) {
        radio_read_packet(message, buffer);

        uint8_t *data = buffer;
        //uint8_t whitenIndex = whitenMask + getFrequencyIndex(radio_getChannel(message) - 37) * 17;
        uint8_t whitenIndex = whitenMask + radio_getChannel(message) * 17;
        //uint8_t *whiten = whitenMask[getFrequencyIndex(message->radio->getChannel() - 37) * 17];

        uint8_t b = *data;
        // @TODO: Pre-compute this and just do a comparison instead of the below 0x40
        b = reverse(b);
        b ^= pgm_read_byte(whitenIndex++);
        //b ^= *(whiten++);

        // PDU type must be ADV_NONCONN_IND (E.7.7.65.13)
        if (b != 0x40) { continue; }

        data += 1;
        for (uint8_t index = 0; index < 16; index++) {
            uint8_t b = *data;

            // Reverse the bits
            // https://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits
            b = reverse(b);

            // De-whiten
            b ^= pgm_read_byte(whitenIndex++);
            //b ^= *(whiten++);

            *(data++) = b;
        }

        bool complete = blecast_addPayload(message, &buffer[1]);
        if (complete) { success = true; }
    }

    radio_stopListening(message);

    radio_setChannel(message, NEXT_CHANNEL);

    return success;
}

