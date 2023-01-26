//==============================================================================
/**
@file       SRSInterfacePlugin.cpp

@brief      CPU plugin

@copyright  (c) 2018, Corsair Memory, Inc.
			This source code is licensed under the MIT-style license found in the LICENSE file.

**/
//==============================================================================

#include "srsinterfaceplugin.h"
#include "Windows/SRSInterface.h"
#include <atomic>
#include <sstream>
#include <string>
#include <array>
#include <iomanip>
#include <regex>

#include "Common/ESDConnectionManager.h"
#include "Common/EPLJSONUtils.h"


// Button background images
static const std::string blackImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";
static const std::string redImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==";
//static const std::string blueImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPj/HwADBwIAMCbHYQAAAABJRU5ErkJggg==";
static const std::string greenImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk6ND9DwAC+wG2XjynzAAAAABJRU5ErkJggg==";
static const std::string blueImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAIAAAAlC+aJAAABhWlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw1AUhU9bpVIqDgYUEclQnSyIijhKFYtgobQVWnUweekfNGlIUlwcBdeCgz+LVQcXZ10dXAVB8AfE1cVJ0UVKvC8ptIjxwuN9nHfP4b37AH+jwlSzawJQNctIxWNiNrcqBl/hwwAEjCAkMVNPpBcz8Kyve+qmuovyLO++P6tXyZsM8InEc0w3LOIN4plNS+e8TyywkqQQnxOPG3RB4keuyy6/cS467OeZgpFJzRMLxGKxg+UOZiVDJZ4mjiiqRvn+rMsK5y3OaqXGWvfkLwzntZU012kNI44lJJCECBk1lFGBhSjtGikmUnQe8/APOf4kuWRylcHIsYAqVEiOH/wPfs/WLExNuknhGND9Ytsfo0BwF2jWbfv72LabJ0DgGbjS2v5qA5j9JL3e1iJHQN82cHHd1uQ94HIHGHzSJUNypAAtf6EAvJ/RN+WA/lsgtObOrXWO0wcgQ7NavgEODoGxImWve7y7p3Nu//a05vcDY+dyoSI9tCEAAAAJcEhZcwAALiMAAC4jAXilP3YAAAAHdElNRQfnARcUIDsImwEmAAAAGXRFWHRDb21tZW50AENyZWF0ZWQgd2l0aCBHSU1QV4EOFwAAAPhJREFUaN7t001OwlAUhuG3tz9cqkUMGhINYejMJbgCluESXYFLcCRDQjQhSkSqpbS9t44cOUHjQOL3jE9O8ibngIiIiIiIiIiIiIiIiIiIiMjOgl0Hwz6jK7Jz0lPsEZ2M+IC4S2iJEsIEE2MiTIQJCQyBgeBzfwstraf1eIdv8A2+xlU0Fa6k3lC/s80pXymeyB+Y3+JWPw0YTxhckJ2RnmCPsT2SjDj9Ru0vaKkLqpxyTflC8Uz+yHLK7OZLwHjC8JLeiMMh3QG2T9j5uxfjtpQrNkveFqznLO4Cru/3+gfMvj+xAhSgAAUoQAEKUIACFKCA/xvwAT8RQs4KcJHbAAAAAElFTkSuQmCC";

// Keys for plugin settings.
static const std::string RADIO_SLOT = "radioSlot";
static const std::string SHOW_SELECTED_RADIO = "selectedRadio";
static const std::string RADIO_APPEARANCE = "radioAppearanceOptions";
static const std::string SHOW_ONLY_BLACK_BACKGROUND = "showOnlyBlackBackground";
static const std::string SHOW_NUMBER_OF_CLIENTS = "showNumberOfClients";
static const std::string SENDER_NAME_REGEX = "senderNameRegex";
static const std::string SENDER_NAME_REMOVE_SPACES = "removeSpaces";
static const std::string SENDER_NAME_REMOVE_SPECIAL_CHARACTERS = "removeSpecialCharacters";
static const std::string SENDER_NAME_LINE_BREAKAGE = "senderNameLineBreakage";
static const std::string RADIO_INCREMENTION = "radioIncremention";


class SRSActionSettings
{
public:
	// Radio appearance values
	static const int SHOW_ONLY_RADIO_NAME_AND_FREQUENCY = 0;
	static const int SHOW_SENDER_NAME_ON_TOP = 1;
	static const int SHOW_ONLY_SENDER_NAME = 2;
	static const int SHOW_ONLY_SENDER_NAME_AND_KEEP = 3;

