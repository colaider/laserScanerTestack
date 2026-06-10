#pragma once
#include <iostream>
#include <vector>
#include <winsock2.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")
class HyperME;
class MyScannerUtilities;
typedef void(__stdcall* ProfileCallback)(const unsigned char*, unsigned int, void*);


class ArduinoComm{ 
	private:
		SOCKET s;
		sockaddr_in addr{};
		bool connected = false;
		MyScannerUtilities* scanner = nullptr;
		HyperME* mEngine = nullptr;
		std::string sendData(const char* data);

	public:
		ArduinoComm(char * ip, int port, MyScannerUtilities* scanner);
		~ArduinoComm();

		void moveInDis(float dis, unsigned int speed, int idx, int record);


		void script(float totalWidth, float sectionY, float sectionX, ProfileCallback cb);

		//-- getters and setters 
		void setIpAndPort(char* ip, int port);
		bool getConnected() { return connected; }
};