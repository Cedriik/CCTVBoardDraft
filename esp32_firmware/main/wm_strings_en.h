/**
 * wm_strings_en.h
 * English strings for WiFiManager captive portal
 * WiFiManager, a library for the ESP8266/Arduino platform
 * for configuration of WiFi credentials using a Captive Portal
 *
 * @author Creator tzapu
 * @author tablatronix
 * @version 0.0.0
 * @license MIT
 */

#ifndef _WM_STRINGS_H_
#define _WM_STRINGS_H_

#ifndef FPSTR
  #define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#endif

// HTML strings
const char HTTP_HEAD_START[] PROGMEM = "<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta name=\"format-detection\" content=\"telephone=no\">"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,user-scalable=no\"/>"
"<title>{v}</title>";

const char HTTP_STYLE[] PROGMEM = "<style>"
".c{text-align:center;}"
"div,input{padding:5px;font-size:1em;}"
"input{width:95%;}"
"body{text-align:center;font-family:verdana;}"
"button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;}"
".q{float:right;width:64px;text-align:right;}"
".l{background:url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==\") no-repeat left center;background-size:1em;}"
"</style>";

const char HTTP_SCRIPT[] PROGMEM = "<script>"
"function c(l){"
"document.getElementById('s').value=l.innerText||l.textContent;"
"document.getElementById('p').focus();"
"}"
"</script>";

const char HTTP_HEAD_END[] PROGMEM = "</head>"
"<body><div style='text-align:left;display:inline-block;min-width:260px;{c}'>";

// Alternative naming for compatibility
const char HTTP_HEADER[] PROGMEM = "<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta name=\"format-detection\" content=\"telephone=no\">"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,user-scalable=no\"/>"
"<title>{v}</title>";

const char HTTP_HEADER_END[] PROGMEM = "</head>"
"<body><div style='text-align:left;display:inline-block;min-width:260px;'>";

const char HTTP_ROOT_MAIN[] PROGMEM = "<h1>{t}</h1><h3>{v}</h3>";

const char HTTP_PORTAL_OPTIONS[] PROGMEM = "<form action=\"/wifi\" method=\"get\"><button>Configure WiFi</button></form><br/>"
"<form action=\"/0wifi\" method=\"get\"><button>Configure WiFi (No Scan)</button></form><br/>"
"<form action=\"/info\" method=\"get\"><button>Info</button></form><br/>"
"<form action=\"/reset\" method=\"post\"><button>Reset</button></form>";

const char HTTP_ITEM[] PROGMEM = "<div><a href='#p' onclick='c(this)'>{v}</a>&nbsp;<span class='q {i}'>{r}%</span></div>";

const char HTTP_ITEM_QP[] PROGMEM = "";
const char HTTP_ITEM_QI[] PROGMEM = "<div class='q'>";

const char HTTP_FORM_START[] PROGMEM = "<form method='get' action='{v}'>";
const char HTTP_FORM_WIFI[] PROGMEM = "<label for='s'>SSID</label><input id='s' name='s' length=32 maxlength=32 placeholder='SSID' value='{v}'><br/><label for='p'>Password</label><input id='p' name='p' length=64 maxlength=64 type='password' placeholder='password' value='{p}'>";
const char HTTP_FORM_WIFI_END[] PROGMEM = "";

const char HTTP_FORM_PARAM[] PROGMEM = "<br/><input id='{i}' name='{n}' length='{l}' placeholder='{p}' value='{v}' {c}>";
const char HTTP_FORM_LABEL[] PROGMEM = "<label for='{i}'>{t}</label>";
const char HTTP_FORM_PARAM_HEAD[] PROGMEM = "";

const char HTTP_FORM_END[] PROGMEM = "<br/><button type='submit'>save</button></form>";

