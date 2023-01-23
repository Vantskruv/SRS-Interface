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
#include <iomanip>
#include <regex>

#include "Common/ESDConnectionManager.h"
#include "Common/EPLJSONUtils.h"


// Button background images
static const std::string blackImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";
static const std::string redImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==";
static const std::string blueImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPj/HwADBwIAMCbHYQAAAABJRU5ErkJggg==";
static const std::string greenImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk6ND9DwAC+wG2XjynzAAAAABJRU5ErkJggg==";

// Keys for plugin settings.
static const std::string RADIO_SLOT = "radioSlot";
static const std::string SHOW_SELECTED_RADIO = "selectedRadio";
static const std::string RADIO_APPEARANCE = "radioAppearanceOptions";
static const std::string SHOW_ONLY_BLACK_BACKGROUND = "showOnlyBlackBackground";
static const std::string SENDER_NAME_REGEX = "senderNameRegex";
static const std::string SENDER_NAME_REMOVE_SPACES = "removeSpaces";
static const std::string SENDER_NAME_REMOVE_SPECIAL_CHARACTERS = "removeSpecialCharacters";
static const std::string SENDER_NAME_LINE_BREAKAGE = "senderNameLineBreakage";


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


	bool regexError = false;
	std::string latestSenderOnRadio;
};



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
	radiosRecv.clear();

	if (jsonData.contains("RadioReceivingState") && jsonData["RadioReceivingState"].is_array())
	{
		json recvData = jsonData["RadioReceivingState"];
		for (auto it : recvData)
		{
			if (!it.contains("IsReceiving") || !it.contains("ReceivedOn")) continue;
			try
			{
				bool isRecv = it["IsReceiving"];
				if (!isRecv) continue;
				int onRadio = it["ReceivedOn"];
				std::string senderName = (it.contains("SentBy") ? it["SentBy"] : "");

				radiosRecv.insert(std::make_pair(onRadio, senderName));
			}
			catch (...) {};
		}
	}
}


void parseString(std::string& str, bool removeAllSpaces, const std::string& regexString, bool removeSpecialCharacters, unsigned int lineBreakage)
{
	// Remove all space characters
	if(removeAllSpaces) str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());

	// Removes all data between[], () and <> including the correspondingly brackets.
	
	if (!regexString.empty())
	{
		try {
			//std::regex regexExpression("\\[.*?\\]|\\(.*?\\)|\\<.*?\\>|\\{.*?\\}|\\|.*?\\|");
			std::regex regexExpression(regexString);
			str = std::regex_replace(str, regexExpression, "");
		}
		catch (...)
		{
		}
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
}

void parseRadioData(std::string& str, const json& jsonRadios, int currentRadio)
{
	if (jsonRadios.size() > currentRadio && jsonRadios[currentRadio].contains("freq") && jsonRadios[currentRadio].contains("name"))
	{
		double freq = jsonRadios[currentRadio]["freq"];
		std::string name = jsonRadios[currentRadio]["name"];
		str = name + "\n" + ToContextFreqFormat(freq);
	}
	else
	{
		str = "-----\n---.\n---";
	}
}


void SRSInterfacePlugin::UpdateContext(std::pair<const std::string, SRSActionSettings>& itemContext, int currentRadio, bool isSending, int selectedRadio, const json& jsonRadios, const std::multimap<int, std::string>& radiosReceiving)
{
	std::string str;
	SRSActionSettings& as = itemContext.second;

	unsigned int radiosReceivingCount = radiosReceiving.count(currentRadio);

	if (radiosReceivingCount>0)
	{
		str = radiosReceiving.lower_bound(currentRadio)->second;
		parseString(str, as.removeAllSpacesInSenderName, as.regexString, as.removeSpecialCharactersInSenderName, as.senderNameLineBreakage);
		as.latestSenderOnRadio = str;

		if (as.radioAppearance == SRSActionSettings::SHOW_ONLY_RADIO_NAME_AND_FREQUENCY)
		{
			parseRadioData(str, jsonRadios, currentRadio);
		}
		else  if (radiosReceivingCount > 1)
		{
			str = std::to_string(radiosReceivingCount) + "x\n#####\n#####";
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
			parseRadioData(str, jsonRadios, currentRadio);
		}
	}
		

	SetRadioToContext(itemContext.first, str, isSending && !as.showOnlyBlackBackground, radiosReceivingCount>0 && !as.showOnlyBlackBackground, currentRadio == selectedRadio && !as.showOnlyBlackBackground, as.regexError);
}



