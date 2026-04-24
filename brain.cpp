/*
This is a PI3 port of an already existing library. 
I attempted to make as few changes as possible to the existing library
This includes leaving the orginal comments where it makes sense
I also added some of my own comments where it makes sense
You can fine the orginal cpp file here:
https://github.com/kitschpatrol/brain/blob/main/Brain.cpp
*/

#include <cstdio>
#include <string<
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include "Brain.h"

using namespace std;

// constructor
Brain::Brain(int serialFd) {
    this->serialFd = serialFd;

    // Keep the rest of the initialization process in a separate method in case
    // we overload the constructor.
    init();
}

void Brain::init() {
    // It's up to the calling code to start the stream
    // Usually Serial.begin(9600);
    freshPacket = false;
    inPacket = false;
    packetIndex = 0;
    packetLength = 0;
    eegPowerLength = 0;
    hasPower = false;
    checksum = 0;
    checksumAccumulator = 0;

    signalQuality = 200;
    attention = 0;
    meditation = 0;

    clearEegPower();
}

boolean Brain::update() {
    uint8_t byte;
    int n = read(serialFd, &byte, 1);

    if (n == 1) { 
        latestByte = byte; 

        // Build a packet if we know we're and not just listening for sync bytes.
        if (inPacket) {

            // First byte after the sync bytes is the length of the upcoming packet.
            if (packetIndex == 0) {
                packetLength = latestByte;

                // Catch error if packet is too long
                if (packetLength > MAX_PACKET_LENGTH) {
                    // Packet exceeded max length
                    // Send an error
                    sprintf(latestError, "ERROR: Packet too long %i", packetLength);
                    inPacket = false;
                }
            }
            else if (packetIndex <= packetLength) {
                // Run of the mill data bytes.

                // Print them here

                // Store the byte in an array for parsing later.
                packetData[packetIndex - 1] = latestByte;

                // Keep building the checksum.
                checksumAccumulator += latestByte;
            }
            else if (packetIndex > packetLength) {
                // We're at the end of the data payload.

                // Check the checksum.
                checksum = latestByte;
                checksumAccumulator = 255 - checksumAccumulator;

                // Do they match?
                if (checksum == checksumAccumulator) {
                    boolean parseSuccess = parsePacket();

                    if (parseSuccess) {
                        freshPacket = true;
                    }
                    else {
                        // Parsing failed, send an error.
                        sprintf(latestError, "ERROR: Could not parse");
                        // good place to print the packet if debugging
                    }
                }
                else {
                    // Checksum mismatch, send an error.
                    sprintf(latestError, "ERROR: Checksum");
                    // good place to print the packet if debugging
                }
                // End of packet

                // Reset, prep for next packet
                inPacket = false;
            }

            packetIndex++;
        }

        // Look for the start of the packet
        if ((latestByte == 170) && (lastByte == 170) && !inPacket) {
            // Start of packet
            inPacket = true;
            packetIndex = 0;
            checksumAccumulator = 0;
        }

        // Keep track of the last byte so we can find the sync byte pairs.
        lastByte = latestByte;
    }

    if (freshPacket) {
        freshPacket = false;
        return true;
    }
    else {
        return false;
    }

}

void Brain::clearPacket() {
    for (uint8_t i = 0; i < MAX_PACKET_LENGTH; i++) {
        packetData[i] = 0;
    }
}

void Brain::clearEegPower() {
    // Zero the power bands.
    for(uint8_t i = 0; i < EEG_POWER_BANDS; i++) {
        eegPower[i] = 0;
    }
}

