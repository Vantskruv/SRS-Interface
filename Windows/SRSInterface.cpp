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
	serverInHint = {};
	serverInHint.sin_addr.S_un.S_addr = ADDR_ANY;
	serverInHint.sin_family = AF_INET;
	serverInHint.sin_port = htons(SRSInterface::UDP_RECEIVE_PORT); // Convert from little to big endian
	serverInSocket = INVALID_SOCKET;
	clientOutSocket = INVALID_SOCKET;
	wsaCode = WSA_INVALID_HANDLE;

	senderNameTimeToWait = std::chrono::seconds(1);
}



SRSInterface::~SRSInterface()
{
	stop_server();
	delete srsData;
}

bool SRSInterface::open_sockets()
{
	if(wsaCode!=0)
	{
		WSADATA data;
		WORD version = MAKEWORD(2, 2);
		wsaCode = WSAStartup(version, &data);

		if (wsaCode != 0)
		{
			DebugPrint("ERROR: WSA startup failed in initialize_server(): %d", wsaCode);
			return false;
		}
	}

	//serverInSocket = socket(AF_INET, SOCK_DGRAM, 0);
	serverInSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//DWORD nonBlocking = 1;
	//ioctlsocket(serverInSocket, FIONBIO, &nonBlocking);
	
	BOOL bOptVal = TRUE;
	int bOptLen = sizeof(BOOL);
	if (setsockopt(serverInSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&bOptVal, bOptLen) == SOCKET_ERROR)
	{
		int wsaError = WSAGetLastError();
		DebugPrint("setsockopt error: %d\n", wsaError);
		WSACleanup();
		return false;
	}
	
	if (serverInSocket == INVALID_SOCKET)
	{
		DebugPrint("ERROR: Server socket failed in initialize_sockets(): %d", WSAGetLastError());
		wsaCode = WSA_INVALID_HANDLE;
		WSACleanup();
		return false;
	}

	if (bind(serverInSocket, (sockaddr*)&serverInHint, sizeof(serverInHint)) == SOCKET_ERROR)
	{
		DebugPrint("ERROR: Server bind failed in initialize_sockets(): %d", WSAGetLastError());
		closesocket(serverInSocket);
		serverInSocket = INVALID_SOCKET;
		wsaCode = WSA_INVALID_HANDLE;
		WSACleanup();
		return false;
	}

	clientOutSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (clientOutSocket == INVALID_SOCKET)
	{
		DebugPrint("ERROR: Client socket failed in initialize_sockets(): %d", WSAGetLastError());
		wsaCode = WSA_INVALID_HANDLE;
		WSACleanup();
		return false;
	}

	return true;
}