	int radioSlot;
	bool isFrequencySetter = false;

	double radioIncremention = 0.025;
	
	// Radio appearance settings 
	bool showSelectedRadio = false;
	int radioAppearance = SHOW_ONLY_RADIO_NAME_AND_FREQUENCY;
	bool showOnlyBlackBackground = false;
	bool showNumberOfClients = false;

	// Special sender name settings
	std::string regexString;
	bool removeAllSpacesInSenderName = false;
	bool removeSpecialCharactersInSenderName = false;
	int senderNameLineBreakage = 5;


	bool regexError = false;
	std::string latestSenderOnRadio;
};



std::string GetJSON_String(const nlohmann::json& jsonObject, const std::string& key, std::string defaultValue)
{
	try
	{
		if (!jsonObject.contains(key)) return defaultValue;
		std::string rValue = jsonObject[key];
		return rValue;
	}
	catch (nlohmann::json::exception& e)
	{
		DebugPrint("Error GetJSON_String: %s\n", e.what());
		return defaultValue;
	}
}

double GetJSON_Double(const nlohmann::json& jsonObject, const std::string& key, double defaultValue)
{
	try
	{
		if (!jsonObject.contains(key)) return defaultValue;
		double rValue = jsonObject[key];
		return rValue;
	}
	catch (nlohmann::json::exception& e)
	{
		DebugPrint("Error GetJSON_Double: %s\n", e.what());
		return defaultValue;
	}
}

int GetJSON_Integer(const nlohmann::json& jsonObject, const std::string& key, int defaultValue)
{
	try
	{
		if (!jsonObject.contains(key)) return defaultValue;
		int rValue = jsonObject[key];
		return rValue;
	}
	catch (nlohmann::json::exception& e)
	{
		DebugPrint("Error GetJSON_Integer: %s\n", e.what());
		return defaultValue;
	}
}

bool GetJSON_Bool(const nlohmann::json& jsonObject, const std::string& key, bool defaultValue)
{
	try
	{
		if (!jsonObject.contains(key)) return defaultValue;
		bool rValue = jsonObject[key];
		return rValue;
	}
	catch (nlohmann::json::exception& e)
	{
		DebugPrint("Error GetJSON_Bool: %s\n", e.what());
		return defaultValue;
	}
}


std::string ToContextFreqFormat(double freq)
{
	freq = freq / 1000000.0;
	std::stringstream ss;
	ss << std::setprecision(3) << std::fixed << freq;
	std::string rStr = ss.str();

	size_t pointPos = rStr.find('.');

	return rStr.substr(0, pointPos) + ".\n" + rStr.substr(pointPos + 1);
}

SRSInterfacePlugin::SRSInterfacePlugin()
{
	srs_server = new SRSInterface(this);
}

SRSInterfacePlugin::~SRSInterfacePlugin()
{
	delete srs_server;
}


void SRSInterfacePlugin::SetRadioToContext(const std::string& context, const std::string& str, bool isSending, bool isReceiving, bool isSelected, bool regexError)
{
	if (regexError)
	{
		mConnectionManager->SetTitle(str, context, kESDSDKTarget_HardwareOnly);
		mConnectionManager->SetTitle("REGEX\nERROR", context, kESDSDKTarget_SoftwareOnly);
	}
	else mConnectionManager->SetTitle(str, context, kESDSDKTarget_HardwareAndSoftware);

	const std::string* bgImage = &blackImage;
	if (isSending)
	{
		bgImage = &greenImage;
	}
	else if (isReceiving)
	{
		bgImage = &redImage;
	}
	else if (isSelected)
	{
		bgImage = &blueImage;
	}


	mConnectionManager->SetImage(*bgImage, context, kESDSDKTarget_HardwareAndSoftware);
}

void GetRadioRecieveData(const json& jsonData, std::multimap<int, std::string>& radiosRecv)
{
	if (jsonData.contains("RadioReceivingState") && jsonData["RadioReceivingState"].is_array())
	{
		try
		{
			json recvData = jsonData["RadioReceivingState"];
			for (auto it : recvData)
			{
				if (!it.contains("IsReceiving") || !it.contains("ReceivedOn")) continue;

				bool isRecv = it["IsReceiving"];
				if (!isRecv) continue;
				int onRadio = it["ReceivedOn"];
				std::string senderName = (it.contains("SentBy") ? it["SentBy"] : "");

				radiosRecv.insert(std::make_pair(onRadio, senderName));
			}
		}
		catch (const json::type_error & e)
		{
			DebugPrint("JSON Error in GetRadioReceieveData: %s\n", e.what());
			radiosRecv.clear();
		}
	}
}