const char HTTP_SCAN_LINK[] PROGMEM = "<br/><div class=\"c\"><a href=\"/wifi\">Scan</a></div>";
const char HTTP_BACKBTN[] PROGMEM = "<hr><a href=\"/\">Back</a>";

const char HTTP_FORM_STATIC_HEAD[] PROGMEM = "<br/>";

const char HTTP_SAVED[] PROGMEM = "<div>Credentials Saved<br />Trying to connect ESP to network.<br />If it fails reconnect to AP to try again</div>";

const char HTTP_END[] PROGMEM = "</div></body></html>";

const char HTTP_BR[] PROGMEM = "<br/>";

const char HTTP_JS[] PROGMEM = "<script>setTimeout(function(){window.location.reload();},30000);</script>";

// HTTP headers (HTTP_HEAD_CT defined in wm_consts_en.h)

// Info page strings
const char S_brand[] PROGMEM = "WiFiManager";
const char S_debugPrefix[] PROGMEM = "*wm:";
const char S_y[] PROGMEM = "Yes";
const char S_n[] PROGMEM = "No";
const char S_enable[] PROGMEM = "Enabled";
const char S_disable[] PROGMEM = "Disabled";
const char S_NA[] PROGMEM = "Unknown";

// Menu tokens
const char S_options[] PROGMEM = "options";
const char S_wifi[] PROGMEM = "wifi";
const char S_wifinoscan[] PROGMEM = "wifinoscan";
const char S_info[] PROGMEM = "info";
const char S_param[] PROGMEM = "param";
const char S_close[] PROGMEM = "close";
const char S_restart[] PROGMEM = "restart";
const char S_exit[] PROGMEM = "exit";
const char S_erase[] PROGMEM = "erase";
const char S_update[] PROGMEM = "update";

// Title strings
const char S_titlewifi[] PROGMEM = "Configure WiFi";
const char S_titleparam[] PROGMEM = "Setup";

// HTTP method strings  
const char S_GET[] PROGMEM = "GET";
const char S_POST[] PROGMEM = "POST";

// Network strings
const char S_nonetworks[] PROGMEM = "No networks found. Refresh to scan again.";
const char S_passph[] PROGMEM = "Password";

// Parameter strings
const char S_parampre[] PROGMEM = "param_";

// Static IP form strings
const char S_staticip[] PROGMEM = "Static IP";
const char S_staticgw[] PROGMEM = "Static Gateway";  
const char S_subnet[] PROGMEM = "Subnet";
const char S_staticdns[] PROGMEM = "Static DNS";

// Error messages
const char S_paramproblem[] PROGMEM = "[ERROR] parameter problem, invalid id";

// Access Point mode strings
const char S_apinfo[] PROGMEM = "Access Point Info";
const char S_apmac[] PROGMEM = "AP MAC";
const char S_apip[] PROGMEM = "AP IP";
const char S_apbssid[] PROGMEM = "AP BSSID";
const char S_apssid[] PROGMEM = "AP SSID";

// Station mode strings
const char S_stainfo[] PROGMEM = "Station Info";
const char S_stamac[] PROGMEM = "STA MAC";
const char S_staip[] PROGMEM = "STA IP";
const char S_stagw[] PROGMEM = "STA Gateway";
const char S_stasub[] PROGMEM = "STA Subnet";
const char S_dnss[] PROGMEM = "DNS Server";
const char S_host[] PROGMEM = "Hostname";
const char S_stamoder[] PROGMEM = "Station Mode";
const char S_autoconx[] PROGMEM = "AutoConnect";

// Device info strings
const char S_chipid[] PROGMEM = "Chip ID";
const char S_chiprev[] PROGMEM = "Chip Revision";
const char S_idesize[] PROGMEM = "Flash IDE Size";
const char S_flashsize[] PROGMEM = "Flash Real Size";
const char S_corever[] PROGMEM = "Core Version";
const char S_bootver[] PROGMEM = "Boot Version";
const char S_cpufreq[] PROGMEM = "CPU Frequency";
const char S_freeheap[] PROGMEM = "Memory - Free Heap";
const char S_memsketch[] PROGMEM = "Memory - Sketch Size";
const char S_memsmeter[] PROGMEM = "Memory - Sketch Free";

