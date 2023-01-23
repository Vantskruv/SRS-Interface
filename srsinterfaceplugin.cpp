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


static const std::string blackImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";
static const std::string redImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==";
static const std::string blueImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPj/HwADBwIAMCbHYQAAAABJRU5ErkJggg==";
static const std::string greenImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk6ND9DwAC+wG2XjynzAAAAABJRU5ErkJggg==";
static const std::string blueFrameImage = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEoAAABKCAYAAAAc0MJxAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsQAAA7EAZUrDhsAAADXSURBVHhe7dyxCsIwFEDR1jX//6mdq1JBLBqvs+dASObLe2vWMca+bdvC3OVx88V6O/vxZOYUSrNX9zwHqxcJFU1W7zl2/+V9AxMVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkVCRUJFQkV+Whrym8/PxMqOq0en5ioSKhkWa5avQ2J8T6lDwAAAABJRU5ErkJggg==";

static const std::string RADIO_SLOT = "radioSlot";
static const std::string SHOW_SELECTED_RADIO = "selectedRadio";
static const std::string RADIO_APPEARANCE = "radioAppearanceOptions";
static const std::string SHOW_ONLY_BLACK_BACKGROUND = "showOnlyBlackBackground";
static const std::string SENDER_NAME_REGEX = "senderNameRegex";
static const std::string SENDER_NAME_REMOVE_SPACES = "removeSpaces";
static const std::string SENDER_NAME_REMOVE_SPECIAL_CHARACTERS = "removeSpecialCharacters";
static const std::string SENDER_NAME_LINE_BREAKAGE = "senderNameLineBreakage";



std::string ToContextFreqFormat(double freq)
{
	freq = freq / 1000000.0;
	std::stringstream ss;
	ss << std::setprecision(3) << std::fixed << freq;
	std::string rStr = ss.str();

	size_t pointPos = rStr.find('.');

	return rStr.substr(0, pointPos) + ".\n" + rStr.substr(pointPos + 1);
}

void SRSInterfacePlugin::CallBackFunction(const json& jsonData)
{
	//obj->UpdateData(jsonData);
}

SRSInterfacePlugin::SRSInterfacePlugin()
{
	srs_server = new SRSInterface(this);
	//srs_server = new SRSInterface<SRSInterfacePlugin>(this, &this->CallBackFunction);
}

SRSInterfacePlugin::~SRSInterfacePlugin()
{
	delete srs_server;
}


