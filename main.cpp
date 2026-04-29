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
    // atomic value so other program can break this loop and merge the thread
    while (brainRunning){
        // if brain has a update
        if (brain->update()){
            BrainSample bs;
            
            // pulls values we care about
            bs.signalQuality = brain->readSignalQuality();
            bs.attention = brain->readAttention();
            bs.meditation = brain->readMeditation();

            uint32_t* power = brain->readPowerArray();
            for (int i = 0; i < EEG_POWER_BANDS; i++){
                bs.eeg[i] = power[i];
            }
            
            // pushesh brain values onto stack
            samples.push_back(bs);
            // pluts the program to sleep to prevent CPU burn
            // headset only sends about 1 packet per seconds anyways
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

// commmand bytes for the HD44780
void lcdInit() {
    //lcdSend(BIT_MODE_8, 0); // sets 8 bit mode before switching to 4 bit mode
    lcdSend(BIT_MODE_4, 0); // 4 bit mode
    lcdSend(FUNCTION_SET, 0); // Function Set
    lcdSend(DISPLAY_CONTROL, 0); // Display On/Off Control
    lcdSend(ENTRY_MODE, 0); // Entry Mode Set
    lcdSend(RS, 0); // clears dipslay and sets cursor to (0,0)
}

void lcdPrint(const string &s, int cursor, bool LCDclear) {
    //clears LCD screen
    if(LCDclear == true){ lcdSend(RS, 0); }
    // this sets the writing cursor of the LCD screen
    lcdSend((LCD_offsets[cursor] + 0), 0);
    // this writes it by sending each char to LCD
    for (char c : s) { lcdSend(c, RS); } 
}

int score = 0;

void lcdShowScore() {
    lcdPrint("SCORE:", 0, true); // clears screen, write SCORE to line 0
    lcdPrint(to_string(score), 1, false); // writes score to line 1
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

    // itterates through buttons and LCDs pinouts 
    // initalizes wirepi 
    for (int i = 0; i < 5; i++) {
        pinMode(buttonPins[i], INPUT);
        pullUpDnControl(buttonPins[i], PUD_UP);
        pinMode(ledPins[i], OUTPUT);
    }

    
    lcd_fd = open(I2C_DEV, O_RDWR); 

    // throws error if I2C fails to open
    if (lcd_fd < 0) { std::cerr << "Failed to open I2C device\n"; return 1; }

    // throws error if I2C address doesnt exist
    if (ioctl(lcd_fd, I2C_SLAVE, I2C_ADDR) < 0) { std::cerr << "Failed to set I2C address\n"; return 1; }

    lcdInit();

    // open serial at Baudrate 9600
    int serialFd = openSerial(SER_DEV, B9600);

    // feeds the serial input to the brain class
    Brain brain(serialFd);
    // this might not be necessary but it splits off a thread to handle serial in
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

            // sets atomic value to break brain collecter thread loop
            brainRunning = false;
            // this delay exists to prevent a race condition with joining the thread and push_back in the thread
            // easier than worrying about mutex's
            delay(50);
            // joins serial thread
            brainThread.join();
            // averages out attention values
            double avgAttention = 0;
            for (const BrainSample& bs: samples){ vgAttention += bs.attention; 
            avgAttention /= samples.size();
            
            // asks for users name
            string name;
            cout << "Enter name: ";
            cin >> name;

            // we can replace cout with a push to the website or smt
            cout << "NAME: " << name << "\nSCORE: " << score << "\nAverage focus: " << avgAttention << endl;
            
            // writes curl command to send out
            string cmd =
            "curl -X POST http://127.0.0.1:5000/submit "
            "-H 'Content-Type: application/json' "
            "-d '{\"name\":\"" + name + "\","
            "\"score\":" + to_string(score) + ","
            "\"attention\":" + to_string(avgAttention) + "}'";

            // sends curl command
            system(cmd.c_str());
            break;
        }
    }

    // cleans up I2C
    close(lcd_fd);
    return 0;
}
