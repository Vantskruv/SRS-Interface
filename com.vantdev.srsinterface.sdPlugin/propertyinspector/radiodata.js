let websocket = null;
let piContext = null;
let actionInfo = {};
let settings = {};
	
function connectElgatoStreamDeckSocket(inPort, inPropertyInspectorUUID, inRegisterEvent, inInfo, inActionInfo)
{
	if(websocket)
	{
		websocket.close();
		websocket = null;
	}
	
	piContext = inPropertyInspectorUUID;
	actionInfo = JSON.parse(inActionInfo);
	websocket = new WebSocket('ws://localhost:' + inPort);
	
	websocket.onopen = function() {
		let json = {
			event: inRegisterEvent,
			uuid: inPropertyInspectorUUID
		};

		websocket.send(JSON.stringify(json));
	};
	
	
	websocket.onmessage = function(evt)
	{
		// do something with the messages
	};
	
	window.addEventListener("beforeunload", function(e)
	{
		e.preventDefault();
	});

	loadConfiguration(actionInfo.payload.settings);
}

function getProperty(object, property, defaultValue) {
	if (object[property] != undefined) return object[property];
	return defaultValue;
}

function loadConfiguration(payload)
{
	let showSelectedRadio = document.getElementById("showSelectedRadio");
	showSelectedRadio.checked = getProperty(payload, "selectedRadio", false);

	let frequencyChange = document.getElementById("frequencyChange");
	frequencyChange.checked = getProperty(payload, "frequencyChange", false);

	let radioSlot = document.getElementById("radioSlot");
	radioSlot.value = getProperty(payload, "radioSlot", 1);

	let radioAppearanceSettings = document.getElementById("radioAppearanceSettings");
	radioAppearanceSettings.value = getProperty(payload, "radioAppearanceSettings", "");

	let radioAppearanceOnTransmissionSettings = document.getElementById("radioAppearanceOnTransmissionSettings");
	radioAppearanceOnTransmissionSettings.value = getProperty(payload, "radioAppearanceOnTransmissionSettings", "");


	let showOnlyBlackBackground = document.getElementById("showOnlyBlackBackground");
	showOnlyBlackBackground.checked = getProperty(payload, "showOnlyBlackBackground", false)
	
	let senderNameRegex = document.getElementById("senderNameRegex");
	senderNameRegex.value = getProperty(payload, "senderNameRegex", "");
	
	let removeSpaces = document.getElementById("removeSpaces");
	removeSpaces.checked = getProperty(payload, "removeSpaces", false);
	
	let removeSpecialCharacters = document.getElementById("removeSpecialCharacters");
	removeSpecialCharacters.checked = getProperty(payload, "removeSpecialCharacters", false);
		
	let senderNameLineBreakage = document.getElementById("senderNameLineBreakage");
	senderNameLineBreakage.value = getProperty(payload, "senderNameLineBreakage", 5);
	
	if(showSelectedRadio.checked)
	{
		let defaultOptionsGroup = document.getElementById("defaultOptionsGroup");
		defaultOptionsGroup.style.display = "none";
	}
	else
	{
		let defaultOptionsGroup = document.getElementById("defaultOptionsGroup");
		defaultOptionsGroup.style.display = "block";
	}

	if (frequencyChange.checked) {
		let frequencyChangeGroup = document.getElementById("frequencyChangeGroup");
		frequencyChangeGroup.style.display = "block";
	}
	else {
		let frequencyChangeGroup = document.getElementById("frequencyChangeGroup");
		frequencyChangeGroup.style.display = "none";
	}
}


// Tells our plugin that a setting has changed, and we also store the setting.
function sendValueToPlugin(value, param)
{
	actionInfo.payload.settings[param] = value;

	if (param == "selectedRadio") {
		if (value) {
			let defaultOptionsGroup = document.getElementById("defaultOptionsGroup");
			defaultOptionsGroup.style.display = "none";
		}
		else {
			let defaultOptionsGroup = document.getElementById("defaultOptionsGroup");
			defaultOptionsGroup.style.display = "block";
		}
	}

	if (param == "frequencyChange") {
		if (value) {
			let frequencyChangeGroup = document.getElementById("frequencyChangeGroup");
			frequencyChangeGroup.style.display = "block";
		}
		else {
			let frequencyChangeGroup = document.getElementById("frequencyChangeGroup");
			frequencyChangeGroup.style.display = "none";
		}
	}

	if (websocket && (websocket.readyState === 1))
	{
		const json = {
				"action": actionInfo['action'],
				"event": "sendToPlugin",
				"context": piContext,
				"payload": {
					[param] : value 
				}
		 };
		websocket.send(JSON.stringify(json));

		const jsonSettings = {
			'event': 'setSettings',
			'context': piContext,
			'payload': actionInfo.payload.settings
		};
		websocket.send(JSON.stringify(jsonSettings));
	}

	
}