const messageHandler = event => {
    var message = event.data.message;
    var args = event.data.args;

    switch (message) {
        case commands.MG_INIT:
            if (args) {
                console.log("MG_INIT");
                alert(`Received unexpected message: ${JSON.stringify(event.args)}`);
            }
            break;
        case commands.MG_COLOR:
            if (args) {
                console.log("MG_NAV_COMPLETED");
                document.getElementById("colorable").style.color = "Blue";
            }
            break;       
        default:
            console.log(`Received unexpected message: ${JSON.stringify(event.data)}`);
    }
    console.log('event', event);
    window.xgen = event;
};

function sendMessageToBrowser(command, jsonArgs = {}) {
    let messageJson = {
        message: command,
        args: jsonArgs
    };

    window.chrome.webview.postMessage(messageJson);
}

function initialization(json) {
    console.log(json);
    window.xgenInitialization = json;
}

function init() {
    window.chrome.webview.addEventListener('message', messageHandler);
}

init();