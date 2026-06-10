#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include "stdafx.h"
#include <iostream>
#include <conio.h>
#include "InterfaceLLT_2.h"
#include "GetProfiles_Callback_Measure.h"
#include "MyScannerUtilities.h"
#include "PlyUtilities.h"
#include "ArduinoComm.h"
#include "MyMetrologyEngine.h"
#include "HyperME.h"
#include "MetrologyFFT.h"
#include "DataGrouper.h"
#include "structs.h"



using namespace std;

MyScannerUtilities* g_Scanner = nullptr;
PlyUtilities* g_Ply = nullptr;
ArduinoComm* arduinoComm = nullptr;
MetrologyFFT* mEngine = nullptr;
DataGrouper* dataGr = nullptr;

bool errorCall(const char* msg) {
    cout << "ERROR: " << msg << "\n";
    WSACleanup();
    while (!_kbhit()) {}
    return -1;
}

void __stdcall NewProfile(const unsigned char* Data, unsigned int Size, void* UserData) {
    g_Ply->addScanLine(Data, Size);
	g_Ply->setProfileIdx(g_Ply->getProfileIdx() + 1);
    if (g_Ply->getProfileIdx() % 10 == 0)
        cout << "Scans: " << g_Ply->getProfileIdx() << "\r";
}


int main(int argc, char* argv[]) {
    bool LoadError = false;
    CInterfaceLLT* CLLT = new CInterfaceLLT("..\\LLT.dll", &LoadError);
 //   
    g_Scanner = new MyScannerUtilities(CLLT, 1280, 200, 50);
    g_Ply = new PlyUtilities("./ply/", g_Scanner);
    arduinoComm = new ArduinoComm("192.168.1.2", 5010, g_Scanner);
	g_Scanner->setPly(g_Ply);

    if (LoadError) { errorCall("Can't load DLL"); return -1; }
    if (!g_Scanner->getConnected()) { errorCall("Scanner not connected"); return -1; }
    if (!arduinoComm->getConnected()) { errorCall("Arduino not connected"); return -1; }


    //500 55
    float lengthX = 0.0f, lengthY = 0.0f;
    cout << "Enter a length: ";
    cin >> lengthX;
    cout << "You entered: " << lengthX << endl;
    cout << "Enter a width: ";
    cin >> lengthY;
    cout << "You entered: " << lengthY << endl;

     arduinoComm->script(lengthX, lengthY, 20.0, NewProfile);

    //cout << "\n\nTotal scan lines: " << g_Ply->getProfileIdx() << "\n";
    cout << "\nPress any key to exit...\n";
    while (1) {};
    while (!_kbhit()) {}
    return 0;
}



