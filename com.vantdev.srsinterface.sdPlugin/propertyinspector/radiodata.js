let websocket = null;
let piContext = null;
let actionInfo = {};
	
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
		//sendValueToPlugin("propertyInspectorWillDisappear", "property_inspector");
		//sendValueToPlugin("mellan", "topp");
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
	
	/*
	if( radioAppearanceSelected < radioElements.length)
	{
		radioElements[radioAppearanceSelected].checked = true;
	}
	*/
	
	/*
	switch(radioAppearanceSelected)
	{
		case 0:
			let setChecked = document.getElementById("onlyRadioNameAndFrequency");
			setChecked.checked = true;
		break;
		case 1:
			let setChecked = document.getElementById("senderNameOnTop");
			setChecked.checked = true;
		break;
		case 2:
			let setChecked = document.getElementById("showOnlySenderName");
			setChecked.checked = true;
		break;
		case 3:
			let setChecked = document.getElementById("showOnlySenderNameAndKeep");
			setChecked.checked = true;
		break;
	}
	*/
	
	//radioAppearance[radioAppearanceSelected].checked = true;
	

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


function sendValueToPlugin(value, param)
{
	if(param == "selectedRadio")
	{
		if(value)
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

	if (websocket)
	{
		const json = {
				"action": actionInfo['action'],
				"event": "sendToPlugin",
				"context": piContext, // as received from the 'connectElgatoStreamDeckSocket' callback
				"payload": {
					// here we can use ES6 object-literals to use the  'param' parameter as a JSON key. In our example this resolves to {'myIdentifier': <value>}
					[param] : value 
				}
		 };
		 // send the json object to the plugin
		 // please remember to 'stringify' the object first, since the websocket
		 // just sends plain strings.
		 websocket.send(JSON.stringify(json));
	}
}