void SRSInterface::close_sockets()
{
	DebugPrint("CLOSING SOCKETS\n");
	closesocket(clientOutSocket);
	clientOutSocket = INVALID_SOCKET;
	closesocket(serverInSocket);
	serverInSocket = INVALID_SOCKET;
	if(wsaCode == 0) WSACleanup();
	wsaCode = WSA_INVALID_HANDLE;
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
	int sendOK = sendto(clientOutSocket, s.c_str(), s.size() + 1, 0, (sockaddr*)&clientOutHint, sizeof(clientOutHint));
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


/** 
 * Returns true if either server is already running, or if it successfully starrted up. 
 */
bool SRSInterface::start_server()
{
	std::lock_guard<std::mutex> lock(mServer);
	if (isServerThreadUpdating) return true;

	if (tServer.joinable())
	{
		close_sockets();
		tServer.join();
	}

	DebugPrint("START_SERVER: STARTING SERVER\n");
	if (!open_sockets()) return false;
	tServer = std::thread(&SRSInterface::server_thread, this);
	while (!isServerThreadUpdating) std::this_thread::sleep_for(std::chrono::milliseconds(10));
	
	if (!tSenderNameShowTimer.joinable()) tSenderNameShowTimer = std::thread(&SRSInterface::senderNameTimerThread, this);

	DebugPrint("START_SERVER: SERVER IS NOW STARTED\n");
	return true;
}

void SRSInterface::stop_server()
{
	//std::lock_guard<std::mutex> lock(mServer);
	DebugPrint("STOP_SERVER: STOPPING THE SERVER\n");
	isServerThreadUpdating = false;
	close_sockets();
	if(tServer.joinable()) tServer.join();

	stopSenderNameShowTimer = true;
	cvSenderNameShowTimer.notify_all();
	if (tSenderNameShowTimer.joinable()) tSenderNameShowTimer.join();
	DebugPrint("STOP_SERVER: SERVER IS STOPPED\n");
}


void SRSInterface::cancel_stop_server()
{
	cvStopServerDelay.notify_all();
	if (tStopServerDelay.joinable()) tStopServerDelay.join();
}

void SRSInterface::stop_server_after(int seconds)
{
	if(!tStopServerDelay.joinable()) tStopServerDelay = std::thread(&SRSInterface::stop_server_delay, this, seconds);
}

void SRSInterface::stop_server_delay(int seconds)
{
	std::unique_lock<std::mutex> lock(mServer);

	if (cvStopServerDelay.wait_for(lock, std::chrono::seconds(seconds)) == std::cv_status::timeout)
	{
		//srsInterfacePlugin->cvStopSenderNameShowTimerThread.notify_all();
		//if (srsInterfacePlugin->tSenderNameShowTimer.joinable()) srsInterfacePlugin->tSenderNameShowTimer.join();
		stop_server();
	}
}

void SRSInterface::server_thread()
{
	sockaddr_in client;
	int clientSize = sizeof(client);

	int RECIEVE_BUFFER_SIZE = 65507;
	char* recieveBuffer = new char[RECIEVE_BUFFER_SIZE];

	
	//DEBUG from file ----------------------------------
	/*
	std::ifstream fIn;
	fIn.open("jsondatatest.txt", std::ifstream::in);
	isServerThreadUpdating = true;
	if (fIn.is_open())
	{
		std::stringstream sBuffer;
		sBuffer << fIn.rdbuf();
		fIn.close();

		try
		{
			const json jsonBuffer = json::parse(sBuffer.str());
			construct_safe_data(jsonBuffer, srsData);
			while (isServerThreadUpdating)
			{
				Sleep(1000);
				std::scoped_lock lock(mSRSData);
				srsInterfacePlugin->UpdateSRSData(srsData);
			}
		}
		catch (...)
		{
		};
	}
	*/
	//---------------------- DEBUG FROM FILE
	

	isServerThreadUpdating = true;
	while (isServerThreadUpdating)
	{
		ZeroMemory(&client, clientSize);
		ZeroMemory(recieveBuffer, RECIEVE_BUFFER_SIZE);
		int bytesIn = recvfrom(serverInSocket, recieveBuffer, RECIEVE_BUFFER_SIZE, 0, (sockaddr*)&client, &clientSize);
		if (bytesIn == SOCKET_ERROR)
		{
			DebugPrint("SOCKET_ERROR: %d\n", WSAGetLastError());
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			continue;
		}

		if (srsInterfacePlugin)
		{
			try
			{
				const json jsonBuffer = json::parse(recieveBuffer);
				std::scoped_lock lock(mSRSData);
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
	isServerThreadUpdating = false;
}


void SRSInterface::senderNameTimerThread()
{
	std::mutex localMutex;
	std::unique_lock<std::mutex> lock(localMutex);
	DebugPrint("STARTING SENDER NAME SHOW TIMER\n");

	stopSenderNameShowTimer = false;
	while (!stopSenderNameShowTimer)
	{
		if (cvSenderNameShowTimer.wait_for(lock, senderNameTimeToWait) == std::cv_status::timeout)
		{
			std::scoped_lock lock(mSRSData);
			for (auto& it : srsData->radioList)
			{
				if (it.sentByRemainingSeconds == 0) continue;
				it.sentByRemainingSeconds--;
			}
		}
	}

	DebugPrint("SENDER NAME SHOW TIMER IS STOPPED\n");
}


bool SRSInterface::construct_safe_data(const json& jsonData, SRSData* data)
{
	if (!jsonData.contains("/RadioInfo/radios"_json_pointer)) return false;
	if(!jsonData["RadioInfo"]["radios"].is_array()) return false;

	data->radioList.resize(jsonData["RadioInfo"]["radios"].size());
	unsigned int i = 0;
	for (auto itRadio : jsonData["RadioInfo"]["radios"])
	{
		SRSRadioData newRadio;
		try
		{
			data->radioList[i].frequency = itRadio["freq"];
			data->radioList[i].radioName = itRadio["name"];
			i++;
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
				data->radioList[radioRecieving].sentBy = sentBy;
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