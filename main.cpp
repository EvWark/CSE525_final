#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <utility>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "constants.hpp"
#include <wiringPi.h>

using namespace std;

int lcd_fd;

void lcdWriteByte(int data) {
    write(lcd_fd, &data, 1);
}

void lcdPulseEnable(int data) {
    lcdWriteByte(data | ENABLE);
    usleep(500);
    lcdWriteByte(data & ~ENABLE);
    usleep(500);
}

void lcdSend4bits(int data) {
    lcdWriteByte(data);
    lcdPulseEnable(data);
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
    delay(100);
    return;
}

void flashFail() {
    lcdPrint("YOU FAIL", 0);

    for (int i = 0; i < 5; i++) { digitalWrite(ledPins[i], HIGH); }
    delay(500);
    for (int i = 0; i < 5; i++) { digitalWrite(ledPins[i], LOW); }
}

// this holds the program until a button is pressed
// kind of badly made but it works i guess
// need to add a value to bounce out of this if you wait too long
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
    pinMode(CONFIRM_BUTTON, INPUT);
    pullUpDnControl(CONFIRM_BUTTON, PUD_UP);

    for (int i = 0; i < 5; i++) {
        pinMode(buttonPins[i], INPUT);
        pullUpDnControl(buttonPins[i], PUD_UP);
        pinMode(ledPins[i], OUTPUT);
    }

    #define I2C_DEV  "/dev/i2c-1"
    lcd_fd = open(I2C_DEV, O_RDWR);

    lcdInit();
    lcdPrint( "Waiting for start button", 0);
    cout << "Waiting for start button" << endl;
    while (digitalRead(CONFIRM_BUTTON) == HIGH);
    // displays score
    lcdShowScore();

    // Game Loop
    vector<int> target_list;
    while (true) {
        int target = rand() % 5;
        cout << target << endl;
        target_list.push_back(target);
        for (int value: target_list){ LED_Flash(value); }

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
            // we can replace cout with a push to the website or smt
            cout << "NAME: " << name << " SCORE: " << score << endl;

            break;
        }
    }

    close(lcd_fd);
    return 0;
}