/*
Test code cobbled together just to test the brain.cpp
*/
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include "brain.hpp"

int openSerial(const char* device, speed_t baudrate) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Failed to open serial port " << device
                  << ": " << std::strerror(errno) << std::endl;
        return -1;
    }

    struct termios options{};
    if (tcgetattr(fd, &options) != 0) {
        std::cerr << "tcgetattr failed: " << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    cfsetispeed(&options, baudrate);
    cfsetospeed(&options, baudrate);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    options.c_oflag &= ~OPOST;

    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        std::cerr << "tcsetattr failed: " << std::strerror(errno) << std::endl;
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return fd;
}

int main() {
    const char* serialDevice = "/dev/serial0";  

    int serialFd = openSerial(serialDevice, B9600);
    if (serialFd < 0) {
        return 1;
    }

    std::cout << "Listening for EEG data on " << serialDevice << " at 9600 baud..." << std::endl;
    std::cout << "Press Ctrl+C to stop.\n" << std::endl;

    Brain brain(serialFd);

    while (true) {
        if (brain.update()) {
            std::cout << brain.readCSV() << std::endl;
            std::cout.flush();
        }

        usleep(200);
    }

    close(serialFd);
    return 0;
}