void SRSInterfacePlugin::UpdateData(const json& jsonData)
{
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);
	if (mConnectionManager == nullptr || mVisibleContexts.size()==0) return;
	if(jsonData!=prevJsonData || radioChange)
	{
		radioChange = false;

		int selectedRadio = 0;
		bool isSending = false;
		int sendingOn = 0;
		json jsonRadios = nlohmann::json::array();

		std::multimap<int, std::string> radiosReceiving;
		GetRadioRecieveData(jsonData, radiosReceiving);

		try
		{

			if (jsonData.contains("/RadioInfo/selected"_json_pointer))
			{
				selectedRadio = jsonData["RadioInfo"]["selected"];
			}

			if (jsonData.contains("/RadioSendingState/IsSending"_json_pointer) && jsonData.contains("/RadioSendingState/SendingOn"_json_pointer))
			{
				isSending = jsonData["RadioSendingState"]["IsSending"];
				sendingOn = jsonData["RadioSendingState"]["SendingOn"];
			}

			if (jsonData.contains("/RadioInfo/radios"_json_pointer) && jsonData["RadioInfo"]["radios"].is_array())
			{
				jsonRadios = jsonData["RadioInfo"]["radios"];
			}

			for (auto itemContext : mVisibleContexts)
			{
				int currentRadio = itemContext.second.showSelectedRadio ? selectedRadio : itemContext.second.radioSlot;

				if (currentRadio >= jsonRadios.size())
				{
					SetRadioToContext("-----", "---./n---", false, false, false, itemContext.second.regexError);
				}
				else
				{
					UpdateContext(itemContext, currentRadio, sendingOn == currentRadio && isSending, selectedRadio, jsonRadios, radiosReceiving);
				}
			}

			prevJsonData = jsonData;
		}
		catch (...) {};
	}
	
}

void SRSInterfacePlugin::KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Nothing to do
}

void SRSInterfacePlugin::KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);
	
	if (mVisibleContexts[inContext].showSelectedRadio) return;	// No need to select the radio when it is always selected
	srs_server->SelectRadio(mVisibleContexts[inContext].radioSlot);
	radioChange = true;
}


void SRSInterfacePlugin::TitleParametersDidChange(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID)
{
	
}