int GetNumberOfClients(const json& jsonTunedClients, int currentRadio)
{
	try
	{
		if (currentRadio > jsonTunedClients.size()) return 0;

		int rTunedClients = jsonTunedClients[currentRadio];
		return rTunedClients;
	}
	catch (json::type_error& e)
	{
		DebugPrint("JSON Error in GetNumberOfClients: %s\n", e.what());
		return 0;
	}
}


std::string parseSenderName(std::string str, bool removeAllSpaces, const std::string& regexString, bool removeSpecialCharacters, unsigned int lineBreakage)
{
	// Remove all space characters
	if(removeAllSpaces) str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
	
	if (!regexString.empty())
	{
		// Removes all data between[], () and <> including the correspondingly brackets.
		//std::regex regexExpression("\\[.*?\\]|\\(.*?\\)|\\<.*?\\>|\\{.*?\\}|\\|.*?\\|");
		std::regex regexExpression(regexString);
		str = std::regex_replace(str, regexExpression, "");
	}//if

	// Remove all characters that is not a..z/A..Z/1..9
	if(removeSpecialCharacters)
	{
		str.erase(std::remove_if(str.begin(), str.end(), [](const char& element)
		{
			if (std::isalnum(element) || std::isspace(element)) return false;
			return true;
		}), str.end());
	}

	// Insert a line breakage every 5:th character
	if(lineBreakage>0)
	{
		for (auto it = str.begin(); (lineBreakage + 1) <= std::distance(it, str.end()); ++it)
		{
			std::advance(it, lineBreakage);
			it = str.insert(it, '\n');
		}
	}

	return str;
}


std::string parseSRSRadioData(const SRSRadioData& radioData, bool showTunedClients)
{
	if (showTunedClients)
	{
		return radioData.radioName + "\n[ " + std::to_string(radioData.tunedClients) + " ]";
	}

	return radioData.radioName + "\n" + ToContextFreqFormat(radioData.frequency);
}



void SRSInterfacePlugin::UpdateSRSData(const SRSData* srsData)
{
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);

	for (auto itemContext : mVisibleContexts)
	{
		if (itemContext.second.isFrequencySetter) continue;
		int currentRadio = itemContext.second.showSelectedRadio ? srsData->selectedRadio : itemContext.second.radioSlot;

		if (currentRadio >= srsData->radioList.size())
		{
			SetRadioToContext("-----", "---./n---", false, false, false, itemContext.second.regexError);
		}
		else
		{
			std::string str;
			const SRSRadioData& srsRadio = srsData->radioList[currentRadio];
			SRSActionSettings& as = itemContext.second;

			if (srsRadio.isRecieving)
			{
				str = parseSenderName(srsRadio.sentBy[0], as.removeAllSpacesInSenderName, as.regexString, as.removeSpecialCharactersInSenderName, as.senderNameLineBreakage);
				mVisibleContexts[itemContext.first].latestSenderOnRadio = str;

				if (as.radioAppearance == SRSActionSettings::SHOW_ONLY_RADIO_NAME_AND_FREQUENCY)
				{
					str = parseSRSRadioData(srsRadio, as.showNumberOfClients);
				}
				else if (srsRadio.sentBy.size() > 1)
				{
					str = std::to_string(srsRadio.sentBy.size()) + "x\n#####\n#####";
					//str = "#####\n#####\n#####";
				}
			}
			else
			{
				if (as.radioAppearance == SRSActionSettings::SHOW_ONLY_SENDER_NAME)
				{
					str = "";
				}
				else if (as.radioAppearance == SRSActionSettings::SHOW_ONLY_SENDER_NAME_AND_KEEP)
				{
					str = as.latestSenderOnRadio;
				}
				else
				{
					str = parseSRSRadioData(srsRadio, as.showNumberOfClients);
				}
			}
			
			SetRadioToContext(	itemContext.first, 
								str, 
								srsData->sendingRadio  == currentRadio && !as.showOnlyBlackBackground,
								srsRadio.isRecieving && !as.showOnlyBlackBackground,
								currentRadio == srsData->selectedRadio && !as.showOnlyBlackBackground,
								as.regexError);
			//UpdateContext(itemContext, currentRadio, srsData->sendingRadio == currentRadio, srsData->selectedRadio, jsonRadios, radiosReceiving, GetNumberOfClients(jsonTunedClients, currentRadio));
		}
	}
}