void SRSInterfacePlugin::SetRadioToContext(const std::string& context, const std::string& str, bool isSending, bool isReceiving, bool isSelected)
{
	mConnectionManager->SetTitle(str, context, kESDSDKTarget_HardwareAndSoftware);

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

			bool isRecv = it["IsReceiving"];
			if (!isRecv) continue;
			int onRadio = it["ReceivedOn"];
			std::string senderName = (it.contains("SentBy") ? it["SentBy"] : "");

			radiosRecv.insert(std::make_pair(onRadio, senderName));
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
	std::string str = "-----\n---.\n---";
	SRSActionSettings& as = itemContext.second;

	unsigned int radiosReceivingCount = radiosReceiving.count(currentRadio);

	if (radiosReceivingCount>0)
	{
		str = radiosReceiving.lower_bound(currentRadio)->second;
		parseString(str, as.removeAllSpacesInSenderName, as.regexString, as.removeSpecialCharactersInSenderName, as.senderNameLineBreakage);
		itemContext.second.latestSenderOnRadio = str;

		if (as.radioAppearance == SRSActionSettings::SHOW_ONLY_RADIO_NAME_AND_FREQUENCY)
		{
			parseRadioData(str, jsonRadios, currentRadio);
		}
		else  if (radiosReceivingCount > 1)
		{
			str = std::to_string(radiosReceivingCount) + "x";
			str = str + "\n" + str + "\n" + str;
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
		

	SetRadioToContext(itemContext.first, str, isSending && !as.showOnlyBlackBackground, radiosReceivingCount>0 && !as.showOnlyBlackBackground, currentRadio == selectedRadio && !as.showOnlyBlackBackground);
}



void SRSInterfacePlugin::UpdateData(const json& jsonData)
{
	if (mConnectionManager == nullptr || mVisibleContexts.size()==0) return;
	if(jsonData!=prevJsonData || radioChange)
	{
		std::string str = "-----\n---.\n---";
		std::lock_guard<std::mutex> lock(mVisibleContextsMutex);
		radioChange = false;

		try
		{
			int selectedRadio = 0;
			if (jsonData.contains("/RadioInfo/selected"_json_pointer))
			{
				selectedRadio = jsonData["RadioInfo"]["selected"];
			}
				
			bool isSending = false;
			int sendingOn = 0;
			if (jsonData.contains("/RadioSendingState/IsSending"_json_pointer) && jsonData.contains("/RadioSendingState/SendingOn"_json_pointer))
			{
				isSending = jsonData["RadioSendingState"]["IsSending"];
				sendingOn = jsonData["RadioSendingState"]["SendingOn"];
			}

			json jsonRadios = nlohmann::json::array();
			if (jsonData.contains("/RadioInfo/radios"_json_pointer) && jsonData["RadioInfo"]["radios"].is_array())
			{
				jsonRadios = jsonData["RadioInfo"]["radios"];
			}

			std::multimap<int, std::string> radiosReceiving;
			GetRadioRecieveData(jsonData, radiosReceiving);

			for (auto itemContext : mVisibleContexts)
			{
				int currentRadio = itemContext.second.radioSlot;
				if (itemContext.second.showSelectedRadio)	// If selectedRadio is set for this context, we only show the selectedRadio for this context.
				{
					currentRadio = selectedRadio;
				}
				if (currentRadio >= jsonRadios.size())	// If radio index is greator than 
				{
					SetRadioToContext("-----", "---./n---", false, false, false);
				}
				else
				{
					UpdateContext(itemContext, currentRadio, sendingOn == currentRadio && isSending, selectedRadio, jsonRadios, radiosReceiving);
				}
			}

			prevJsonData = jsonData;
		}
		catch (std::exception e)
		{
			//str = "EXP!\n!!!.\n!!!";
			//str = e.what();
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
	if (inPayload.contains(RADIO_SLOT))
	{
		std::string sRadioSlot = inPayload[RADIO_SLOT];
		int radioSlot = 0;
		try
		{
			radioSlot = std::stoi(sRadioSlot);
			mVisibleContexts[inContext].radioSlot = radioSlot;
			radioChange = true;
		}
		catch (...)
		{
			return;
		};
	}
	else if (inPayload.contains(SHOW_SELECTED_RADIO))
	{
		mVisibleContexts[inContext].showSelectedRadio = inPayload[SHOW_SELECTED_RADIO];
		radioChange = true;
	}
	else if (inPayload.contains(RADIO_APPEARANCE))
	{
		//std::string sRadioAppearance = inPayload[RADIO_APPEARANCE];
		int radioAppearance = inPayload[RADIO_APPEARANCE];;
		try
		{
			//radioAppearance = std::stoi(sRadioAppearance);
			mVisibleContexts[inContext].radioAppearance = radioAppearance;
			radioChange = true;
		}
		catch (...)
		{
			return;
		};
	}
	else if (inPayload.contains(SHOW_ONLY_BLACK_BACKGROUND))
	{
		mVisibleContexts[inContext].showOnlyBlackBackground = inPayload[SHOW_ONLY_BLACK_BACKGROUND];
		radioChange = true;
	}
	else if (inPayload.contains(SENDER_NAME_REGEX))
	{
		mVisibleContexts[inContext].regexString = inPayload[SENDER_NAME_REGEX];
		radioChange = true;
	}
	else if (inPayload.contains(SENDER_NAME_REMOVE_SPACES))
	{
		mVisibleContexts[inContext].removeAllSpacesInSenderName = inPayload[SENDER_NAME_REMOVE_SPACES];
		radioChange = true;
	}
	else if (inPayload.contains(SENDER_NAME_REMOVE_SPECIAL_CHARACTERS))
	{
		mVisibleContexts[inContext].removeSpecialCharactersInSenderName = inPayload[SENDER_NAME_REMOVE_SPECIAL_CHARACTERS];
		radioChange = true;
	}
	else if (inPayload.contains(SENDER_NAME_LINE_BREAKAGE))
	{
		try
		{
			std::string sLineBreakage = inPayload[SENDER_NAME_LINE_BREAKAGE];
			int lineBreakage = 5;
			lineBreakage = std::stoi(sLineBreakage);
			mVisibleContexts[inContext].senderNameLineBreakage = lineBreakage;
			radioChange = true;
		}
		catch (...)
		{
			return;
		};
	}

	/*
	json settings;
	settings["radioSlot"] = mVisibleContexts[inContext].radioSlot;
	settings["selectedRadio"] = mVisibleContexts[inContext].showSelectedRadio;
	settings["showTransmitterName"] = mVisibleContexts[inContext].showTransmitterName;
	mConnectionManager->SetSettings(settings, inContext);
	*/

	json settings;
	settings[RADIO_SLOT] = mVisibleContexts[inContext].radioSlot;
	settings[SHOW_SELECTED_RADIO] = mVisibleContexts[inContext].showSelectedRadio;
	settings[RADIO_APPEARANCE] = mVisibleContexts[inContext].radioAppearance;
	settings[SHOW_ONLY_BLACK_BACKGROUND] = mVisibleContexts[inContext].showOnlyBlackBackground;
	settings[SENDER_NAME_REGEX] = mVisibleContexts[inContext].regexString;
	settings[SENDER_NAME_REMOVE_SPACES] = mVisibleContexts[inContext].removeAllSpacesInSenderName;
	settings[SENDER_NAME_REMOVE_SPECIAL_CHARACTERS] = mVisibleContexts[inContext].removeSpecialCharactersInSenderName;
	settings[SENDER_NAME_LINE_BREAKAGE] = mVisibleContexts[inContext].senderNameLineBreakage;
	mConnectionManager->SetSettings(settings, inContext);

}

void SRSInterfacePlugin::WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Remember the context
	mVisibleContextsMutex.lock();
	if(mVisibleContexts.size() == 0) srs_server->start_server();

	SRSActionSettings srsActionSettings;

	//if (inPayload["settings"].contains("radioSlot")) srsActionSettings.radioSlot = inPayload["settings"]["radioSlot"];
	//if (inPayload["settings"].contains("selectedRadio")) srsActionSettings.showSelectedRadio = inPayload["settings"]["selectedRadio"];
	//if (inPayload["settings"].contains("showTransmitterName")) srsActionSettings.showTransmitterName = inPayload["settings"]["showTransmitterName"];


	if (inPayload["settings"].contains(RADIO_SLOT)) srsActionSettings.radioSlot = inPayload["settings"][RADIO_SLOT];
	if (inPayload["settings"].contains(SHOW_SELECTED_RADIO)) srsActionSettings.showSelectedRadio = inPayload["settings"][SHOW_SELECTED_RADIO];
	if (inPayload["settings"].contains(RADIO_APPEARANCE)) srsActionSettings.radioAppearance = inPayload["settings"][RADIO_APPEARANCE];
	if (inPayload["settings"].contains(SHOW_ONLY_BLACK_BACKGROUND)) srsActionSettings.showOnlyBlackBackground = inPayload["settings"][SHOW_ONLY_BLACK_BACKGROUND];
	if (inPayload["settings"].contains(SENDER_NAME_REGEX)) srsActionSettings.regexString = inPayload["settings"][SENDER_NAME_REGEX];
	if (inPayload["settings"].contains(SENDER_NAME_REMOVE_SPACES)) srsActionSettings.removeAllSpacesInSenderName = inPayload["settings"][SENDER_NAME_REMOVE_SPACES];
	if (inPayload["settings"].contains(SENDER_NAME_REMOVE_SPECIAL_CHARACTERS)) srsActionSettings.removeSpecialCharactersInSenderName = inPayload["settings"][SENDER_NAME_REMOVE_SPECIAL_CHARACTERS];
	if (inPayload["settings"].contains(SENDER_NAME_LINE_BREAKAGE)) srsActionSettings.senderNameLineBreakage = inPayload["settings"][SENDER_NAME_LINE_BREAKAGE];


	//mVisibleContexts.insert(std::make_pair(inContext, std::make_pair(radioSlot, selectedRadio)));
	mVisibleContexts.insert(std::make_pair(inContext, srsActionSettings));
	radioChange = true;
	mVisibleContextsMutex.unlock();
}

void SRSInterfacePlugin::WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Remove the context
	mVisibleContextsMutex.lock();
	mVisibleContexts.erase(inContext);
	if(mVisibleContexts.size() == 0) srs_server->stop_server();
	mVisibleContextsMutex.unlock();
}

void SRSInterfacePlugin::DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo)
{
	// Nothing to do
}

void SRSInterfacePlugin::DeviceDidDisconnect(const std::string& inDeviceID)
{
	// Nothing to do
}
