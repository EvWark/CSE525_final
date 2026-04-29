/*
This has pins defintion and offsets
Most of these you should never need to mess with
*/

#ifndef PINSHPP
#define PINSHPP
// GPIO pins
#define CONFIRM_BUTTON 5 // Start/confirm book
int buttonPins[5] = {0, 3, 13, 24, 4}; // LED select buttons
int ledPins[5]    = {14, 2, 12, 25, 1}; // LED pins
// these corispond 1:1 so button 1 goes to LED 1. This connects int wiringPI pin number to GPIO pins
// it would be easier to change these values when rewiring then it would be to find what GPIO the go to exactly imo

// I2C config
#define I2C_ADDR 0x27
#define I2C_DEV "/dev/i2c-1"
#define SER_DEV "/dev/serial0"

// LCD control bytes
#define BIT_MODE_8 0x33 // 0011 0011
#define BIT_MODE_4 0x32 // 0011 0010
#define FUNCTION_SET 0x28 // 0010 1000 4 bit mode, 2 display lines, regular character font
#define DISPLAY_CONTROL 0x0C // 0000 1100 Turns on display, cursor is not displayed, character does not blink
#define LCD_BACKLIGHT 0x08 // 0000 1000
#define ENTRY_MODE 0x06 // 0000 0110
#define ENABLE 0x04 // 0000 0100
#define RW 0x02 // 0000 0010
#define RS 0x01 // 0000 0001
//offsets for the cursor, 0x80 is the first row, 0xC0 is the 2nd row

int LCD_offsets[] = {0x80, 0xC0}; // 1000 0000, 1100 0000

#endif