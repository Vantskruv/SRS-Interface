#pragma once
#include <string>
#include <mutex>


class SRSRadioData
{
	public:
		double frequency;
		std::string radioName;
		bool isRecieving = false;
		std::string sentBy;
		unsigned int tunedClients = 0;

		unsigned int sentByRemainingSeconds = 0;
};

class SRSData
{
	public:
		std::vector<SRSRadioData> radioList;
		unsigned int selectedRadio = 0;
		unsigned int sendingRadio = 0;
};

class SRSInterface
{
private:
	const std::string UDP_IP = "127.0.0.1";
	static const int UDP_SEND_PORT = 9040;
	static const int UDP_RECEIVE_PORT = 7082;
	sockaddr_in clientOutHint;
	sockaddr_in serverInHint;
	UINT_PTR clientOutSocket;
	UINT_PTR serverInSocket;
	int wsaCode;

	std::atomic<bool> isServerThreadUpdating = false;

	std::mutex mSRSData;
	//SRSData* srsData = nullptr;
	bool construct_safe_data(const json& jsonData, SRSData* data);

	std::condition_variable cvSenderNameShowTimer;
	std::thread tSenderNameShowTimer;
	std::atomic<bool> stopSenderNameShowTimer = false;
	std::chrono::seconds senderNameTimeToWait;
	void senderNameTimerThread();

	std::mutex mServer;
	void server_thread();
	std::thread tServer; // Running the server_thread()

	std::thread tStopServerDelay;
	std::atomic<int> stopServerTimer;
	std::condition_variable cvStopServerDelay;
	bool open_sockets();
	void close_sockets();
	void stop_server_delay();
	void stop_server();

	class SRSInterfacePlugin* srsInterfacePlugin = nullptr;	//Access to callback method UpdateData(const json& jsonData) in SRSInterfacePlugin

public:
	//static const std::string TUNED_CLIENTS;
	SRSData* srsData = nullptr;

	bool start_server();
	void stop_server_after(int seconds);
	void cancel_stop_server();

	void SelectRadio(int radio); // Set selected radio in SRS
	void ToggleGuard(int radio);
	void ChangeChannel(int radio, bool increase);
	void IncrementRadioFrequency(int radio, double frequency); // Set selected radio in SRS
	bool GetRadioFrequency(int radio, double& frequency);
	int GetSelectedRadio();
	
	SRSInterface(class SRSInterfacePlugin* _srsInterfacePlugin);
	~SRSInterface();
};


