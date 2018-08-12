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
 *  High-Level Information
 *  
 *  Scope Using Functions
 *  
 *  We move a lot of memory allocation-heavy operations into functions to ensure
 *  the compiler correctly generates code that cleans up after itself and releaeses
 *  memory once is it no longer needed. We need every ounce of memory and storage
 *  we can get our hands on.
 *  
 *  
 *  Malloc vs stack
 *  
 *  Each instance of malloc/free vs using stack space has been carefully examined
 *  as assembly and comparing generated code. In some instances, the code is inlined
 *  resulting in smaller code (odd, but poke around in the obj-dump to see weird
 *  reasons why, such as how smallish (less than the number of registers) arrays get
 *  passed in as individual parameters.
 *  
 *  I suspect this will change over time, and once a newer version of the compiler
 *  is used, with better -Os support, we can move more of these to use the stack.
 *  
 *  
 *  Available Trade-offs
 *  
 *  As we add more features, we may need to slim down the Storage or SRAM, depending on
 *  what we are low on. Some features can trade one for the other and some features we
 *  may be able to cripple in subtle ways to gain the space we need. Here is a list of
 *  current options:
 *  
 *  - The QR code may have penalty scores removed and always choose mask 0; less
 *    standards compliant but gains 1318 bytes of storage.
 *  - Use uECC Optimiation level 1 (instead of 2); increases signing from 6s to 7.2s but
 *    gains 2168 bytes of storage.
 */



/**
 *  Library Includes
 *  
 *  Each of these includes are in the ../libs/ folder and should be added to your
 *  Arduino `libraries` folder via a symlink.
 *  
 *  e.g. ln -s /home/ricmoo/firefly/wallet/source/libs/ethers /home/ricmoo/Arduino/libraries/ethers
 */


/**
 *  Private Key
 *  
 *  KEEP THIS SECRET!
 *  
 *  Anyone with this private key can steal your ether and any other crypto-assets associated with it.
 *  
 *  This is hard-coded, unencrypted into the version 0 (v0) protocol, but in the future versions
 *  of this software will have much more clever ways to create and store your private key.
 *  
 *  To generate a new random private key, in the terminal type:
 * 
 *  /home/ethers> python -c 'import os; print ", ".join([ str(ord(c)) for c in bytes(os.urandom(32)) ])'
 *  
 */

static const uint8_t privateKey[] = {
#error You MUST generate a new private key and add it here, then remove this error line
};


// For debugging, we want to enable Serial dumping, but in the final product
// removing the Serial printing squeaks in a bit more room for larger transactions
// received over BLECast.
#define DEBUG_SERIAL  1


// The Ethereum library (signing, parsing transactions and cryptographic hashes)
#include <ethers.h>

// A customied version of my QR code library, to improve memory, flash and storage space as needed
#include <firefly_qrcode.h>

// A customized version of my BLECast library, to improve memory, flash and storage space as needed
// In the future this will also be used for its AES implementation for encrypted private keys
#include <firefly_blecast.h>

// Our custom zero-memory display driver
#include "firefly_display.h"

typedef enum ErrorCode {
    ErrorCodeNone           = 0,
    ErrorCodeOutOfMemory    = 31,
    ErrorCodeSigningError   = 41,
    ErrorCodeInvalidKey     = 42,
    ErrorBadTransaction     = 43,
    ErrorCodeStorageFailed  = 51,
} ErrorCode;


// The keccak256(privateKey)
#define EEPROM_DATA_OFFSET_CHECKSUM            (0)
#define EEPROM_DATA_LENGTH_CHECKSUM            (32)

// The secret key (symmetric) used for phone to send encrypted messages
#define EEPROM_DATA_OFFSET_PAIR_SECRET         (EEPROM_DATA_OFFSET_CHECKSUM + EEPROM_DATA_LENGTH_CHECKSUM)
#define EEPROM_DATA_LENGTH_PAIR_SECRET         (16) 

// The address of the private key
#define EEPROM_DATA_OFFSET_ADDRESS             (EEPROM_DATA_OFFSET_PAIR_SECRET + EEPROM_DATA_LENGTH_PAIR_SECRET)
#define EEPROM_DATA_LENGTH_ADDRESS             (20)

// The wallet URI (checksummed address with "ethereum:" scheme)
#define EEPROM_DATA_OFFSET_ADDRESS_URI         (EEPROM_DATA_OFFSET_ADDRESS + EEPROM_DATA_LENGTH_ADDRESS)
#define EEPROM_DATA_LENGTH_ADDRESS_URI         (9 + ETHERS_CHECKSUM_ADDRESS_LENGTH)   


extern unsigned int __heap_start;
extern void *__brkval;

/*
 * The free list structure as maintained by the 
 * avr-libc memory allocation routines.
 */
struct __freelist {
  size_t sz;
  struct __freelist *nx;
};

/* The head of the free list structure */
extern struct __freelist *__flp;


/* Calculates the size of the free list */
int freeListSize() {
  struct __freelist* current;
  int total = 0;
  for (current = __flp; current; current = current->nx) {
    total += 2; /* Add two bytes for the memory block's header  */
    total += (int) current->sz;
  }
  return total;
}

int freeMemory() {
  int free_memory;
  if ((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__heap_start);
  } else {
    free_memory = ((int)&free_memory) - ((int)__brkval);
    free_memory += freeListSize();
  }
  return free_memory;
}


/**
 *  Our Hardware Configuration
 */
 
#define BUTTON_PIN            (3)

// ATmega 328
#define RADIO_PIN_CE          (9)
#define RADIO_PIN_CSN         (10)

// ATmega 2560
//#define RADIO_PIN_CE          (40)
//#define RADIO_PIN_CSN         (53)

#define DISPLAY_ADDRESS       (0x3c)


// Dumps a buffer
static void dumpBuffer(uint8_t *buffer, uint16_t length) {
#if DEBUG_SERIAL == 1
    //Serial.print("0x");
    for(uint16_t i = 0; i < length; i++) {
        // Prefix single nibble with a "0"
        if (buffer[i] < 0x10) { Serial.print("0"); }
        Serial.print((uint8_t)(buffer[i]), HEX);
        Serial.print(' ');
    }
    Serial.println("");
#endif
}

static void waitForButton() {
    // Wait for the button down
    while (digitalRead(BUTTON_PIN) == 1) { delay(50); }
    
    // De-bounce
    delay(50);
    
    // wait for the button up
    while (digitalRead(BUTTON_PIN) == 0) { delay(50); }
}

void dumpFreeMemory() {
#if DEBUG_SERIAL == 1
    display_debug_char(DISPLAY_ADDRESS, 'M');
    display_debug_int(DISPLAY_ADDRESS, freeMemory());
    display_debug_char(DISPLAY_ADDRESS, 'H');
    display_debug_int(DISPLAY_ADDRESS, (uint32_t)(&__heap_start));
    display_debug_char(DISPLAY_ADDRESS, 'B');
    display_debug_int(DISPLAY_ADDRESS, (uint32_t)(&__brkval));
    waitForButton();
    display_clear(DISPLAY_ADDRESS);
#endif
}

static char getHexNibble(uint8_t value) {
    value &= 0x0f;
    if (value <= 9) { return '0' + value; }
    return 'A' + (value - 10);
}

// If something goes wrong, indicate the error code and halt
static void crash(ErrorCode errorCode, uint16_t lineNo) {
    if (DEBUG_SERIAL) {
        Serial.println("ERROR");
        Serial.println(errorCode);
        Serial.println(lineNo);
    }
    if (errorCode != ErrorCodeNone) {
        display_invert(DISPLAY_ADDRESS, true);
        display_clear(DISPLAY_ADDRESS);
    
        display_debug_text(DISPLAY_ADDRESS, (const char*)"ERR");
        display_debug_int(DISPLAY_ADDRESS, errorCode);
        
        display_debug_char(DISPLAY_ADDRESS, 'L');
        display_debug_int(DISPLAY_ADDRESS, lineNo);
    }    
    
    while (1) { delay(0x7fffffff); }
}

const char addressURIPrefix[] PROGMEM = {
    'e', 't', 'h', 'e', 'r', 'e', 'u', 'm', ':'
};

const char messageHeaderPrefix[] PROGMEM = {
    0x19, 'E', 't', 'h', 'e', 'r', 'e', 'u', 'm', ' ', 'S', 'i', 'g', 'n', 'e', 'd', ' ', 'M', 'e', 's', 's', 'a', 'g', 'e', ':', '\n'
};

static bool equalsStorage(uint16_t offset, uint16_t length, uint8_t *buffer) {
    for (uint16_t i = 0; i < length; i++) {
        if (buffer[i] != eeprom_read_byte((uint8_t*)(offset + i))) {
            return false;
        }
    }
    return true;
}

static void readStorage(uint16_t offset, uint16_t length, uint8_t *buffer) {
    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = eeprom_read_byte((uint8_t*)(offset + i));
    }
}