// Additional strings for form elements
const char S_ssid[] PROGMEM = "SSID";
const char S_password[] PROGMEM = "Password";
const char S_save[] PROGMEM = "Save";
const char S_scan[] PROGMEM = "Scan";
const char S_refresh[] PROGMEM = "Refresh";
const char S_reset[] PROGMEM = "Reset";
const char S_erase_wifi[] PROGMEM = "Erase WiFi Config";

// Status strings
const char S_status[] PROGMEM = "Status";
const char S_connected[] PROGMEM = "Connected";
const char S_disconnected[] PROGMEM = "Disconnected";

// Configuration portal strings
const char S_configuseap[] PROGMEM = "Configure WiFi (Use Access Point)";
const char S_confignonap[] PROGMEM = "Configure WiFi (No Access Point Scan)";

// Additional missing constants referenced by WiFiManager
const char S_ssidpre[] PROGMEM = "ESP";
const char S_titlewifisettings[] PROGMEM = "WiFi Settings";
const char S_titlewifisaved[] PROGMEM = "WiFi Saved";
const char S_titleparamsaved[] PROGMEM = "Parameters Saved";
const char S_titleinfo[] PROGMEM = "Info";
const char S_titleexit[] PROGMEM = "Exit";
const char S_titlereset[] PROGMEM = "Reset";
const char S_titleerase[] PROGMEM = "Erase";
const char S_titleclose[] PROGMEM = "Close";
const char S_exiting[] PROGMEM = "Exiting...";
const char S_resetting[] PROGMEM = "Resetting...";
const char S_closing[] PROGMEM = "Closing...";
const char S_error[] PROGMEM = "ERROR";
const char S_notfound[] PROGMEM = "File Not Found";
const char S_uri[] PROGMEM = "URI: ";
const char S_method[] PROGMEM = "Method: ";
const char S_args[] PROGMEM = "Arguments: ";

// HTTP Status strings
const char HTTP_STATUS_ON[] PROGMEM = "<span style='color:green'>ON</span>";
const char HTTP_STATUS_OFF[] PROGMEM = "<span style='color:red'>OFF</span>";
const char HTTP_STATUS_OFFPW[] PROGMEM = "<span style='color:red'>OFF (Wrong Password)</span>";
const char HTTP_STATUS_OFFNOAP[] PROGMEM = "<span style='color:red'>OFF (No AP Found)</span>";
const char HTTP_STATUS_OFFFAIL[] PROGMEM = "<span style='color:red'>OFF (Connection Failed)</span>";
const char HTTP_STATUS_NONE[] PROGMEM = "<span style='color:gray'>NONE</span>";

// HTTP form strings
const char HTTP_PARAMSAVED[] PROGMEM = "<div>Parameters saved successfully</div>";

// Portal menu array (replacing HTTP_PORTAL_MENU)
const char* const HTTP_PORTAL_MENU[] PROGMEM = {
    "",  // 0
    "",  // 1
    "",  // 2
    "",  // 3
    "",  // 4
    "",  // 5
    "",  // 6
    "",  // 7
    "<form action=\"/\" method=\"get\"><button>Back</button></form>",  // 8
    "",  // 9
};

