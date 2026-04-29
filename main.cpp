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

// global variables
int lcd_fd;
vector<BrainSample> samples;
atomic<bool> brainRunning(true);
int score = 0;

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

// opens the serial connection
int openSerial(const char* device, int baudrate) {
    // opens serial deivce
    // opens for R/W, does not make program terminal controling terminal, non-blockling open
    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

    // pulls currently terminal settings
    termios options;
    tcgetattr(fd, &options);

    // sets input and output baudrate
    cfsetispeed(&options, baudrate);
    cfsetospeed(&options, baudrate);

    // sets control flags and serial mode
    options.c_cflag |= (CLOCAL | CREAD); // ignores modern control lines, enable receiver
    options.c_cflag &= ~PARENB; // turns off parity bit checking
    options.c_cflag &= ~CSTOPB; // clears 2 bit mode if set, default is 1 stop bit
    options.c_cflag &= ~CSIZE; // removes current data bit setting
    options.c_cflag |= CS8; // sets 8 data bit mode

    // sets local modes
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // configures binary serial mode
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // disables control flow for output
    options.c_oflag &= ~OPOST; // disables any output processing

    tcsetattr(fd, TCSANOW, &options); // writes back to serial device immediately 

    return fd;
}

// writes out to linux via system call
void lcdWriteByte(int data) {
    write(lcd_fd, &data, 1);
}

// LCD only reads data when enable pin flashs high to low, this writes it out
void lcdPulseEnable(int data) {
    lcdWriteByte(data | ENABLE);
    usleep(500);
    lcdWriteByte(data & ~ENABLE);
    usleep(500);
}

/*
void lcdSend4bits(int data) {
    lcdWriteByte(data);
    lcdPulseEnable(data);
}*/

// sends data to I2C chip
// has to break values into 2 chuncks when sending them
void lcdSend(int value, int mode) {
    int high = mode | (value & 0xF0) | LCD_BACKLIGHT;
    int low  = mode | ((value << 4) & 0xF0) | LCD_BACKLIGHT;

    // writes high bytes
    lcdWriteByte(high);
    lcdPulseEnable(high);
    /// writes low bytes
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

void lcdShowScore() {
    lcdPrint("SCORE:", 0, true); // clears screen, write SCORE to line 0
    lcdPrint(to_string(score), 1, false); // writes score to line 1
}

void LED_Flash(int target){
    digitalWrite(ledPins[target], HIGH); // flashes LED to high for 400 miliseconds
    delay(400);
    digitalWrite(ledPins[target], LOW);  // flashes LED to low for 100 miliseconds
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
    

    lcdPrint("Press the", 0, true); // Clears screen, writes to first line
    lcdPrint("start button", 1, false); // writes to second line
    cout << "Waiting for start button" << endl; // writes out consol to press start button
    while (digitalRead(CONFIRM_BUTTON) == HIGH); // waits for start
    lcdShowScore(); // displays score

    // Game Loop
    vector<int> target_list;
    vector<int> input_list; 
    while (true) {
        // picking random LED code
        int target = rand() % 5; // generates random number
        target_list.push_back(target); // adds it to target vector
        for (int value: target_list){ LED_Flash(value); } // flashes LED

        // pull in user input
        input_list.clear(); // clears input list
        for (int i: target_list){
            int input = waitForButton(); // waits until input
            input_list.push_back(input); // push input onto vector
            LED_Flash(input); // flashes corresponding LED
        }
        
        // checks for victory condition
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
            for (const BrainSample& bs: samples){ vgAttention += bs.attention; }
            avgAttention /= samples.size();
            
            // asks for users name
            string name;
            cout << "Enter name: ";
            cin >> name;

            // we can replace cout with a push to the website or smt
            cout << "NAME: " << name << "\nSCORE: " << score << "\nAverage focus: " << avgAttention << endl;
            
            // writes curl command to send out
            string cmd =
            "curl -X POST http://127.0.0.1:8000/players/ "
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