static void writeStorage(uint16_t offset, uint16_t length, uint8_t *buffer) {
    for (uint16_t i = 0; i < length; i++) {
        eeprom_write_byte((uint8_t*)(offset + i), buffer[i]);
    }
    
    delay(100);
    
    // Make sure the data was written correctly
    if (!equalsStorage(offset, length, buffer)) {
        crash(ErrorCodeStorageFailed, __LINE__);
    }
}

static void generateCache() {
    uint8_t checksum[32];
    ethers_keccak256(privateKey, 32, checksum);
    
    // Already generated all the cached data (note: this is written last like a journal commit)
    if (equalsStorage(EEPROM_DATA_OFFSET_CHECKSUM, EEPROM_DATA_LENGTH_CHECKSUM, checksum)) {
        return;
    }

     // The largest amount of memory we need for anything we cache
    uint8_t scratch[9 + ETHERS_CHECKSUM_ADDRESS_LENGTH];

    // *********
    // Compute the address (leave some space at the beginning for the URI scheme in the next part)
    bool success = ethers_privateKeyToAddress(privateKey, &scratch[9]);
    if (!success) { crash(ErrorCodeInvalidKey, __LINE__); }
    writeStorage(EEPROM_DATA_OFFSET_ADDRESS, EEPROM_DATA_LENGTH_ADDRESS, &scratch[9]);

    // *********
    // Generate the wallet URI ("ethereum:" + checksumAddress);
    memcpy_P(scratch, addressURIPrefix, 9);

    // Place the null-terminated, checksum address into the string after the "ethereum:"
    ethers_addressToChecksumAddress(&scratch[9], (char*)&scratch[9]);

    writeStorage(EEPROM_DATA_OFFSET_ADDRESS_URI, EEPROM_DATA_LENGTH_ADDRESS_URI, scratch);

    // *********
    // Compute the pairing secret keccak(0x00 || keccak(privateKey))[:16]
    ethers_keccak256(checksum, 32, &scratch[33]);
    scratch[32] = 0;
    ethers_keccak256(&scratch[32], 33, scratch);
    
    writeStorage(EEPROM_DATA_OFFSET_PAIR_SECRET, EEPROM_DATA_LENGTH_PAIR_SECRET, scratch);

    // *********
    // Compute the pairing secret keccak(0x00 || keccak(privateKey))[:16]
    writeStorage(EEPROM_DATA_OFFSET_CHECKSUM, EEPROM_DATA_LENGTH_CHECKSUM, checksum);
}

