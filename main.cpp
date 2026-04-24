#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <thread>
#include <termios.h>

#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "constants.hpp"
#include "brain.hpp"

#include <wiringPi.h>

using namespace std;

int lcd_fd;
vector<BrainSample> samples;
atomic<bool> brainRunning(true);

// this checks if there is a update from serial
void brainCollector(Brain* brain){
    while (brainRunning){
        if (brain->update()){
            BrainSample bs;
            
            bs.signalQuality = brain->readSignalQuality();
            bs.attention = brain->readAttention();
            bs.meditation = brain->readMeditation();

            uint32_t* power = brain->readPowerArray();
            for (int i = 0; i < EEG_POWER_BANDS; i++){
                bs.eeg[i] = power[i];
            }
            
            samples.push_back(bs);
            usleep(200);
        }
    }
}

int openSerial(const char* device, int baudrate) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

    termios options;
    tcgetattr(fd, &options);

    cfsetispeed(&options, baudrate);
    cfsetospeed(&options, baudrate);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    tcsetattr(fd, TCSANOW, &options);

    return fd;
}

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

void lcdPrint(const string &s, int cursor, bool LCDclear) {
    //clears LCD screen
    if(LCDclear == true){ lcdSend(0x01, 0); }
    // this sets the writing cursor of the LCD screen
    lcdSend((LCD_offsets[cursor] + 0), 0);
    // this writes it by sending each char to LCD
    for (char c : s) { lcdSend(c, RS); } 
}

int score = 0;

void lcdShowScore() {
    lcdPrint("SCORE:", 0, true);
    lcdPrint(to_string(score), 1, false);
}

void LED_Flash(int target){
    digitalWrite(ledPins[target], HIGH);
    delay(400);
    digitalWrite(ledPins[target], LOW);
    delay(100);
    return;
}

void flashFail() {
    lcdPrint("YOU FAILED", 0, true);
    int i = 0;
    while(i < 3){
        for (int i = 0; i < 5; i++) { digitalWrite(ledPins[i], HIGH); }
        delay(400);
        for (int i = 0; i < 5; i++) { digitalWrite(ledPins[i], LOW); }
        delay(100);
        i++;
    }
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

    
    lcd_fd = open(I2C_DEV, O_RDWR);
        if (lcd_fd < 0) {
        std::cerr << "Failed to open I2C device\n";
        return 1;
    }

    if (ioctl(lcd_fd, I2C_SLAVE, I2C_ADDR) < 0) {
        std::cerr << "Failed to set I2C address\n";
        return 1;
    }

    lcdInit();

    // open serial at Baudrate 9600
    int serialFd = openSerial(SER_DEV, B9600);

    Brain brain(serialFd);
    thread brainThread(brainCollector, &brain);
    
    lcdPrint("Press the", 0, true);
    lcdPrint("start button", 1, false);
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

            brainRunning = false;
            // this delay exists to prevent a race condition with joining the thread and push_back in the thread
            // easier than worrying about mutex's
            delay(50);
            brainThread.join();
            double avgAttention = 0;
            for (const BrainSample& bs: samples){
                avgAttention += bs.attention;
            }
            avgAttention /= samples.size();
            
            string name;
            cout << "Enter name: ";
            cin >> name;

            //temp 
            avgAttention = 5;
            // we can replace cout with a push to the website or smt
            cout << "NAME: " << name << "\nSCORE: " << score << "\nAverage focus: " << avgAttention << endl;
            
            string cmd =
            "curl -X POST http://127.0.0.1:5000/submit "
            "-H 'Content-Type: application/json' "
            "-d '{\"name\":\"" + name + "\","
            "\"score\":" + to_string(score) + ","
            "\"attention\":" + to_string(avgAttention) + "}'";

            system(cmd.c_str());
            break;
        }
    }

    close(lcd_fd);
    return 0;
}