#ifndef ENCODER_TEST_H
#define ENCODER_TEST_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <string>

#pragma comment(lib, "ws2_32.lib")

// Scanner callback - called for every profile received
extern void __stdcall NewProfile(const unsigned char* Data, unsigned int Size, void* UserData);

// Encoder setup
bool SetupEncoderTriggeredScanner();

#endif // ENCODER_TEST_H