static void showAddress() {

    // We use this scratch space for (QR Code module data) ("ethereum:" + checkSumAddress)
    uint8_t qrCodeBufferSize = qrcode_getBufferSize(3);
    uint8_t *scratch = (uint8_t*)malloc(qrCodeBufferSize + (9 + ETHERS_CHECKSUM_ADDRESS_LENGTH));

    // Load the cached URI from EEPROM
    readStorage(EEPROM_DATA_OFFSET_ADDRESS_URI, EEPROM_DATA_LENGTH_ADDRESS_URI, &scratch[qrCodeBufferSize]);

    QRCode qrcode;
    qrcode_initText(&qrcode, &scratch[0], 3, 0, (char*)(&scratch[qrCodeBufferSize]));
    display_qrcodes(DISPLAY_ADDRESS, &qrcode, NULL);

    free(scratch);
}

static void expandHexNibbles(uint8_t *buffer, uint8_t length) {
    for (int8_t i = length - 1; i >= 0; i--) {
        buffer[2 * i + 1] = getHexNibble(buffer[i]);
        buffer[2 * i + 0] = getHexNibble(buffer[i] >> 4);
    }
}

static void showPairingScreen() {
    // We use this scratch space for (QR Code Address) (temporary space to generate string)
    uint8_t qrCodeBufferSize = qrcode_getBufferSize(3);
    uint8_t *scratch = (uint8_t*)malloc(qrCodeBufferSize + 77);

    uint8_t *pairString = (&scratch[qrCodeBufferSize]);
    pairString[0] = 'V';
    pairString[1] = '0';
    pairString[2] = '/';
    pairString[43] = '/';
    pairString[76] = 0;

    // Put the raw binary in the pairing string and expand it into nibbles
    readStorage(EEPROM_DATA_OFFSET_ADDRESS, EEPROM_DATA_LENGTH_ADDRESS, &pairString[3]);
    expandHexNibbles(&pairString[3], 20);

    // Put the secret key (for transmission) in the pairing string and expand it into nibbles
    readStorage(EEPROM_DATA_OFFSET_PAIR_SECRET, EEPROM_DATA_LENGTH_PAIR_SECRET, &pairString[44]);
    expandHexNibbles(&pairString[44], 16);

    // Show the QR code
    QRCode qrcode;
    qrcode_initText(&qrcode, &scratch[0], 3, 0, (char*)pairString);
    display_qrcodes(DISPLAY_ADDRESS, &qrcode, NULL);

    free(scratch);
}

