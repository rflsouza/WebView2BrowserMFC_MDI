const messageHandler = event => {
    console.log('messageHandler event', event);

    var message = event.data.message;
    var args = event.data.args;

    switch (message) {
        case commands.MG_INIT:
            if (args) {
                console.log("MG_INIT");
                console.log(`Received MG_INIT message: ${JSON.stringify(args)}`);
            }
            break;
        case commands.MG_COLOR:
            if (args) {
                console.log("MG_COLOR");
                document.getElementById("colorable").style.color = "Blue";
            }
            break;       
        default:
            console.log(`Received unexpected message: ${JSON.stringify(event.data)}`);
    }    
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