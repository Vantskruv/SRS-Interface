#include "pch.h"
#include "SRSInterface.h"
#include "../srsinterfaceplugin.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>
#include <WS2tcpip.h>
#include <TlHelp32.h>

#pragma comment (lib, "ws2_32.lib")
using namespace nlohmann;


SRSInterface::SRSInterface(SRSInterfacePlugin* pluginCallback)
{
	srsInterfacePlugin = pluginCallback;
}

SRSInterface::~SRSInterface()
{
	stop_server();
}


void SRSInterface::SelectRadio(int radio)
{
	// Startup Winsock
	WSADATA data;
	WORD version = MAKEWORD(2, 2);
	int wsOK = WSAStartup(version, &data);

	if (wsOK != 0)
	{
		//std::cout << "Can't start Winsock! " << wsOK << std::endl;
		return;
	}

	// Create a hint structure for the server
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(SRSInterface::UDP_SEND_PORT);
	inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

	// Socket creation
	SOCKET out = socket(AF_INET, SOCK_DGRAM, 0);

	// Write out to that socket
	//std::string s = "{Command: 0, RadioId: 1, Frequency: 144000000.0}\n"; // Works! Setting frequency to 144.000
	std::string s = "{Command: 1, RadioId: " + std::to_string(radio) + "}\n"; // Works! Selecting radio
	int sendOK = sendto(out, s.c_str(), s.size() + 1, 0, (sockaddr*)&server, sizeof(server));

	if (sendOK == SOCKET_ERROR)
	{
		//std::cout << "That didn't work! " << WSAGetLastError() << std::endl;
	}

	// Close the socket
	closesocket(out);
	WSACleanup();

	return;
}


int SRSInterface::server_thread()
{

	// Startup Winsock
	WSADATA data;
	WORD version = MAKEWORD(2, 2);
	int wsOK = WSAStartup(version, &data);

	if (wsOK != 0)
	{
		std::cout << "Can't start Winsock! " << wsOK << std::endl;
		return wsOK;
	}

	// Bind socket to ip address and port
	//SOCKET serverIn = socket(AF_INET, SOCK_DGRAM, 0);
	serverInSocket = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in serverHint;
	serverHint.sin_addr.S_un.S_addr = ADDR_ANY;
	serverHint.sin_family = AF_INET;
	serverHint.sin_port = htons(SRSInterface::UDP_RECEIVE_PORT); // Convert from little to big endian

	if (bind(serverInSocket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR)
	{
		return WSAGetLastError();
	}

	sockaddr_in client;
	int clientSize = sizeof(client);

	int RECIEVE_BUFFER_SIZE = 65507;
	char* recieveBuffer = new char[RECIEVE_BUFFER_SIZE];

	/*
	//DEBUG from file ----------------------------------
	std::ifstream fIn;
	fIn.open("jsondatatest.txt", std::ifstream::in);

	if (fIn.is_open())
	{
		std::stringstream sBuffer;
		sBuffer << fIn.rdbuf();
		fIn.close();

		try
		{
			while (isServerThreadUpdating)
			{
				const json jsonBuffer = json::parse(sBuffer.str());
				srsInterfacePlugin->UpdateData(jsonBuffer);
				Sleep(1000);
			}
		}
		catch (...)
		{
		};
		
	}
	-----------------------------------------------------
	*/


	while (isServerThreadUpdating)
	{
		ZeroMemory(&client, clientSize);
		ZeroMemory(recieveBuffer, RECIEVE_BUFFER_SIZE);
		int bytesIn = recvfrom(serverInSocket, recieveBuffer, RECIEVE_BUFFER_SIZE, 0, (sockaddr*)&client, &clientSize);
		if (bytesIn == SOCKET_ERROR)
		{
			//std::cout << "Error receiving from client " << WSAGetLastError() << std::endl;
			Sleep(1000);
			continue;
		}
		//else std::cout << "Updating!" << std::endl;

		if (srsInterfacePlugin)
		{
			try
			{
				const json jsonBuffer = json::parse(recieveBuffer);
				srsInterfacePlugin->UpdateData(jsonBuffer);
			}
			catch (...)
			{
			}
		}
	}

	if (serverInSocket!= 0) closesocket(serverInSocket);
	serverInSocket = 0;
	WSACleanup();

	return 0;
}

int SRSInterface::start_server()
{
	std::lock_guard<std::mutex> lock(mServer);
	isServerThreadUpdating = false;
	if (serverInSocket != 0) closesocket(serverInSocket);
	if (tServer.joinable()) tServer.join();

	isServerThreadUpdating = true;
	tServer = std::thread(&SRSInterface::server_thread, this);
	return 0;
}

void SRSInterface::stop_server()
{
	std::lock_guard<std::mutex> lock(mServer);
	isServerThreadUpdating = false;
	if (serverInSocket != 0) closesocket(serverInSocket);
	if(tServer.joinable()) tServer.join();
}