#define SHOW_PAIRIING_SCREEN_DURATION      3000

// Note: Messages over BLECase are sent as 16 byte chunks, where each chunk contains a 4 byte
//       preamble (3 bytes checksum, 1 bit END_FLAG, 1 bit PARTIAL_FLAG, 6 bits chunk index).
//       Blocks are repeated continuously since the protocol is one-directional, there is no
//       way for the sender to know a chunk has been received. The total message from the
//       driver is a 1 byte command followed by the payload
//
//       Command 0: The payload (up to 779 bytes) is an unsigned Ethereum transacrtion
//       Command 1: The payload (up to 128 bytes) is a printable-ASCII (plus \n) message
//       Command 2: The payload (up to 48 bytes) is a binary message
//
//       The MSB of the command must be 0. A top bit of 1 may be used in the future to
//       indicate a multi-byte command

static void waitForTransaction(uint8_t unsignedTransactionHash[32]) {    

    BLECastMessage message;
    message.radioPinCE = RADIO_PIN_CE;
    message.radioPinCSN = RADIO_PIN_CSN;

#if DEBUG_SERIAL == 1
    // The Serial library takes up extra space (buffers, etc), so we cannot have as large a message
    const uint16_t messageDataSize = 356;
#else
    // 780 will hold 60 payloads (12 bytes data + 1 byte marker) for a total payload of 720 bytes
    const uint16_t messageDataSize = 780;
#endif

    // Allocate a buffer to receive the message (payload)
    uint8_t *messageData = (uint8_t*)(malloc(messageDataSize));
    if (!messageData) { crash(ErrorCodeOutOfMemory, __LINE__); }

    // The secret key to listen for; Anyone with this key can send transactions to your
    // Firefly when on and can listen to ransactions sent from your phone
    uint8_t pairSecret[EEPROM_DATA_LENGTH_PAIR_SECRET];
    readStorage(EEPROM_DATA_OFFSET_PAIR_SECRET, EEPROM_DATA_LENGTH_PAIR_SECRET, pairSecret);
    blecast_init(&message, pairSecret, messageData, messageDataSize);

    // We use this to track how long the button has been held down
    uint16_t buttonOn = 0;

    while (true) {

        // Long-hold Button? Trigger pairing screen
        if (digitalRead(BUTTON_PIN) == 0) {
             if (buttonOn == 0) {
                 display_invert(DISPLAY_ADDRESS, true);
             } else if (buttonOn > (SHOW_PAIRIING_SCREEN_DURATION + 49) / 50) {
                 display_invert(DISPLAY_ADDRESS, false);
                 break;
             }
             
             buttonOn++;

             delay(50);
             continue;

        } else if (buttonOn) {
             display_invert(DISPLAY_ADDRESS, false);
             buttonOn = 0;
        }
          
        bool foundMessage = blecast_poll(&message);
        if (!foundMessage) { continue; }

        Transaction transaction;

        // Check if this message is a valid transaction
        bool isValidTransaction = false;
        if (message.data[0] == 0x00 || message.data[0] == 0x04) {
            isValidTransaction = ethers_decodeTransaction(&transaction, &message.data[1], message.size - 1);
        }

        // Valid transaction!
        if (isValidTransaction) {
            
            // First compute the hash, so we can clobber the data portion of the transaction (to use as the value string)
            ethers_keccak256(&message.data[1], message.size - 1, unsignedTransactionHash);

            if (message.data[0] == 0x00) {
                // Hijack the data space to convert the value into a base-10 string (show up to 5 decimal places)
                ethers_toString(transaction.value, transaction.valueLength, (18 - 5), (char*)transaction.data);            
                display_transaction(DISPLAY_ADDRESS, &transaction, (char*)transaction.data);

            } else if (message.data[0] == 0x04 && transaction.dataLength == 100) {
                 // @TODO: Check sighash, make sure top address bytes are 0
                Serial.println("HERE");
                // Hijack the nonce in the data to convert the value into a base-10 string (show up to 5 decimal places)
                ethers_toString(&transaction.data[36], 32, (18 - 5), (char*)&transaction.data[68]);
                Serial.println((char*)&transaction.data[68]);
                display_contract_transaction(DISPLAY_ADDRESS, &transaction);
              
            } else {
                crash(ErrorBadTransaction, __LINE__);
            }
            
            
            
            break;

        // Valid message for signing
        } else if (message.size > 0 && ((message.data[0] == 0x01 && message.size <= (128 + 1)) || (message.data[0] == 0x02 && message.size <= (48 + 1)))) {

            uint8_t *messageData = (&message.data[1]);
            uint8_t messageLength = message.size - 1;
            
            // Null terminate the message
            messageData[messageLength] = 0;

            display_message(DISPLAY_ADDRESS, (message.data[0] == 0x02), messageData, messageLength);

            // Move the data forward 32 bytes so we have room to inject the message header
            for (int8_t i = messageLength; i >= 0; i--) {
                messageData[i + 32] = messageData[i];
            }

            uint8_t offset = 32;

            // Prepend the length (in base 10, as ascii)
            if (messageLength == 0) {
                messageData[--offset] = '0';
                messageLength++;
            } else {
                uint8_t convertLength = messageLength;
                while (convertLength) {
                    messageData[--offset] = '0' + (convertLength % 10);
                    messageLength++;
                    convertLength /= 10;
                }
            }

            // Copy the message prefix 
            offset -= 26;
            memcpy_P(&messageData[offset], messageHeaderPrefix, 26);
            messageLength += 26;

            // Compute the hash of the string with the message prefix prepended
            ethers_keccak256(&messageData[offset], messageLength, unsignedTransactionHash);
                      
            break;
        }

        // Bad message; listen for another one
        blecast_reset(&message);
    }
    
    blecast_shutdown(&message);

    free(messageData);

    if (buttonOn) {
        showPairingScreen();
        crash(ErrorCodeNone, __LINE__);
    }
}