void SRSInterfacePlugin::KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Nothing to do
}

void SRSInterfacePlugin::KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);
	const SRSActionSettings& actionSettings = mVisibleContexts[inContext];

	if (actionSettings.isFrequencySetter)
	{
		// Increase or decrease radio frequency
		int radioChannel = (actionSettings.showSelectedRadio ? srs_server->GetSelectedRadio() : actionSettings.radioSlot);
		srs_server->ChangeRadioFrequency(radioChannel, actionSettings.radioIncremention);
	}
	else
	{
		if (actionSettings.showSelectedRadio) return;	// No need to select the radio when it is always selected
		srs_server->SelectRadio(actionSettings.radioSlot);
	}
	radioChange = true;
}


void SRSInterfacePlugin::TitleParametersDidChange(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID)
{
	
}


void SRSInterfacePlugin::SendToPlugin(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID)
{
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);

	if (inAction.compare("com.vantdev.srsinterface.setfreq") == 0)
	{
		// Set setfreq settings
		
		if (inPayload.contains(RADIO_SLOT))
		{
			mVisibleContexts[inContext].radioSlot = GetJSON_Integer(inPayload, RADIO_SLOT, 1);
		}
		else if (inPayload.contains(SHOW_SELECTED_RADIO))
		{
			mVisibleContexts[inContext].showSelectedRadio = GetJSON_Bool(inPayload, SHOW_SELECTED_RADIO, true);
		}
		else if(inPayload.contains(RADIO_INCREMENTION))
		{
			try
			{
				std::string radioIncremention = GetJSON_String(inPayload, RADIO_INCREMENTION, "0.025");
				mVisibleContexts[inContext].radioIncremention = std::stod(radioIncremention);
			}
			catch (std::invalid_argument& e)
			{
				DebugPrint("Error in radio incremention value: %s\n", e.what());
				mVisibleContexts[inContext].radioIncremention = 0.000;
			}
		}

		return;
	}


	if (inPayload.contains(RADIO_SLOT))
	{
		mVisibleContexts[inContext].radioSlot = GetJSON_Integer(inPayload, RADIO_SLOT, 1);
	}
	else if (inPayload.contains(SHOW_SELECTED_RADIO))
	{
		mVisibleContexts[inContext].showSelectedRadio = GetJSON_Bool(inPayload, SHOW_SELECTED_RADIO, false);
	}
	else if (inPayload.contains(RADIO_APPEARANCE))
	{
		mVisibleContexts[inContext].radioAppearance = GetJSON_Integer(inPayload, RADIO_APPEARANCE, 0);
	}
	else if (inPayload.contains(SHOW_ONLY_BLACK_BACKGROUND))
	{
		mVisibleContexts[inContext].showOnlyBlackBackground = GetJSON_Bool(inPayload, SHOW_ONLY_BLACK_BACKGROUND, false);
	}
	else if (inPayload.contains(SHOW_NUMBER_OF_CLIENTS))
	{
		mVisibleContexts[inContext].showNumberOfClients = GetJSON_Bool(inPayload, SHOW_NUMBER_OF_CLIENTS, false);
	}
	else if (inPayload.contains(SENDER_NAME_REGEX))
	{
		mVisibleContexts[inContext].regexString = GetJSON_String(inPayload, SENDER_NAME_REGEX, "");
		try
		{
			std::regex regexCheck(mVisibleContexts[inContext].regexString);
			mVisibleContexts[inContext].regexError = false;
		}
		catch (const std::regex_error& e)
		{
			DebugPrint("REGEX ERROR: %s\n", e.what());
			mVisibleContexts[inContext].regexError = true;
			mConnectionManager->SetTitle("REGEX\nERROR", inContext, kESDSDKTarget_SoftwareOnly);
			mVisibleContexts[inContext].regexString = "";
		}
	}
	else if (inPayload.contains(SENDER_NAME_REMOVE_SPACES))
	{
		mVisibleContexts[inContext].removeAllSpacesInSenderName = GetJSON_Bool(inPayload, SENDER_NAME_REMOVE_SPACES, false);
	}
	else if (inPayload.contains(SENDER_NAME_REMOVE_SPECIAL_CHARACTERS))
	{
		mVisibleContexts[inContext].removeSpecialCharactersInSenderName = GetJSON_Bool(inPayload, SENDER_NAME_REMOVE_SPECIAL_CHARACTERS, false);
	}
	else if (inPayload.contains(SENDER_NAME_LINE_BREAKAGE))
	{
		mVisibleContexts[inContext].senderNameLineBreakage = GetJSON_Integer(inPayload, SENDER_NAME_LINE_BREAKAGE, 0);
	}

	radioChange = true;
}

