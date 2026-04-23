#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include <wiringPi.h>

using namespace std;

// GPIO pins
#define CONFIRM_BUTTON 5 // Start/confirm book
int buttonPins[5] = {0, 3, 13, 16, 4}; // LED select buttons
int ledPins[5]    = {7, 2, 12, 15, 1}; // LED pins
// these corispond 1:1 so button 1 goes to LED 1. This connects int wiringPI pin number to GPIO pins
// it would be easier to change these values when rewiring then it would be to find what GPIO the go to exactly imo

// I2C config
#define I2C_ADDR 0x27


int lcd_fd;

// LCD control bits
#define LCD_BACKLIGHT 0x08
#define ENABLE 0x04
#define RW 0x02
#define RS 0x01
//offsets for the cursor, 0x80 is the first row, 0xC0 is the 2nd row
int LCD_offsets[] = {0x80, 0xC0};

void lcdWriteByte(int data) {
    write(lcd_fd, &data, 1);
}

void lcdPulseEnable(int data) {
    lcdWriteByte(data | ENABLE);
    usleep(500);
    lcdWriteByte(data & ~ENABLE);
    usleep(500);
}

void lcdSend(int value, int mode) {
    int high = mode | (value & 0xF0) | LCD_BACKLIGHT;
    int low  = mode | ((value << 4) & 0xF0) | LCD_BACKLIGHT;

    lcdWriteByte(high);
    lcdPulseEnable(high);
    lcdWriteByte(low);
    lcdPulseEnable(low);
}

void lcdInit() {
    lcdSend(0x33, 0);
    lcdSend(0x32, 0);
    lcdSend(0x28, 0);
    lcdSend(0x0C, 0);
    lcdSend(0x06, 0);
    lcdSend(0x01, 0);
}

void lcdPrint(const string &s, int cursor) {
    //sets the cursor value to 0, might not be neccesary
    lcdSend(0x01, 0); 
    // this sets the writing cursor of the LCD screen
    lcdSend((LCD_offsets[cursor] + 0), 0);
    // this writes it by sending each char to LCD
    for (char c : s) { lcdSend(c, RS); } 
}

int score = 0;

void lcdShowScore() {
    lcdPrint("SCORE:", 0);
    lcdPrint(to_string(score), 1);
}

void LED_Flash(int target){
    digitalWrite(ledPins[target], HIGH);
    delay(400);
    digitalWrite(ledPins[target], LOW);
    return;
}

void flashFail() {
    lcdPrint("YOU FAIL", 0);

    for (int i = 0; i < 5; i++)
        digitalWrite(ledPins[i], HIGH);

    delay(500);
    for (int i = 0; i < 5; i++)
        digitalWrite(ledPins[i], LOW);
}

//this holds the program until a button is pressed
int waitForButton() {
    while (true) {
        for (int i = 0; i < 5; i++) {
            if (digitalRead(buttonPins[i]) == LOW) {
                delay(200); // debounce
                return i;
            }
        }
    }
}


// main function
int main() {
    // init config
    srand(time(NULL));
    wiringPiSetup();
    pinMode(START_BUTTON, INPUT);
    pullUpDnControl(START_BUTTON, PUD_UP);
    
    for (int i = 0; i < 5; i++) {
        pinMode(buttonPins[i], INPUT);
        pullUpDnControl(buttonPins[i], PUD_UP);
        pinMode(ledPins[i], OUTPUT);
    }

    // #define I2C_DEV  "/dev/i2c-1"
    lcd_fd = open("/dev/i2c-1", O_RDWR);
    /*
    if (lcd_fd < 0) {
        std::cerr << "Failed to open I2C device\n";
        return 1;
    }
    if (ioctl(lcd_fd, I2C_SLAVE, I2C_ADDR) < 0) {
        std::cerr << "Failed to set I2C address\n";
        return 1;
    }*/

    lcdInit();
    lcdShowScore();
    cout << "Waiting for start button" << endl();

    while (digitalRead(START_BUTTON) == HIGH);

    // Game Loop
    while (true) {
        int target = rand() % 5;
        vector<int> target_list;
        target_list.push_back(target);

        for (int i: target_list){ LED_Flash(target); }

        vector<int> input_list;
        input_list.clear();
        for (int i: target_list){
            int input = waitForButton();
            input_list.push_back(input);
            LED_Flash(input);
        }
        
        if (input_list == target_list) {
            score++;
            lcdShowScore();
        } else {
            flashFail();

            string name;
            cout << "Enter name: ";
            cin >> name;

            cout << "NAME: " << name
                      << " SCORE: " << score << endl;

            break;
        }
    }

    close(lcd_fd);
    return 0;
}