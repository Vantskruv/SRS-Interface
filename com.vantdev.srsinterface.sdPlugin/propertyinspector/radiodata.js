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

function loadConfiguration(payload)
{
	let showSelectedRadio = document.getElementById("showSelectedRadio");
	showSelectedRadio.checked = payload.selectedRadio;

	let radioSlot = document.getElementById("radioSlot");
	radioSlot.value = payload.radioSlot;
		
	let radioAppearanceSelected = payload.radioAppearanceOptions;
	let radioButtons = document.getElementsByName("rdio");
	radioButtons[radioAppearanceSelected].checked = true;

	let showOnlyBlackBackground = document.getElementById("showOnlyBlackBackground");
	showOnlyBlackBackground.checked = payload.showOnlyBlackBackground;
	
	let senderNameRegex = document.getElementById("senderNameRegex");
	senderNameRegex.value = payload.senderNameRegex;
	
	let removeSpaces = document.getElementById("removeSpaces");
	removeSpaces.checked = payload.removeSpaces;
	
	let removeSpecialCharacters = document.getElementById("removeSpecialCharacters");
	removeSpecialCharacters.checked = payload.removeSpecialCharacters;
		
	let senderNameLineBreakage = document.getElementById("senderNameLineBreakage");
	senderNameLineBreakage.value = payload.senderNameLineBreakage;
		
	
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