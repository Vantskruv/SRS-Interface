#pragma once
#include <string>
#include <mutex>

//template <class T>
class SRSInterface
{
private:
	const std::string UDP_IP = "127.0.0.1";
	static const int UDP_SEND_PORT = 9040;
	static const int UDP_RECEIVE_PORT = 7082;
	UINT_PTR serverInSocket = 0;
	bool isServerThreadUpdating = true;

	int server_thread();
	std::thread tServer; // Running the server_thread()
	class SRSInterfacePlugin* srsInterfacePlugin = nullptr;	//Access to callback method UpdateData(const json& jsonData) in SRSInterfacePlugin

	std::mutex mServer;

public:
	int start_server();
	void stop_server();
	void SelectRadio(int radio); // Set selected radio in SRS
	
	SRSInterface(class SRSInterfacePlugin* _srsInterfacePlugin);
	~SRSInterface();
};