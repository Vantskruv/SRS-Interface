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
#include <mutex>

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
static const std::string RADIO_CHANGE_FREQUENCY = "frequencyChange";
static const std::string SHOW_ONLY_BLACK_BACKGROUND = "showOnlyBlackBackground";
static const std::string SENDER_NAME_REGEX = "senderNameRegex";
static const std::string SENDER_NAME_REMOVE_SPACES = "removeSpaces";
static const std::string SENDER_NAME_REMOVE_SPECIAL_CHARACTERS = "removeSpecialCharacters";
static const std::string SENDER_NAME_LINE_BREAKAGE = "senderNameLineBreakage";
static const std::string RADIO_INCREMENTION = "radioIncremention";
static const std::string RADIO_APPEARANCE_SETTINGS = "radioAppearanceSettings";
static const std::string RADIO_APPEARANCE_ON_TRANSMISSION_SETTINGS = "radioAppearanceOnTransmissionSettings";

static const unsigned int TAG_INVALID =     0x00000000;
static const unsigned int TAG_STRING =      0x10000000;
static const unsigned int TAG_RADIONAME =   0x20000000;
static const unsigned int TAG_CLIENTS =     0x30000000;
static const unsigned int TAG_SENDERNAME =  0x40000000;
static const unsigned int TAG_FREQ =        0x50000000;
static const unsigned int TAG_FREQMHZ =     0x60000000;
static const unsigned int TAG_FREQKHZ =     0x70000000;
static const unsigned int TAG_FREQINDEX =   0x80000000;
static const unsigned int TAG_GETKEY =      0xF0000000;
static const unsigned int TAG_GETNUMBER =   0x0FFFFFFF;

static const int ERROR_FLAG_REGEX = 0x100;
static const int ERROR_FLAG_DEF_FIELD = 0x010;
static const int ERROR_FLAG_TRANS_FIELD = 0x001;


class SRSActionSettings
{
public:
	int radioSlot;
	
	std::vector<std::pair<unsigned int, std::string>> vLayout;
	std::vector<std::pair<unsigned int, std::string>> vLayoutTransmission;
	
	bool isFrequencySetter = false;
	double radioIncremention = 0.025;
	
	// Radio appearance settings 
	bool showSelectedRadio = false;
	bool showOnlyBlackBackground = false;
	
	// Special sender name settings
	std::string regexString;
	bool removeAllSpacesInSenderName = false;
	bool removeSpecialCharactersInSenderName = false;
	int senderNameLineBreakage = 5;

	int errorCode = 0;
	std::string latestSenderOnRadio;
	volatile unsigned int latestSenderTimer = 0;
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



int ResolveTag(const std::string& strTag)
{
	std::string str = strTag;
	std::transform(str.begin(), str.end(), str.begin(), ::toupper);
	if (strcmp(str.c_str(), "RADIONAME") == 0) return TAG_RADIONAME;
	else if (strcmp(str.c_str(), "CLIENTS") == 0) return TAG_CLIENTS;
	else if (strcmp(str.c_str(), "FREQ") == 0) return TAG_FREQ;
	else if (strcmp(str.c_str(), "FREQMHZ") == 0) return TAG_FREQMHZ;
	else if (strcmp(str.c_str(), "FREQKHZ") == 0) return TAG_FREQKHZ;
	else if (strcmp(str.substr(0, 10).c_str(), "SENDERNAME") == 0)
	{
		if (strlen(str.c_str()) < 11) return TAG_SENDERNAME;
		try
		{
			unsigned int senderNameTimer = std::stoul(strTag.substr(10)) | TAG_SENDERNAME;
			if (TAG_SENDERNAME == (senderNameTimer & TAG_GETKEY)) return senderNameTimer;
			else return TAG_INVALID;
		}
		catch (...)
		{
			return TAG_INVALID;
		}
	}
	else if (strcmp(str.substr(0, 9).c_str(), "FREQINDEX") == 0)
	{
		try
		{
			unsigned int freqIndex = std::stoul(str.substr(9)) | TAG_FREQINDEX;
			if (TAG_FREQINDEX == (freqIndex & TAG_GETKEY)) return freqIndex;
			else return TAG_INVALID;
		}
		catch (...)
		{
			return TAG_INVALID;
		}
	}
	
	return TAG_INVALID;
}

