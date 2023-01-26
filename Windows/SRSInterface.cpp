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
	srsData = new SRSData;

	// Create a hint structure for the clientserver
	clientOutHint.sin_family = AF_INET;
	clientOutHint.sin_port = htons(SRSInterface::UDP_SEND_PORT);
	inet_pton(AF_INET, "127.0.0.1", &clientOutHint.sin_addr);

	// Create a hint structure for the server
	serverInHint.sin_addr.S_un.S_addr = ADDR_ANY;
	serverInHint.sin_family = AF_INET;
	serverInHint.sin_port = htons(SRSInterface::UDP_RECEIVE_PORT); // Convert from little to big endian
	serverInSocket = INVALID_SOCKET;
	clientOutSocket = INVALID_SOCKET;

	open_sockets();
}



SRSInterface::~SRSInterface()
{
	stop_server();
	close_sockets();
	delete srsData;
}

bool SRSInterface::open_sockets()
{
	std::lock_guard<std::mutex> lock(mServer);
	WSADATA data;
	WORD version = MAKEWORD(2, 2);
	int wsOK = WSAStartup(version, &data);

	if (wsOK != 0)
	{
		DebugPrint("ERROR: WSA startup failed in initialize_server(): %d", wsOK);
		return false;
	}

	serverInSocket = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (serverInSocket == INVALID_SOCKET)
	{
		DebugPrint("ERROR: Server socket failed in initialize_sockets(): %d", WSAGetLastError());
		WSACleanup();
		return false;
	}

	if (bind(serverInSocket, (sockaddr*)&serverInHint, sizeof(serverInHint)) == SOCKET_ERROR)
	{
		DebugPrint("ERROR: Server bind failed in initialize_sockets(): %d", WSAGetLastError());
		closesocket(serverInSocket);
		serverInSocket = INVALID_SOCKET;
		WSACleanup();
		return false;
	}

	clientOutSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (clientOutSocket == INVALID_SOCKET)
	{
		DebugPrint("ERROR: Client socket failed in initialize_sockets(): %d", WSAGetLastError());
		WSACleanup();
		return false;
	}

	return true;
}

void SRSInterface::close_sockets()
{
	std::lock_guard<std::mutex> lock(mServer);
	closesocket(clientOutSocket);
	clientOutSocket = INVALID_SOCKET;
	closesocket(serverInSocket);
	serverInSocket = INVALID_SOCKET;
	WSACleanup();
}


void SRSInterface::SelectRadio(int radio)
{
	std::lock_guard<std::mutex> lock(mServer);
	if (clientOutSocket == INVALID_SOCKET) return;

	//std::string s = "{Command: 0, RadioId: 1, Frequency: 144000000.0}\n";
	std::string s = "{Command: 1, RadioId: " + std::to_string(radio) + "}\n";
	int sendOK = sendto(clientOutSocket, s.c_str(), s.size() + 1, 0, (sockaddr*)&clientOutHint, sizeof(clientOutHint));
}

void SRSInterface::ChangeRadioFrequency(int radio, double frequency)
{
	std::lock_guard<std::mutex> lock(mServer);
	if (clientOutSocket == INVALID_SOCKET) return;

	
	//std::string s = "{Command: 0, RadioId: " + std::to_string(radio) + ", Frequency: " + std::to_string(frequency) + " }\n";
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(3) << "{Command: 0, RadioId: " << radio << ", Frequency: " << frequency << " }\n";
	std::string s = oss.str();
	DebugPrint("%f\n%s\n", frequency, s.c_str());
	int sendOK = sendto(clientOutSocket, s.c_str(), s.size() + 1, 0, (sockaddr*)&clientOutHint, sizeof(clientOutHint));
	DebugPrint("SendOK: %d\n", sendOK);
}

bool SRSInterface::GetRadioFrequency(int radio, double& frequency)
{
	std::lock_guard<std::mutex> lock(mSRSData);
	if (radio >= srsData->radioList.size()) return false;
	frequency = srsData->radioList[radio].frequency;

	return true;
}

int SRSInterface::GetSelectedRadio()
{
	std::lock_guard<std::mutex> lock(mSRSData);
	return srsData->selectedRadio;
}

void SRSInterface::start_server()
{
	std::lock_guard<std::mutex> lock(mServer);
	if (isServerThreadUpdating) return;

	tServer = std::thread(&SRSInterface::server_thread, this);
	while (!isServerThreadUpdating) std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void SRSInterface::stop_server()
{
	std::lock_guard<std::mutex> lock(mServer);
	isServerThreadUpdating = false;
	if(tServer.joinable()) tServer.join();
}

void SRSInterface::server_thread()
{
	isServerThreadUpdating = true;

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

	delete [] recieveBuffer;
	return;
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

		if (srsInterfacePlugin)
		{
			try
			{
				const json jsonBuffer = json::parse(recieveBuffer);
				//srsInterfacePlugin->UpdateData(jsonBuffer);
				std::lock_guard<std::mutex> lock(mSRSData);
				construct_safe_data(jsonBuffer, srsData);
				srsInterfacePlugin->UpdateSRSData(srsData);
			}
			catch (json::parse_error& e)
			{
				DebugPrint(e.what());
			}
		}
	}

	delete [] recieveBuffer;
}

