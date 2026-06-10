#include "stdafx.h"
#include "MyScannerUtilities.h"
#include "PlyUtilities.h"  // full include here
#include <iostream>

using namespace std;



void MyScannerUtilities::configureScanParametrs() {
    scanner->SetResolution(resolution);
    scanner->SetProfileConfig(PROFILE);
    scanner->SetFeature(FEATURE_FUNCTION_EXPOSURE_TIME, expT);
    scanner->SetFeature(FEATURE_FUNCTION_IDLE_TIME, idlT);
}

void MyScannerUtilities::SetupEncoderTriggeredScanner() {
    unsigned int Trigger = TRIG_MODE_PULSE | TRIG_POLARITY_HIGH;
    Trigger |= TRIG_INPUT_DIGIN | TRIG_EXT_ACTIVE;
    this->scanner->SetFeature(FEATURE_FUNCTION_TRIGGER, Trigger);

    unsigned int MultiPort = MULTI_DIGIN_TRIG_ONLY | MULTI_LEVEL_5V;
    this->scanner->SetFeature(FEATURE_FUNCTION_DIGITAL_IO, MultiPort);
}

MyScannerUtilities::MyScannerUtilities(CInterfaceLLT* sc, int resolution, int expT, int f) {
    scanner = sc; this->ply = nullptr;
    this->resolution = resolution;  this->expT = expT; 
    this->idlT = f;
    
    connectScanner();
    if (!connected) { return; }
    configureScanParametrs();
    SetupEncoderTriggeredScanner();
}

MyScannerUtilities::~MyScannerUtilities() {
    std::cout << "Scanner ERROR\n";
    delete scanner;
    WSACleanup();
}

void MyScannerUtilities::connectScanner() {
    scanner->CreateLLTDevice(INTF_TYPE_ETHERNET);

    std::vector<unsigned int> I(1);
    scanner->GetDeviceInterfacesFast(&I[0], 1);
    if (I.empty() || I[0] == 0) { return; }

    scanner->SetDeviceInterface(I[0], 0);
    if (scanner->Connect() < GENERAL_FUNCTION_OK) { return; }

    std::cout << "Scanner connected OK\n";
    connected = true;
}

void MyScannerUtilities::startScan(ProfileCallback cb, int* counter) {
    scanner->RegisterCallback(STD_CALL, (void*)cb, 0);
    scanner->TransferProfiles(NORMAL_TRANSFER, true);

    cout << "Scanning... " << *counter << "\n";

}

void MyScannerUtilities::endScan(int scanIdx) {
    scanner->TransferProfiles(NORMAL_TRANSFER, false);
	//ply->SavePLY(scanIdx);
	ply->storeDataInScanArray(scanIdx);
	ply->clearScans();
	ply->setProfileIdx(0);
}

void MyScannerUtilities::setProfileFrequencyAndExposure(int f, int ex) {
    int idl = (10 / f) - ex;
    this->scanner->SetFeature(FEATURE_FUNCTION_EXPOSURE_TIME, ex);
    this->scanner->SetFeature(FEATURE_FUNCTION_IDLE_TIME, idl);
}

void MyScannerUtilities::setRegionOfInterest(float x) {
    // x = half width in mm, e.g. x=15 means ±15mm = 30mm total centered on 0

    // Total X range of scanCONTROL 29xx-50 is 50mm
    // Center region: from (50% - x/50*50%) to (50% + x/50*50%)
    float halfPercent = (x / 50.0f) * 50.0f;  // x as percentage of 50mm range
    double start_x = 50.0 - halfPercent;
    double end_x = 50.0 + halfPercent;

    // scanCONTROL 29xx formula
    unsigned short col_start = (unsigned short)(65535 - (end_x / 100.0 * 65535));
    unsigned short col_size = (unsigned short)((end_x - start_x) / 100.0 * 65535);

    // Keep full Z range
    unsigned short row_start = 0;
    unsigned short row_size = 65535;

    // Activate ROI
    CInterfaceLLT* pLLT = getScanner();

    int ret = pLLT->SetFeature(FEATURE_FUNCTION_ROI1_PRESET, 0x82000800);

    ret = pLLT->SetFeature(FEATURE_FUNCTION_ROI1_DISTANCE,
        ((unsigned int)row_start << 16) + row_size);

    ret = pLLT->SetFeature(FEATURE_FUNCTION_ROI1_POSITION,
        ((unsigned int)col_start << 16) + col_size);

    ret = pLLT->SetFeature(FEATURE_FUNCTION_EXTRA_PARAMETER, 0);

    cout << "ROI set: start_x=" << start_x << "% end_x=" << end_x << "%\n";
    cout << "col_start=" << col_start << " col_size=" << col_size << "\n";
}

//-- getters and setters --//
bool MyScannerUtilities::setScanner(CInterfaceLLT* sc) {
    this->scanner = sc;
    connectScanner();
    if (!connected) { return false; }
    configureScanParametrs();
    SetupEncoderTriggeredScanner();
    return true;
}

bool MyScannerUtilities::getConnected() { return connected; }
int MyScannerUtilities::getResolution() { return this->resolution;}
int MyScannerUtilities::getExpT() { return this->expT; }
int MyScannerUtilities::getIdlT() { return this->idlT; }
void MyScannerUtilities::setPly(PlyUtilities* ply) { this->ply = ply; }
PlyUtilities* MyScannerUtilities::getPly() { return this->ply; }
CInterfaceLLT* MyScannerUtilities::getScanner() { return this->scanner;}