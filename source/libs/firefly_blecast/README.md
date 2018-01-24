BLECast
=======

BLECast is a protocol and library to send messages (up to 756 bytes) over the BLE
advertsizing data, from iOS to an Arduino using the nRF2401.

**Features:**

- Works out of the box with iOS
- Up to 756 bytes per message
- nRF2401 is cheap ($1 or less)


**Note:** This library and protocol are still VERY experimental; the API and protocol may change.


API
---

### BLECastMessage

An instance of this structure us used to manage the currently discovered blocks and
the radio state.

```c
#define BLECAST_INVALID_ID     0x7f000000

typedef struct BLECastMessage {
    
    // The number of unique payloads that have been successfully discovered.
    int8_t discoveredPayloadCount;

    // The total number of expected payloads. This is -1 if the length
    // is not yet known.
    int8_t totalPayloadCount;

    // The size (in bytes) of the message and the message data.
    int16_t size;
    uint8_t *data;

    // The ID of the message (the CRC-24 of the total message)
    // This is equal to BLECAST_INVALID_ID until a message is complete
    uint32_t id;
    
    // Generally, do NOT touch these
    void *headPayload;
    void *aesContext;
    void *radioInfo;
} BLECastMessage;
```

### Initializing

Initialize *message* with the 128-bit (16 byte) secret *key* used to decrypt payloads
and the *pin1* and *pin2* indicate the CE and CSN pins of the nRF2401.

```c
bool blecast_init(BLECastMessage *message, uint8_t key[16], uint8_t pin1, uint8_t pin2)
```


### Polling

Returns true if a complete message has been found. This must be called periodically to
check the radio's buffer for payloads, which are constructed into a message.

```c
bool blecast_poll(BLECastMessage *message)
```


### Resetting

To continue using the same key and configured radio, reset will clean up the
payload memory, but keep the internal state for re-using a BLECastMessage.

```c
void blecast_reset(BLECastMessage *message)
```


### Cleaning up

There are various data structures stored on the heap. This must be called when a
message is not longer needed to reclaim this memory.

```c
void blecast_free(BLECastMessage *message);
```


Example
-------

This example will continuously listen for new messages and display it in the serial monitor.

Please see the example in examples for pin connections.

```c
#include "blecast.h"

BLECastMessage message;

uint32_t lastMessageId = BLECAST_INVALID_ID;

void setup() {

    // Set up the Serial Monitor
    Serial.begin(115200);
    while (!Serial) { }

    Serial.println("BLECast ready.");

    // The secret key to listen for
    uint8_t key[] = { 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 2, 2, 3, 3, 4, 4 };

    // Initialize the message data structure
    blecast_init(&message, key, 9, 10);
}

void loop() {
    
    // Poll for a message (returns true if a new message is complete)
    if (blecast_poll(&message)) {
        
        // A different message
        if (message.id != lastMessageId) {

            Serial.print("Got Message: ");

            // Print each characater and a new line
            for (uint8_t i = 0; i < message.size; i++) { Serial.print((char)(message.data[i])); }
            Serial.println("\n");
             
            // Remember this message ID
            lastMessageId = message.id; 
        }
        
        // Reset the message data structure so it is ready for a new message
        blecast_reset(&message);
    }
}
```

Protocol
--------

The data is sent as the UUID of a BLE advertising service packet. This limits a payload to 16 bytes.

If the message is over 12 bytes, it is prefixed with the CRC-24 checksum of the message.

The message is chunked into 12 byte blocks, with a 4 byte header.

Each block:

- The CRC-24 of the block without this checksum (13 bytes) - 24 bits
- The Termination bit; 1 if this is the last block, 0 otherwise - 1 bit
- The Partial bit; 1 if this block is less than 12 bytes, 0 if it is 12 - 1 bit
- The Index of this block - 6 bits
- The block data - 12 bytes

The CRC-24 bytes is then extended across each byte, bit-shift by 1 bit for each byte.

The entire payload is then encrypted using AES-128-ECB.


License
-------

MIT License.

Dependency libraries' are licenced under other various libraries. I plan to migrate off these
libraries over time.