// Note: The URI is generated entirely using upper-case letters and symbols available
//       in the QR code standard for Alphanumeric types, so that everything fits in a
//       version 3 QR code (of 77 available characters, this uses 71)
static void generateSignatureURI(char component, uint8_t *signature, char *text) {
    // URI Scheme; i.e. "SIG:"
    text[0] = 'S';
    text[1] = 'I';
    text[2] = 'G';
    text[3] = ':';

    // URI path prefix; i.e. "(R|S)/"
    text[4] = component;
    text[5] = '/';

    // Null termination
    text[7 + 64 - 1] = 0;

    // URI path; i.e. "[0-9A-F]{64}"
    uint8_t offset = 6;
    for (uint8_t i = 0; i < 32; i++) {
        text[offset++] = getHexNibble(signature[i] >> 4);
        text[offset++] = getHexNibble(signature[i]);
    }
}


static void showSignedTransaction(uint8_t *signature) {
    uint8_t qrCodeBufferSize = qrcode_getBufferSize(3);

    // We use this scratch space for: (QR Code R) (QR Code S) (temporary space to generate string)
    uint8_t *scratch = (uint8_t*)malloc(2 * qrCodeBufferSize + (7 + 64));
    if (!scratch) { crash(ErrorCodeOutOfMemory, __LINE__); }
    
    char *text = (char*)(&scratch[2 * qrCodeBufferSize]);

    // SIG:R/XXXX
    generateSignatureURI('R', &signature[0], text);

    QRCode qrcodeR;
    qrcode_initText(&qrcodeR, &scratch[0], 3, 0, text);

    // SIG:S/XXXX
    generateSignatureURI('S', &signature[32], text);

    QRCode qrcodeS;
    qrcode_initText(&qrcodeS, &scratch[qrCodeBufferSize], 3, 0, text);

    display_qrcodes(DISPLAY_ADDRESS, &qrcodeR, &qrcodeS);

    free(scratch);
}