// Additional HTTP info page strings
const char HTTP_INFO_esphead[] PROGMEM = "<h3>ESP32 Information</h3>";
const char HTTP_INFO_wifihead[] PROGMEM = "<h3>WiFi Information</h3>";
const char HTTP_INFO_uptime[] PROGMEM = "<dt>Uptime</dt><dd>{1} sec</dd>";
const char HTTP_INFO_chipid[] PROGMEM = "<dt>Chip ID</dt><dd>{1}</dd>";
const char HTTP_INFO_chiprev[] PROGMEM = "<dt>Chip Revision</dt><dd>{1}</dd>";
const char HTTP_INFO_idesize[] PROGMEM = "<dt>Flash IDE Size</dt><dd>{1} bytes</dd>";
const char HTTP_INFO_psrsize[] PROGMEM = "<dt>PSRAM Size</dt><dd>{1} bytes</dd>";
const char HTTP_INFO_cpufreq[] PROGMEM = "<dt>CPU Frequency</dt><dd>{1} MHz</dd>";
const char HTTP_INFO_freeheap[] PROGMEM = "<dt>Free Heap</dt><dd>{1} bytes</dd>";
const char HTTP_INFO_memsketch[] PROGMEM = "<dt>Sketch Size</dt><dd>{1} bytes</dd>";
const char HTTP_INFO_memsmeter[] PROGMEM = "<dt>Free Sketch Space</dt><dd>{1} bytes</dd>";
const char HTTP_INFO_apip[] PROGMEM = "<dt>AP IP</dt><dd>{1}</dd>";
const char HTTP_INFO_apmac[] PROGMEM = "<dt>AP MAC</dt><dd>{1}</dd>";
const char HTTP_INFO_aphost[] PROGMEM = "<dt>AP Hostname</dt><dd>{1}</dd>";
const char HTTP_INFO_apbssid[] PROGMEM = "<dt>AP BSSID</dt><dd>{1}</dd>";
const char HTTP_INFO_stassid[] PROGMEM = "<dt>STA SSID</dt><dd>{1}</dd>";
const char HTTP_INFO_staip[] PROGMEM = "<dt>STA IP</dt><dd>{1}</dd>";
const char HTTP_INFO_stagw[] PROGMEM = "<dt>STA Gateway</dt><dd>{1}</dd>";
const char HTTP_INFO_stasub[] PROGMEM = "<dt>STA Subnet</dt><dd>{1}</dd>";
const char HTTP_INFO_dnss[] PROGMEM = "<dt>DNS Server</dt><dd>{1}</dd>";
const char HTTP_INFO_host[] PROGMEM = "<dt>Hostname</dt><dd>{1}</dd>";
const char HTTP_INFO_stamac[] PROGMEM = "<dt>STA MAC</dt><dd>{1}</dd>";
const char HTTP_INFO_conx[] PROGMEM = "<dt>Connected</dt><dd>{1}</dd>";
const char HTTP_INFO_temp[] PROGMEM = "<dt>Temperature</dt><dd>{1}°C</dd>";
const char HTTP_INFO_aboutver[] PROGMEM = "<dt>WiFiManager</dt><dd>{1}</dd>";
const char HTTP_INFO_aboutarduino[] PROGMEM = "<dt>Arduino</dt><dd>{1}</dd>";
const char HTTP_INFO_sdkver[] PROGMEM = "<dt>ESP32 SDK</dt><dd>{1}</dd>";
const char HTTP_INFO_aboutdate[] PROGMEM = "<dt>Build Date</dt><dd>{1}</dd>";

// Additional buttons and forms
const char HTTP_ERASEBTN[] PROGMEM = "<form action=\"/erase\" method=\"post\"><button class=\"D\">Erase</button></form>";
const char HTTP_HELP[] PROGMEM = "<div><a href=\"https://github.com/tzapu/WiFiManager\">WiFiManager GitHub</a></div>";

// OTA Update strings
const char HTTP_UPDATE[] PROGMEM = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
const char HTTP_UPDATE_FAIL[] PROGMEM = "<div>Update Failed!</div>";
const char HTTP_UPDATE_SUCCESS[] PROGMEM = "<div>Update Successful! Rebooting...</div>";

// Debug strings
const char D_HR[] PROGMEM = "------------------------------------";

#include "wm_consts_en.h"

#endif