 bool ConstructLayout(const std::string& str, std::vector<std::pair<unsigned int, std::string>>& vLayout)
{
	if (str.empty()) return true;

	std::string strBuffer;
	std::string tagBuffer;
	bool tagActive = false;

	for (unsigned int i = 0; i < str.size(); i++)
	{

		if (str[i] == '\\')
		{
			if ((i + 1) < str.size() && (str[i+1] == ']' || str[i+1] == '['))
			{
				i++;
			}
		}
		else if (str[i] == '[' && !tagActive)
		{
			if (!strBuffer.empty())
			{
				vLayout.push_back(std::make_pair(TAG_STRING, strBuffer));
				strBuffer.clear();
			}

			tagActive = true;
			continue;
		}
		else if (str[i] == ']' && tagActive)
		{
			unsigned int tagType = ResolveTag(tagBuffer);
			if ((tagType & TAG_GETKEY) == TAG_INVALID)
			{
				vLayout.clear();
				return false;
			}

			vLayout.push_back(std::make_pair(tagType, ""));

			tagBuffer.clear();
			tagActive = false;
			continue;
		}

		if (tagActive) tagBuffer.push_back(str[i]);
		else strBuffer.push_back(str[i]);
	}

	if (!strBuffer.empty())
	{
		vLayout.push_back(std::make_pair(TAG_STRING, strBuffer));
	}

	return true;
}

 std::string parseSenderName(std::string str, bool removeAllSpaces, const std::string& regexString, bool removeSpecialCharacters, unsigned int lineBreakage)
 {
	 if (str.empty()) return "";

	 // Remove all space characters
	 if (removeAllSpaces) str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());

	 if (!regexString.empty())
	 {
		 // Removes all data between[], () and <> including the correspondingly brackets.
		 //std::regex regexExpression("\\[.*?\\]|\\(.*?\\)|\\<.*?\\>|\\{.*?\\}|\\|.*?\\|");
		 std::regex regexExpression(regexString);
		 str = std::regex_replace(str, regexExpression, "");
	 }//if

	 // Remove all characters that is not a..z/A..Z/1..9
	 if (removeSpecialCharacters)
	 {
		 str.erase(std::remove_if(str.begin(), str.end(), [](const char& element)
			 {
				 if (std::isalnum(element) || std::isspace(element)) return false;
				 return true;
			 }), str.end());
	 }

	 // Insert a line breakage every 5:th character
	 if (lineBreakage > 0)
	 {
		 for (auto it = str.begin(); (lineBreakage + 1) <= std::distance(it, str.end()); ++it)
		 {
			 std::advance(it, lineBreakage);
			 it = str.insert(it, '\n');
		 }
	 }

	 return str;
 }


std::string FreqToStr(double freq)
{
	freq = freq / 1000000.0;
	std::stringstream ss;
	ss << std::setprecision(3) << std::fixed << freq;
	return ss.str();
}


void SRSInterfacePlugin::DrawContext(const std::string& context, unsigned int radio, SRSData* srsData, const SRSActionSettings& as)
{
	std::string str;
	if (radio < 0 || radio >= srsData->radioList.size()) return;

	SRSRadioData& radioData = srsData->radioList[radio];
	SRSActionSettings& radioSettings = mVisibleContexts[context];


	const std::string* bgImage = &blackImage;
	if (radioSettings.showOnlyBlackBackground);
    else if (srsData->sendingRadio == radio)
	{
		bgImage = &greenImage;
	}
	else if (radioData.isRecieving)
	{
		bgImage = &redImage;
	}
	else if (srsData->selectedRadio == radio)
	{
		bgImage = &blueImage;
	}
	mConnectionManager->SetImage(*bgImage, context, kESDSDKTarget_HardwareAndSoftware);

	const std::vector<std::pair<unsigned int, std::string>>* vLayout = &as.vLayout;
	if (radioData.isRecieving && !as.vLayoutTransmission.empty()) vLayout = &as.vLayoutTransmission;

	for (auto& vI : *vLayout)
	{
		switch (vI.first & TAG_GETKEY)
		{
		case TAG_STRING:
			str.append(vI.second);
		break;
		case TAG_RADIONAME:
			str.append(radioData.radioName);
		break;
		case TAG_CLIENTS:
			str.append(std::to_string(radioData.tunedClients));
		break;
		case TAG_SENDERNAME:
			
			if (radioData.isRecieving && radioData.sentBy.size())
			{
				unsigned int timerSeconds = vI.first & TAG_GETNUMBER;
				if (timerSeconds > 0) radioData.sentByRemainingSeconds = timerSeconds;
				str.append(parseSenderName(radioData.sentBy, radioSettings.removeAllSpacesInSenderName, radioSettings.regexString, radioSettings.removeSpecialCharactersInSenderName, radioSettings.senderNameLineBreakage));
			}
			else if (radioData.sentByRemainingSeconds>0)
			{
				str.append(parseSenderName(radioData.sentBy, radioSettings.removeAllSpacesInSenderName, radioSettings.regexString, radioSettings.removeSpecialCharactersInSenderName, radioSettings.senderNameLineBreakage));
			}
		break;
		case TAG_FREQ:
			str.append(FreqToStr(radioData.frequency));
		break;
		case TAG_FREQMHZ:
		{
			std::string mhz = FreqToStr(radioData.frequency);
			size_t pointPos = mhz.find('.');
			str.append(mhz.substr(0, pointPos));
		}
		break;
		case TAG_FREQKHZ:
		{
			std::string khz = FreqToStr(radioData.frequency);
			size_t pointPos = khz.find('.');
			str.append(khz.substr(pointPos + 1));
		}
		break;
		case TAG_FREQINDEX:
		{
			std::string hzIndex = FreqToStr(radioData.frequency);
			size_t pointPos = hzIndex.find('.');
			unsigned int n = hzIndex.size() - (vI.first & TAG_GETNUMBER);
			if (n == pointPos) n++;
			str.append(hzIndex.substr(n, 1));
		}
		}//switch
	}//for

	if ((radioSettings.errorCode & ERROR_FLAG_REGEX) == ERROR_FLAG_REGEX)
	{
		mConnectionManager->SetTitle(str, context, kESDSDKTarget_HardwareOnly);
		mConnectionManager->SetTitle("ERROR\nREGEX", context, kESDSDKTarget_SoftwareOnly);
	}
	else if ((radioSettings.errorCode & ERROR_FLAG_DEF_FIELD) == ERROR_FLAG_DEF_FIELD)
	{
		mConnectionManager->SetTitle(str, context, kESDSDKTarget_HardwareOnly);
		mConnectionManager->SetTitle("ERROR\nDEF\nFIELD", context, kESDSDKTarget_SoftwareOnly);
	}
	else if ((radioSettings.errorCode & ERROR_FLAG_TRANS_FIELD) == ERROR_FLAG_TRANS_FIELD)
	{
		mConnectionManager->SetTitle(str, context, kESDSDKTarget_HardwareOnly);
		mConnectionManager->SetTitle("ERROR\nTRANS\nFIELD", context, kESDSDKTarget_SoftwareOnly);
	}
	else mConnectionManager->SetTitle(str, context, kESDSDKTarget_HardwareAndSoftware);
 }