void SRSInterfacePlugin::WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Remember the context
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);
	if (mVisibleContexts.size() == 0)
	{
		srs_server->start_server();
	}

	SRSActionSettings srsActionSettings;
	json payloadSettings = inPayload["settings"];

	if (inAction.compare("com.vantdev.srsinterface.setfreq") == 0)
	{
		mConnectionManager->SetTitle("", inContext, kESDSDKTarget_HardwareAndSoftware);
		mConnectionManager->SetImage(blackImage, inContext, kESDSDKTarget_HardwareAndSoftware);
		// Load settings for setfreq action button
		srsActionSettings.isFrequencySetter = true;
		
		srsActionSettings.radioSlot = GetJSON_Integer(payloadSettings, RADIO_SLOT, 1);
		srsActionSettings.showSelectedRadio = GetJSON_Bool(payloadSettings, SHOW_SELECTED_RADIO, true);
		
		try
		{
			std::string radioIncremention = GetJSON_String(payloadSettings, RADIO_INCREMENTION, "0.025");
			srsActionSettings.radioIncremention = std::stod(radioIncremention);
		}
		catch (std::invalid_argument& e)
		{
			DebugPrint("Error in radio incremention value: %s\n", e.what());
			srsActionSettings.radioIncremention = 0.000;
		}

		mVisibleContexts.insert(std::make_pair(inContext, srsActionSettings));
		radioChange = true;

		return;
	}


	srsActionSettings.radioSlot = GetJSON_Integer(payloadSettings, RADIO_SLOT, 1);
	srsActionSettings.showSelectedRadio = GetJSON_Bool(payloadSettings, SHOW_SELECTED_RADIO, false);
	srsActionSettings.radioAppearance = GetJSON_Integer(payloadSettings, RADIO_APPEARANCE, 0);
	srsActionSettings.showOnlyBlackBackground = GetJSON_Bool(payloadSettings, SHOW_ONLY_BLACK_BACKGROUND, false);
	srsActionSettings.showNumberOfClients = GetJSON_Bool(payloadSettings, SHOW_NUMBER_OF_CLIENTS, false);
	srsActionSettings.removeAllSpacesInSenderName = GetJSON_Bool(payloadSettings, SENDER_NAME_REMOVE_SPACES, false);
	srsActionSettings.removeSpecialCharactersInSenderName = GetJSON_Bool(payloadSettings, SENDER_NAME_REMOVE_SPECIAL_CHARACTERS, false);
	srsActionSettings.senderNameLineBreakage = GetJSON_Integer(payloadSettings, SENDER_NAME_LINE_BREAKAGE, 5);


	srsActionSettings.regexString = GetJSON_String(payloadSettings, SENDER_NAME_REGEX, "");
	try
	{
		std::regex regexCheck(srsActionSettings.regexString);
		srsActionSettings.regexError = false;
	}
	catch (const std::regex_error& e)
	{
		srsActionSettings.regexError = true;
		DebugPrint("REGEX ERROR: %s\n", e.what());
		mConnectionManager->SetTitle("REGEX\nERROR", inContext, kESDSDKTarget_SoftwareOnly);
		srsActionSettings.regexString = "";
	}


	
	mVisibleContexts.insert(std::make_pair(inContext, srsActionSettings));
	radioChange = true;
}

void SRSInterfacePlugin::WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Remove the context
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);
	mVisibleContexts.erase(inContext);
	if (mVisibleContexts.size() == 0)
	{
		//srs_server->stop_server();
	}
}

void SRSInterfacePlugin::DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo)
{
	// Nothing to do
}

void SRSInterfacePlugin::DeviceDidDisconnect(const std::string& inDeviceID)
{
	// Nothing to do
}