void SRSInterfacePlugin::SendToPlugin(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID)
{
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);
	try{
	if (inPayload.contains(RADIO_SLOT))
	{
		mVisibleContexts[inContext].radioSlot = inPayload[RADIO_SLOT];
	}
	else if (inPayload.contains(SHOW_SELECTED_RADIO))
	{
		mVisibleContexts[inContext].showSelectedRadio = inPayload[SHOW_SELECTED_RADIO];
	}
	else if (inPayload.contains(RADIO_APPEARANCE))
	{
		mVisibleContexts[inContext].radioAppearance = inPayload[RADIO_APPEARANCE];
	}
	else if (inPayload.contains(SHOW_ONLY_BLACK_BACKGROUND))
	{
		mVisibleContexts[inContext].showOnlyBlackBackground = inPayload[SHOW_ONLY_BLACK_BACKGROUND];
	}
	else if (inPayload.contains(SENDER_NAME_REGEX))
	{
		mVisibleContexts[inContext].regexString = inPayload[SENDER_NAME_REGEX];
		try
		{
			std::regex regexCheck(mVisibleContexts[inContext].regexString);
			mVisibleContexts[inContext].regexError = false;
		}
		catch (...)
		{
			mVisibleContexts[inContext].regexError = true;
			mConnectionManager->SetTitle("REGEX\nERROR", inContext, kESDSDKTarget_SoftwareOnly);
			mVisibleContexts[inContext].regexString = "";
		}
	}
	else if (inPayload.contains(SENDER_NAME_REMOVE_SPACES))
	{
		mVisibleContexts[inContext].removeAllSpacesInSenderName = inPayload[SENDER_NAME_REMOVE_SPACES];
	}
	else if (inPayload.contains(SENDER_NAME_REMOVE_SPECIAL_CHARACTERS))
	{
		mVisibleContexts[inContext].removeSpecialCharactersInSenderName = inPayload[SENDER_NAME_REMOVE_SPECIAL_CHARACTERS];
	}
	else if (inPayload.contains(SENDER_NAME_LINE_BREAKAGE))
	{
		mVisibleContexts[inContext].senderNameLineBreakage = inPayload[SENDER_NAME_LINE_BREAKAGE];
	}

	radioChange = true;
	}
	catch (...) {};
}

void SRSInterfacePlugin::WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Remember the context
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);

	if(mVisibleContexts.size() == 0) srs_server->start_server();

	SRSActionSettings srsActionSettings;
	
	if (inPayload["settings"].contains(RADIO_SLOT)) srsActionSettings.radioSlot = inPayload["settings"][RADIO_SLOT];
	if (inPayload["settings"].contains(SHOW_SELECTED_RADIO)) srsActionSettings.showSelectedRadio = inPayload["settings"][SHOW_SELECTED_RADIO];
	if (inPayload["settings"].contains(RADIO_APPEARANCE)) srsActionSettings.radioAppearance = inPayload["settings"][RADIO_APPEARANCE];
	if (inPayload["settings"].contains(SHOW_ONLY_BLACK_BACKGROUND)) srsActionSettings.showOnlyBlackBackground = inPayload["settings"][SHOW_ONLY_BLACK_BACKGROUND];
	if (inPayload["settings"].contains(SENDER_NAME_REGEX))
	{
		srsActionSettings.regexError = false;
		srsActionSettings.regexString = inPayload["settings"][SENDER_NAME_REGEX];
		try
		{
			std::regex regexCheck(srsActionSettings.regexString);
		}
		catch (...)
		{
			srsActionSettings.regexError = true;
			mConnectionManager->SetTitle("REGEX\nERROR", inContext, kESDSDKTarget_SoftwareOnly);
			srsActionSettings.regexString = "";
		}
	}
	if (inPayload["settings"].contains(SENDER_NAME_REMOVE_SPACES)) srsActionSettings.removeAllSpacesInSenderName = inPayload["settings"][SENDER_NAME_REMOVE_SPACES];
	if (inPayload["settings"].contains(SENDER_NAME_REMOVE_SPECIAL_CHARACTERS)) srsActionSettings.removeSpecialCharactersInSenderName = inPayload["settings"][SENDER_NAME_REMOVE_SPECIAL_CHARACTERS];
	if (inPayload["settings"].contains(SENDER_NAME_LINE_BREAKAGE)) srsActionSettings.senderNameLineBreakage = inPayload["settings"][SENDER_NAME_LINE_BREAKAGE];
		
	
	mVisibleContexts.insert(std::make_pair(inContext, srsActionSettings));
	radioChange = true;
}

void SRSInterfacePlugin::WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Remove the context
	std::lock_guard<std::mutex> lock(mVisibleContextsMutex);
	mVisibleContexts.erase(inContext);
	if(mVisibleContexts.size() == 0) srs_server->stop_server();
}

void SRSInterfacePlugin::DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo)
{
	// Nothing to do
}

void SRSInterfacePlugin::DeviceDidDisconnect(const std::string& inDeviceID)
{
	// Nothing to do
}
