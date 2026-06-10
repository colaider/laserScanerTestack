#include "stdafx.h"
#include "ArduinoComm.h"
#include "MyScannerUtilities.h"
#include "PlyUtilities.h"
#include "HyperME.h"
#include "CSVUtils.h"
#include "MetrologyFFT.h"


using namespace std;


ArduinoComm::ArduinoComm(char* ip, int port, MyScannerUtilities* scanner) {
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	this->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	this->addr.sin_family = AF_INET;
	this->addr.sin_port = htons(port);
	this->addr.sin_addr.s_addr = inet_addr(ip);
	this->scanner = scanner;

	connected = connect(s, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR;
}


void ArduinoComm::setIpAndPort(char* ip, int port) {
	this->addr.sin_port = htons(port);
	this->addr.sin_addr.s_addr = inet_addr(ip);
}


string ArduinoComm::sendData(const char* data) {
	string msg = string(data) + "\n";
	char buf[256];
	string fullResponse;

	send(s, msg.c_str(), (int)msg.size(), 0);

	while (true) {
		int received = recv(s, buf, sizeof(buf) - 1, 0);
		if (received > 0){
			buf[received] = '\0';
			string chunk(buf);
			fullResponse += chunk;
			if (fullResponse.find("DONE") != string::npos) {break;}
		}
	}
	return fullResponse;
}  


ArduinoComm::~ArduinoComm() {
	cout << "Closing socket\n";
	closesocket(s);
	WSACleanup();
}


void ArduinoComm::moveInDis(float dis, unsigned int speed, int idx, int record){
	char cmd[64];
	sprintf(cmd, "MF%03.0fA%dS%04dR%d", abs(dis), idx, speed, record);
	cmd[1] = dis < 0 ? 'B' : 'F';
	this->sendData(cmd);
}


void ArduinoComm::script(float totalWidth, float sectionY, float sectionX, ProfileCallback cb) {
	float resolution = 40.0 / 1000.0;
	float xCorrection = 0.0f;
	int scetionsNum = (int)ceil(totalWidth / sectionX);
	int totalScans = (int)(sectionY / resolution);
	vector<MyMetrologyEngine::GrooveData> grooveAreas;
	CSVUtils* csvUtils = new CSVUtils("./CSV/csvLog.csv");
	const double PI = 3.141592653589793;

	// 2, 35
	float blueRegion = 0.0f, redRegion = 0.0f; int batchLength = 0; bool hole = false; float minHolA = 0.0f;

	cout << "Measure holes? (1 for yes, 0 for grooves): ";
	cin >> hole;
	cout << "You entered: " << hole << endl;

	if (!hole) {
		cout << "Enter a blue region length: ";
		cin >> blueRegion;
		cout << "You entered: " << blueRegion << endl;
		cout << "Enter a red region length: ";
		cin >> redRegion;
		cout << "You entered: " << redRegion << endl;
	}
	if (hole) {
		cout << "Enter min hole area: ";
		cin >> minHolA;
		cout << "You entered: " << minHolA << endl;
	}

	cout << "Enter batch length: ";
	cin >> batchLength; 
	cout << "You entered: " << batchLength << endl;	
	



	this->sendData("HOME");
	this->moveInDis(45, 400, 1, 0);
	this->moveInDis(15, 50, 0, 0);

	this->scanner->getPly()->startSaveThread();
	this->scanner->setRegionOfInterest(sectionX + 2.0f);



	float cumulativeX = 0.0f; float rotAngle = 0.0f;
	int i = 0; 
	while(fabs(cumulativeX) < totalWidth) {
		int count = this->scanner->getPly()->getProfileIdx();
		Sleep(500);  // let scanner settle
		scanner->startScan(cb, &count);
		Sleep(200);  // let scanner settle
		this->moveInDis(sectionY, 100, 0, 1);
		scanner->endScan(i);

		MetrologyFFT mEngine("./ply/PointCloud_6.ply", this->scanner->getPly());
		mEngine.setRange(sectionY);
		mEngine.getPlyUtils()->correctXRange(sectionX / 2);
		mEngine.colorByPlaneDistance();
		xCorrection = !hole ?  mEngine.scanDisplacementCorrrection(sectionX) : mEngine.scanDisplacementCorrrectionHoles();
		
		
		mEngine.removeBottomPoints(0.15f);
		if (i == 0) { mEngine.alignXAxis(); rotAngle = mEngine.getRotTeta(); }
		else { mEngine.applyRotation(rotAngle); }

		mEngine.colorEndRegions(blueRegion, redRegion);
		vector<MyMetrologyEngine::GrooveData> areas;
		areas = !hole ? mEngine.getGrooveAreas(30.0f) : mEngine.getHoleAreas(minHolA);
		

		for (int j = 0; j < (int)areas.size(); j++) {
			grooveAreas.push_back(areas[j]);
			mEngine.writeTextToCloud(to_string((int)areas[j].area), areas[j].centerX, areas[j].centerY);
		}

		if (mEngine.noPointLeft != 0.0f) { xCorrection = sectionX; }
		if (mEngine.noPointRight != 0.0f) { xCorrection = sectionX/2.0f; }

		// Shift cloud to world coordinates
		mEngine.shiftInX(cumulativeX);
		this->scanner->getPly()->enqueueSave(mEngine.getCloloredScan(), i);

		cout << "X Correction for this scan:\n";
		cout << "Section " << i + 1 << " grooves. X correction: " << xCorrection << "\n";

		// Compute next step
		float stepX = (xCorrection != 0.0f) ? sectionX - (xCorrection+1.5f): sectionX;
		cumulativeX += stepX;

		this->moveInDis(-sectionY, 50, 0, 0);
		this->moveInDis(stepX, 400, 1, 0);
		i++;
	}
	this->scanner->getPly()->stopSaveThread();
	std::vector<float> avgWidths; std::vector<float> areas;
	for (const auto& g : grooveAreas) { avgWidths.push_back(!hole ? g.area/redRegion : 2*sqrt(g.area/ PI)); areas.push_back(g.area); cout  << g.area << "\n"; }
	
	csvUtils->writeToCSV(areas, avgWidths, batchLength, hole);


}   