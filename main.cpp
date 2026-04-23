#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <wiringPi.h>
#include "PCF8574_library/PCF8574.h"


#define START_BUTTON 0

int ledPins[5]    = {1, 2, 3, 4, 5};   
int buttonPins[5] = {6, 7, 8, 9, 10};

PCF8574 lcd(0x27); 

int score = 0;


void setupGPIO() {
    wiringPiSetup();

    pinMode(START_BUTTON, INPUT);
    pullUpDnControl(START_BUTTON, PUD_UP);

    for (int i = 0; i < 5; i++) {
        pinMode(ledPins[i], OUTPUT);
        pinMode(buttonPins[i], INPUT);
        pullUpDnControl(buttonPins[i], PUD_UP);
    }
}

void lcdPrintScore() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SCORE:");
    lcd.setCursor(0, 1);
    lcd.print(score);
}

void flashFail() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("YOU FAIL");

    for (int i = 0; i < 5; i++) {
        digitalWrite(ledPins[i], HIGH);
    }
    delay(500);
    for (int i = 0; i < 5; i++) {
        digitalWrite(ledPins[i], LOW);
    }
}

int waitForButtonPress() {
    while (true) {
        for (int i = 0; i < 5; i++) {
            if (digitalRead(buttonPins[i]) == LOW) {
                delay(200); // debounce
                return i;
            }
        }
    }
}

// ---- MAIN ----
int main() {
    srand(time(NULL));

    setupGPIO();

    // LCD init
    lcd.begin(16, 2);
    lcd.setBacklight(255);

    lcdPrintScore();

    std::cout << "Waiting for start button...\n";

    // wait for start
    while (digitalRead(START_BUTTON) == HIGH);

    std::vector<int> sequence;

    while (true) {
        int next = rand() % 5;
        sequence.push_back(next);

        // show sequence (only latest for simplicity)
        digitalWrite(ledPins[next], HIGH);
        delay(500);
        digitalWrite(ledPins[next], LOW);

        // user input
        int input = waitForButtonPress();

        if (input == next) {
            score++;
            lcdPrintScore();
        } else {
            flashFail();

            std::string name;
            std::cout << "Enter your name: ";
            std::cin >> name;

            std::cout << "Submitting score for " << name
                      << " Score: " << score << std::endl;

            // TODO: push to website (HTTP POST via curl/libcurl)

            break;
        }
    }

    return 0;
}