SRSInterfacePlugin::SRSInterfacePlugin()
{
	srs_server = new SRSInterface(this);
}

SRSInterfacePlugin::~SRSInterfacePlugin()
{
	delete srs_server;
}


void SRSInterfacePlugin::UpdateSRSData(SRSData* srsData)
{
	std::scoped_lock lock(mVisibleContextsMutex);

	for (auto& itemContext : mVisibleContexts)
	{
		int currentRadio = itemContext.second.showSelectedRadio ? srsData->selectedRadio : itemContext.second.radioSlot;
		DrawContext(itemContext.first, currentRadio, srsData, itemContext.second);
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
		// DEBUG CODE ----------------------
		// srs_server->srsData->radioList[actionSettings.radioSlot].isRecieving = !srs_server->srsData->radioList[actionSettings.radioSlot].isRecieving;
		// ---------------------- DEBUG CODE

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

	if (inPayload.contains(RADIO_CHANGE_FREQUENCY))
	{
		mVisibleContexts[inContext].isFrequencySetter = GetJSON_Bool(inPayload, RADIO_CHANGE_FREQUENCY, false);
	}
	else if (inPayload.contains(RADIO_INCREMENTION))
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
	else if (inPayload.contains(RADIO_APPEARANCE_SETTINGS))
	{
		mVisibleContexts[inContext].vLayout.clear();
		std::string radioAppearanceSettings = GetJSON_String(inPayload, RADIO_APPEARANCE_SETTINGS, "");
		bool bOK = ConstructLayout(radioAppearanceSettings, mVisibleContexts[inContext].vLayout);
		if (!bOK) mVisibleContexts[inContext].errorCode = mVisibleContexts[inContext].errorCode | ERROR_FLAG_DEF_FIELD;
		else mVisibleContexts[inContext].errorCode = mVisibleContexts[inContext].errorCode & ~ERROR_FLAG_DEF_FIELD;
	}
	else if (inPayload.contains(RADIO_APPEARANCE_ON_TRANSMISSION_SETTINGS))
	{
		mVisibleContexts[inContext].vLayoutTransmission.clear();
		std::string radioAppearanceOnTransmissionSettings = GetJSON_String(inPayload, RADIO_APPEARANCE_ON_TRANSMISSION_SETTINGS, "");
		bool bOK = ConstructLayout(radioAppearanceOnTransmissionSettings, mVisibleContexts[inContext].vLayoutTransmission);
		if (!bOK) mVisibleContexts[inContext].errorCode = mVisibleContexts[inContext].errorCode | ERROR_FLAG_TRANS_FIELD;
		else mVisibleContexts[inContext].errorCode = mVisibleContexts[inContext].errorCode & ~ERROR_FLAG_TRANS_FIELD;
	}
	else if (inPayload.contains(RADIO_SLOT))
	{
		mVisibleContexts[inContext].radioSlot = GetJSON_Integer(inPayload, RADIO_SLOT, 1);
	}
	else if (inPayload.contains(SHOW_SELECTED_RADIO))
	{
		mVisibleContexts[inContext].showSelectedRadio = GetJSON_Bool(inPayload, SHOW_SELECTED_RADIO, false);
	}
	else if (inPayload.contains(SHOW_ONLY_BLACK_BACKGROUND))
	{
		mVisibleContexts[inContext].showOnlyBlackBackground = GetJSON_Bool(inPayload, SHOW_ONLY_BLACK_BACKGROUND, false);
	}
	else if (inPayload.contains(SENDER_NAME_REGEX))
	{
		mVisibleContexts[inContext].regexString = GetJSON_String(inPayload, SENDER_NAME_REGEX, "");
		try
		{
			std::regex regexCheck(mVisibleContexts[inContext].regexString);
			mVisibleContexts[inContext].errorCode = mVisibleContexts[inContext].errorCode & ~ERROR_FLAG_REGEX;
		}
		catch (const std::regex_error& e)
		{
			DebugPrint("REGEX ERROR: %s\n", e.what());
			mVisibleContexts[inContext].errorCode = mVisibleContexts[inContext].errorCode | ERROR_FLAG_REGEX;
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
	std::scoped_lock lock(mVisibleContextsMutex);
	if (mVisibleContexts.size() == 0)
	{
		srs_server->cancel_stop_server();
		srs_server->start_server();

		//stopSenderNameShowTimer = false;
		//if (!tSenderNameShowTimer.joinable()) tSenderNameShowTimer = std::thread(&SRSInterfacePlugin::SenderNameShowTimer, this);
	}

	SRSActionSettings srsActionSettings;
	json payloadSettings = inPayload["settings"];
		

	srsActionSettings.isFrequencySetter = GetJSON_Bool(payloadSettings, RADIO_CHANGE_FREQUENCY, false);
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
	
	std::string radioAppearanceSettings = GetJSON_String(payloadSettings, RADIO_APPEARANCE_SETTINGS, "");
	srsActionSettings.vLayout.clear();
	bool bOK = ConstructLayout(radioAppearanceSettings, srsActionSettings.vLayout);
	if (!bOK) srsActionSettings.errorCode = srsActionSettings.errorCode | ERROR_FLAG_DEF_FIELD;
	else srsActionSettings.errorCode = srsActionSettings.errorCode & ~ERROR_FLAG_DEF_FIELD;

	std::string radioAppearanceOnTransmissionSettings = GetJSON_String(payloadSettings, RADIO_APPEARANCE_ON_TRANSMISSION_SETTINGS, "");
	srsActionSettings.vLayoutTransmission.clear();
	bOK = ConstructLayout(radioAppearanceOnTransmissionSettings, srsActionSettings.vLayoutTransmission);
	if (!bOK) srsActionSettings.errorCode = srsActionSettings.errorCode | ERROR_FLAG_TRANS_FIELD;
	else srsActionSettings.errorCode = srsActionSettings.errorCode & ~ERROR_FLAG_TRANS_FIELD;

	srsActionSettings.radioSlot = GetJSON_Integer(payloadSettings, RADIO_SLOT, 1);
	srsActionSettings.showSelectedRadio = GetJSON_Bool(payloadSettings, SHOW_SELECTED_RADIO, false);
	srsActionSettings.showOnlyBlackBackground = GetJSON_Bool(payloadSettings, SHOW_ONLY_BLACK_BACKGROUND, false);
	srsActionSettings.removeAllSpacesInSenderName = GetJSON_Bool(payloadSettings, SENDER_NAME_REMOVE_SPACES, false);
	srsActionSettings.removeSpecialCharactersInSenderName = GetJSON_Bool(payloadSettings, SENDER_NAME_REMOVE_SPECIAL_CHARACTERS, false);
	srsActionSettings.senderNameLineBreakage = GetJSON_Integer(payloadSettings, SENDER_NAME_LINE_BREAKAGE, 5);


	srsActionSettings.regexString = GetJSON_String(payloadSettings, SENDER_NAME_REGEX, "");
	try
	{
		std::regex regexCheck(srsActionSettings.regexString);
		srsActionSettings.errorCode = srsActionSettings.errorCode & ~ERROR_FLAG_REGEX;
	}
	catch (const std::regex_error& e)
	{
		srsActionSettings.errorCode = srsActionSettings.errorCode | ERROR_FLAG_REGEX;
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
	std::scoped_lock lock(mVisibleContextsMutex);
	mVisibleContexts.erase(inContext);
	
	if (mVisibleContexts.size() == 0)
	{
		srs_server->stop_server_after(serverStopDelayTime);
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
