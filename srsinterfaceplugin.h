//==============================================================================
/**
@file       SRSInterfacePlugin.h

@brief      CPU plugin

@copyright  (c) 2018, Corsair Memory, Inc.
			This source code is licensed under the MIT-style license found in the LICENSE file.

**/
//==============================================================================

#include "Common/ESDBasePlugin.h"
#include <mutex>
#include <thread>
#include <atomic>



class SRSInterfacePlugin : public ESDBasePlugin
{
public:
	
	SRSInterfacePlugin();
	virtual ~SRSInterfacePlugin();
	
	void KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void TitleParametersDidChange(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID) override;

	void SendToPlugin(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID) override;
	
	void WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	void WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID) override;
	
	void DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo) override;
	void DeviceDidDisconnect(const std::string& inDeviceID) override;

	void UpdateSRSData(class SRSData* srsData);


private:

    const int serverStopDelayTime = 60;
	std::mutex mVisibleContextsMutex;
	std::map < std::string, class SRSActionSettings > mVisibleContexts;
	class SRSInterface* srs_server = nullptr;
	json prevJsonData = 0;
	bool radioChange = false; // Update radios if there is a settings change
    
    void DrawContext(const std::string& context, unsigned int radio, class SRSData* srsData, const class SRSActionSettings& as);
	
};


/*
class SRSInterfacePlugin
{
private:
    std::mutex mVisibleContextsMutex;
    std::map<std::string, SRSActionSettings> visibleContexts;
    std::thread tSenderNameShowTimer;
    std::atomic<bool> stopSenderNameShowTimer;
    std::condition_variable cvStopSenderNameShowTimer;
    std::chrono::seconds timeToWait;

public:
    void IsAppearing(const std::string& context)
    {
        std::scoped_lock lock(mVisibleContextsMutex);
        SRSActionSettings newSetting;
        visibleContexts.insert(std::make_pair(context, newSetting));
        if (visibleContexts.size() == 1)
        {
            stopSenderNameShowTimer = false;
            if (!tSenderNameShowTimer.joinable()) tSenderNameShowTimer = std::thread(&SRSInterfacePlugin::SenderNameShowTimer, this);
        }
    }

    void IsDisappearing(const std::string& context)
    {
        std::cout << "Disappearing before lock\n";
        std::scoped_lock lock(mVisibleContextsMutex);
        visibleContexts.erase(context);
        std::cout << "Disappering after lock\n";
        if (visibleContexts.size() == 0)
        {
            stopSenderNameShowTimer = true;
            cvStopSenderNameShowTimer.notify_all();
            std::cout << "Notifying done in disappearing, joining.\n";
            if (tSenderNameShowTimer.joinable()) tSenderNameShowTimer.join();
            std::cout << "Joining done\n";
        }
    }

    void SenderNameShowTimer()
    {
        std::mutex localMutex;
        std::unique_lock<std::mutex> lock(localMutex);
        while (!stopSenderNameShowTimer)
        {
            if (cvStopSenderNameShowTimer.wait_for(lock, timeToWait) == std::cv_status::timeout)
            {
                std::scoped_lock lock(mVisibleContextsMutex);
                std::cout << "Timer\n";
                for (auto& it : visibleContexts)
                {
                    //Do something
                }
            }
        }
    }

    SRSInterfacePlugin()
    {
        timeToWait = std::chrono::seconds(1);
    }
};


*/
