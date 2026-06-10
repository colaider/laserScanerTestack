#pragma once

#include <vector>
#include <iostream>
#include <winsock2.h>
#include "InterfaceLLT_2.h"

class ScannerUtilities {
private:
    CInterfaceLLT* scanner = nullptr;
    bool           connected = false;

    void configureScanParameters(int resolution, int expT, int idlT);
    void setupTrigger();
    void connectScanner();

public:
    ScannerUtilities(CInterfaceLLT* sc);
    ~ScannerUtilities();
    bool isConnected();
    bool getConnected();
    bool setScanner(CInterfaceLLT* sc);

};