static void signAndShowTransaction(uint8_t *unsignedTransactionHash) {

    uint8_t signature[ETHERS_SIGNATURE_LENGTH];
    bool success = ethers_sign(privateKey, unsignedTransactionHash, signature);

    if (!success) { crash(ErrorCodeSigningError, __LINE__); }

    showSignedTransaction(signature);
}

// Execution begins here
void setup() {
  
#if DEBUG_SERIAL == 1
    Serial.begin(115200);
    while (!Serial) { }
#endif

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    pinMode(RADIO_PIN_CSN, OUTPUT);
    pinMode(RADIO_PIN_CE, OUTPUT);
    
    // Make sure we have cached things that take a while to compute
    generateCache();

    // Disable all interrupts (we don't need them and makes verifying the code execution easier)
    //SREG |= (1 << 7);
    //SREG &= ~(1 << 7);

    // Initialize the I2C OLED display
    display_init(DISPLAY_ADDRESS);

    // Show the wallet address
    showAddress();

    // Wait for a valid transaction over BLECast and compute its hash
    uint8_t unsignedTransactionHash[ETHERS_KECCAK256_LENGTH];
    waitForTransaction(unsignedTransactionHash);

    // Wait for the user to accept the transaction
    waitForButton();

    // Show the hour-glass waiting screen
    display_hourglass(DISPLAY_ADDRESS);

    // Sign the transaction and show the QR codes once read
    signAndShowTransaction(unsignedTransactionHash);

    // Done; spin forever and dream of turtles
    crash(ErrorCodeNone, __LINE__);
}


void loop() {
    // Never entered
}
