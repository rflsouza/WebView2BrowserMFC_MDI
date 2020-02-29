# WebView2BrowserMFC_MDI

Sample to use [Microsoft Edge WebView2](https://docs.microsoft.com/microsoft-edge/hosting/webview2) control with MFC MDI Application

The program is an MDI, it starts and creates a new view, to change the views and show 2 working browsers.

If you add a new document, it will keep the views and will delete and create new browsers. 
On the console you can observe the operation.

Opening the program it will show view 1 with browser 1:

![WebView2Browser](/screenshots/Browser_view1.png)

You can change the view:

![WebView2Browser](/screenshots/Browser_SwitchView.png)

After the exchange, it will show view 2 with browser 2:

![WebView2Browser](/screenshots/Browser_view2.png)

if you create a new document, you will see that the browser will delete the current ones and will create new ones, maintaining the views.