/*
int SRSInterface::server_thread_deprecated()
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
	serverInSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (bind(serverInSocket, (sockaddr*)&serverInHint, sizeof(serverInHint)) == SOCKET_ERROR)
	{
		return WSAGetLastError();
	}

	sockaddr_in client;
	int clientSize = sizeof(client);

	int RECIEVE_BUFFER_SIZE = 65507;
	char* recieveBuffer = new char[RECIEVE_BUFFER_SIZE];

	


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
				//srsInterfacePlugin->UpdateData(jsonBuffer);
				construct_safe_data(jsonBuffer, srsData);
				srsInterfacePlugin->UpdateSRSData(srsData);
			}
			catch (json::parse_error& e)
			{
				DebugPrint(e.what());
			}
		}
	}

	if (serverInSocket!= 0) closesocket(serverInSocket);
	serverInSocket = 0;
	WSACleanup();

	return 0;
}


int SRSInterface::start_server_deprecated()
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
	if (tServer.joinable()) tServer.join();
	if (serverInSocket != INVALID_SOCKET) closesocket(serverInSocket);
	serverInSocket = INVALID_SOCKET;
	close_client_socket();
}

void SRSInterface::stop_server_deprecated()
{
	std::lock_guard<std::mutex> lock(mServer);
	isServerThreadUpdating = false;
	if (serverInSocket != 0) closesocket(serverInSocket);
	if(tServer.joinable()) tServer.join();
}
*/



bool SRSInterface::construct_safe_data(const json& jsonData, SRSData* data)
{
	if (!jsonData.contains("/RadioInfo/radios"_json_pointer)) return false;
	if(!jsonData["RadioInfo"]["radios"].is_array()) return false;

	data->radioList.clear();
	for (auto itRadio : jsonData["RadioInfo"]["radios"])
	{
		SRSRadioData newRadio;
		try
		{
			newRadio.frequency = itRadio["freq"];
			newRadio.radioName = itRadio["name"];
			data->radioList.push_back(newRadio);
		}
		catch (...)
		{
			data->radioList.clear();
			return false;
		}
	}

	data->selectedRadio = 0;
	if (jsonData["RadioInfo"].contains("selected"))
	{
		try
		{
			data->selectedRadio = jsonData["RadioInfo"]["selected"];
		}
		catch (...)
		{

		}
	}

	if (jsonData.contains("RadioSendingState") && jsonData["RadioSendingState"].contains("IsSending") && jsonData["RadioSendingState"].contains("SendingOn"))
	{
		if (jsonData["RadioSendingState"]["IsSending"].is_boolean())
		{
			bool isSending = jsonData["RadioSendingState"]["IsSending"];
			if (isSending)
			{
				if (jsonData["RadioSendingState"]["SendingOn"].is_number())
				{
					data->sendingRadio = jsonData["RadioSendingState"]["SendingOn"];
				}
				else
				{
					data->sendingRadio = -1;
				}
			}
			else data->sendingRadio = -1;
		}
	}

	if (jsonData.contains("RadioReceivingState") && jsonData["RadioReceivingState"].is_array())
	{
		for (auto itRecv : jsonData["RadioReceivingState"])
		{
			if (itRecv.contains("IsReceiving") && itRecv["IsReceiving"].is_boolean() && itRecv.contains("ReceivedOn") && itRecv["ReceivedOn"].is_number() && itRecv.contains("SentBy") && itRecv["SentBy"].is_string())
			{
				bool isRecieving = itRecv["IsReceiving"];
				int radioRecieving = itRecv["ReceivedOn"];
				std::string sentBy = itRecv["SentBy"];

				if (radioRecieving >= data->radioList.size() || radioRecieving<0) continue;

				data->radioList[radioRecieving].isRecieving = itRecv["IsReceiving"];
				data->radioList[radioRecieving].sentBy.push_back(sentBy);
			}
		}
	}

	if (jsonData.contains("TunedClients") && jsonData["TunedClients"].is_array() && jsonData["TunedClients"].size() == data->radioList.size())
	{
		unsigned int i = 0;
		for (auto itClients : jsonData["TunedClients"])
		{
			if (itClients.is_number()) data->radioList[i].tunedClients = itClients;
			i++;
		}
	}

	return true;
}