boolean Brain::parsePacket() {
    // Loop through the packet, extracting data.
    // Based on mindset_communications_protocol.pdf from the Neurosky Mindset SDK.
    // Returns true if passing succeeds
    hasPower = false;
    boolean parseSuccess = true;
    // int rawValue = 0;

    clearEegPower();    // clear the eeg power to make sure we're honest about missing values

    for (uint8_t i = 0; i < packetLength; i++) {
        switch (packetData[i]) {
            case 0x2:
                signalQuality = packetData[++i];
                break;
            case 0x4:
                attention = packetData[++i];
                break;
            case 0x5:
                meditation = packetData[++i];
                break;
            case 0x83:
                // ASIC_EEG_POWER: eight big-endian 3-uint8_t unsigned integer values representing delta, theta, low-alpha high-alpha, low-beta, high-beta, low-gamma, and mid-gamma EEG band power values
                // The next uint8_t sets the length, usually 24 (Eight 24-bit numbers... big endian?)
                // We dont' use this value so let's skip it and just increment i
                i++;

                // Extract the values
                for (int j = 0; j < EEG_POWER_BANDS; j++) {
                	uint8_t a,b,c;
                    a = packetData[++i];
                    b = packetData[++i];
                    c = packetData[++i];
                    eegPower[j] = ((uint32_t)a << 16) | ((uint32_t)b << 8) | (uint32_t)c;
                }

                hasPower = true;
                // This seems to happen once during start-up on the force trainer. Strange. Wise to wait a couple of packets before
                // you start reading.
                break;
            case 0x80:
                // We dont' use this value so let's skip it and just increment i
                // uint8_t packetLength = packetData[++i];
                // rawValue = ((int)packetData[++i] << 8) | packetData[++i];
                i += 3;
                break;
            default:
                // Broken packet ?
                /*
                Serial.print(F("parsePacket UNMATCHED data 0x"));
                Serial.print(packetData[i], HEX);
                Serial.print(F(" in position "));
                Serial.print(i, DEC);
                printPacket();
                */
                parseSuccess = false;
                break;
        }
    }
    return parseSuccess;
}

// Keeping this around for debug use
void Brain::printCSV() {
    // Print the CSV over serial
    
    printf("%d, %d, %d", signalQuality, attention, meditation);

    if (hasPower) {
        for(int i = 0; i < EEG_POWER_BANDS; i++) {
            printf(",");
            printf("%d", eegPower[i]);
        }
    }

    printf("\n");
}

char* Brain::readErrors() {
    return latestError;
}

char* Brain::readCSV() {
    // spit out a big string?
    // find out how big this really needs to be
    // should be popped off the stack once it goes out of scope?
    // make the character array as small as possible

    if(hasPower) {

        sprintf(csvBuffer,"%d,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
            signalQuality,
            attention,
            meditation,
            eegPower[0],
            eegPower[1],
            eegPower[2],
            eegPower[3],
            eegPower[4],
            eegPower[5],
            eegPower[6],
            eegPower[7]
        );

        return csvBuffer;
    }
    else {
        sprintf(csvBuffer,"%d,%d,%d",
            signalQuality,
            attention,
            meditation
        );

        return csvBuffer;
    }
}

// For debugging, print the entire contents of the packet data array.
void Brain::printPacket() {
    printf("[");
    for (uint8_t i = 0; i < MAX_PACKET_LENGTH; i++) {
        printf(packetData[i], DEC);

            if (i < MAX_PACKET_LENGTH - 1) {
                printf(", ");
            }
    }
    printfln("]");
}

void Brain::printDebug() {
    printf("\n");
    printf("\n--- Start Packet ---");
    printf("Signal Quality: %d\n", signalQuality);
    printf("Attention: %d\n", attention);
    printf("Meditation: %d\n", meditation);

    if (hasPower) {
        printf("\n");
        printf("\n EEG POWER:");
        printf("Delta: %d\n", eegPower[0]);
        printf("Theta: %d\n", eegPower[1]);
        printf("Low Alpha: %d\n", eegPower[2]);
        printf("High Alpha: %d\n", eegPower[3]);
        printf("Low Beta: %d\n", eegPower[4]);
        printf("High Beta: %d\n", eegPower[5]);
        printf("Low Gamma: %d\n", eegPower[6]);
        printf("Mid Gamma: %d\n", eegPower[7]);
    }

    printf("\n");
    printf("Checksum Calculated: %d\n", checksumAccumulator);
    printf("Checksum Expected: %d\n", checksum);

    printf("\n--- End Packet ---\n");
}

uint8_t Brain::readSignalQuality() {
    return signalQuality;
}

uint8_t Brain::readAttention() {
    return attention;
}

uint8_t Brain::readMeditation() {
    return meditation;
}

uint32_t* Brain::readPowerArray() {
    return eegPower;
}

uint32_t Brain::readDelta() {
    return eegPower[0];
}

uint32_t Brain::readTheta() {
    return eegPower[1];
}

uint32_t Brain::readLowAlpha() {
    return eegPower[2];
}

uint32_t Brain::readHighAlpha() {
    return eegPower[3];
}

uint32_t Brain::readLowBeta() {
    return eegPower[4];
}

uint32_t Brain::readHighBeta() {
    return eegPower[5];
}

uint32_t Brain::readLowGamma() {
    return eegPower[6];
}

uint32_t Brain::readMidGamma() {
    return eegPower[7];
}