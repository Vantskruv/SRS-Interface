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

class SRSActionSettings
{
	public:
		static const int SHOW_ONLY_RADIO_NAME_AND_FREQUENCY = 0;
		static const int SHOW_SENDER_NAME_ON_TOP = 1;
		static const int SHOW_ONLY_SENDER_NAME = 2;
		static const int SHOW_ONLY_SENDER_NAME_AND_KEEP = 3;

		int radioSlot;
		
		// Radio appearance settings 
		bool showSelectedRadio = false;
		int radioAppearance = SHOW_ONLY_RADIO_NAME_AND_FREQUENCY;
		bool showOnlyBlackBackground = false;
		
		// Special sender name settings
		std::string regexString;
		bool removeAllSpacesInSenderName = false;
		bool removeSpecialCharactersInSenderName = false;
		int senderNameLineBreakage = 5;

		std::string latestSenderOnRadio;
};

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

	void UpdateData(const json& jsonData);
	void CallBackFunction(const json& jsonData);

private:
	
	std::mutex mVisibleContextsMutex;
	//std::map < std::string, std::pair<unsigned int, bool> > mVisibleContexts;
	std::map < std::string, SRSActionSettings > mVisibleContexts;
	class SRSInterface* srs_server = nullptr;
	json prevJsonData = 0;
	bool radioChange = false; // Update radios if there is a settings change

	void SetRadioToContext(const std::string& context, const std::string& str, bool isSending, bool isRecieving, bool isSelected);
	void UpdateContext(std::pair<const std::string, SRSActionSettings>& itemContext, int currentRadio, bool isSending, int selectedRadio, const json& jsonRadios, const std::multimap<int, std::string>& radiosReceiving);

};
