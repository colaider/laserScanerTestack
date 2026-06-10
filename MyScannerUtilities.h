#pragma once

#include <iostream>
#include <vector>
#include <winsock2.h>
#include "InterfaceLLT_2.h"

class PlyUtilities;

typedef void(__stdcall* ProfileCallback)(const unsigned char*, unsigned int, void*);

class MyScannerUtilities {
private:
    CInterfaceLLT* scanner;
	PlyUtilities* ply;
    bool           connected = false;
    int resolution;
    int expT;
    int idlT;

    void configureScanParametrs();
    void SetupEncoderTriggeredScanner();

public:
    MyScannerUtilities(CInterfaceLLT* sc, int resolution, int expT, int idlT);
    ~MyScannerUtilities();
    void connectScanner();
    void startScan(ProfileCallback cb, int* counter);
    void endScan(int scanIdx);
    void setProfileFrequencyAndExposure(int f, int ex);
	void setRegionOfInterest(float x);


    //-- Getters and setters --//
    bool getConnected();
    bool setScanner(CInterfaceLLT* sc);
    int getResolution();
    int getExpT();
    int getIdlT();
    void setPly(PlyUtilities* ply);
    PlyUtilities* getPly();
    CInterfaceLLT* getScanner();
};