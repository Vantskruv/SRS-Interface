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

function getProperty(object, property, defaultValue)
{
	if (object[property] != undefined) return object[property];
	return defaultValue;
}

function loadConfiguration(payload)
{
	let showSelectedRadio = document.getElementById("showSelectedRadio");
	showSelectedRadio.checked = getProperty(payload, "selectedRadio", true);

	let radioSlot = document.getElementById("radioSlot");
	radioSlot.value = getProperty(payload, "radioSlot", 1)

	let radioIncremention = document.getElementById("radioIncremention");
	radioIncremention.value = payload.radioIncremention;
	radioIncremention.value = getProperty(payload, "radioIncremention", "0.025");

	if (showSelectedRadio.checked) {
		let defaultOptionsGroup = document.getElementById("defaultOptionsGroup");
		defaultOptionsGroup.style.display = "none";
	}
	else {
		let defaultOptionsGroup = document.getElementById("defaultOptionsGroup");
		defaultOptionsGroup.style.display = "block";
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