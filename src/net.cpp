#include "net.h"
#include "config.h"
#include "board.h"
#include <ETH.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <time.h>

static volatile bool s_ethLink = false, s_ethIp = false;
static bool     s_apOn = false, s_apAuto = false;
static bool     s_mdnsOn = false, s_ntpOn = false;
static uint32_t s_bootMs = 0, s_lanOkSince = 0;
static DNSServer s_dns;

static void onEvent(arduino_event_id_t ev) {
  switch (ev) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname(cfg.hostname);
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      s_ethLink = true;
      Serial.println("[net] LAN enlace arriba");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      s_ethIp = true;
      Serial.printf("[net] LAN ip=%s mask=%s gw=%s dns=%s  %dMbps %s\n",
                    ETH.localIP().toString().c_str(), ETH.subnetMask().toString().c_str(),
                    ETH.gatewayIP().toString().c_str(), ETH.dnsIP().toString().c_str(),
                    ETH.linkSpeed(), ETH.fullDuplex() ? "FD" : "HD");
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      s_ethLink = false; s_ethIp = false;
      Serial.println("[net] LAN enlace caido");
      break;
    default:
      break;
  }
}

static void startAp(bool autoStarted) {
  if (s_apOn) return;
  s_apOn = true;
  s_apAuto = autoStarted;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  if (strlen(cfg.apPass) >= 8) WiFi.softAP(cfg.apSsid, cfg.apPass, cfg.apChannel);
  else                         WiFi.softAP(cfg.apSsid, nullptr, cfg.apChannel);
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(53, "*", WiFi.softAPIP());          // portal cautivo
  Serial.printf("[net] AP '%s' en %s%s\n", cfg.apSsid, WiFi.softAPIP().toString().c_str(),
                autoStarted ? " (auto, sin LAN)" : "");
}

static void stopAp() {
  if (!s_apOn) return;
  s_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  s_apOn = false;
  s_apAuto = false;
  Serial.println("[net] AP detenido");
}

void netBegin() {
  s_bootMs = millis();
  WiFi.onEvent(onEvent);
  WiFi.persistent(false);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ETH.begin(ETH_PHY_LAN8720, ETH_PHY_ADDR_WT32, ETH_PHY_MDC_WT32, ETH_PHY_MDIO_WT32,
            ETH_PHY_POWER_WT32, ETH_CLOCK_GPIO0_IN);
#else
  ETH.begin(ETH_PHY_ADDR_WT32, ETH_PHY_POWER_WT32, ETH_PHY_MDC_WT32, ETH_PHY_MDIO_WT32,
            ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN);
#endif
  if (!cfg.eth.dhcp && cfg.eth.ip) {
    // Con IP fija hay que dar DNS explicito o el equipo queda sin resolucion
    // de nombres (OTA a github.com / NTP). Por defecto: la puerta de enlace
    // y 8.8.8.8 de respaldo.
    ETH.config(IPAddress(cfg.eth.ip), IPAddress(cfg.eth.gw),
               IPAddress(cfg.eth.mask ? cfg.eth.mask : (uint32_t)IPAddress(255, 255, 255, 0)),
               IPAddress(cfg.eth.dns1 ? cfg.eth.dns1 : cfg.eth.gw),
               IPAddress(cfg.eth.dns2 ? cfg.eth.dns2 : (uint32_t)IPAddress(8, 8, 8, 8)));
  }

  if (cfg.apMode == AP_ALWAYS) startAp(false);
}

void netStartAp() { startAp(false); }

void netLoop() {
  uint32_t now = millis();
  if (s_apOn) s_dns.processNextRequest();

  // AP automatico: sin LAN a los 30 s del arranque...
  if (cfg.apMode == AP_AUTO && !s_apOn && !s_ethIp && now - s_bootMs > 30000) startAp(true);
  // ... y se apaga solo si la LAN vuelve y nadie esta conectado al AP
  if (s_ethIp) { if (!s_lanOkSince) s_lanOkSince = now; }
  else s_lanOkSince = 0;
  if (s_apOn && s_apAuto && s_lanOkSince && now - s_lanOkSince > 120000 && WiFi.softAPgetStationNum() == 0)
    stopAp();

  if (s_ethIp && !s_ntpOn && cfg.ntpServer[0]) {
    configTzTime(cfg.tz, cfg.ntpServer, "time.google.com");
    s_ntpOn = true;
  }
  if ((s_ethIp || s_apOn) && !s_mdnsOn && cfg.mdns) {
    if (MDNS.begin(cfg.hostname)) {
      MDNS.addService("http", "tcp", 80);
      MDNS.addService("modbus", "tcp", cfg.tcpPort);
      Serial.printf("[net] mDNS http://%s.local\n", cfg.hostname);
    }
    s_mdnsOn = true;
  }
}

bool netEthLink() { return s_ethLink; }
bool netEthUp()   { return s_ethIp; }
bool netApUp()    { return s_apOn; }
bool netAnyUp()   { return s_ethIp || s_apOn; }

String netEthIp()   { return s_ethIp ? ETH.localIP().toString()   : String("-"); }
String netEthMask() { return s_ethIp ? ETH.subnetMask().toString() : String("-"); }
String netEthGw()   { return s_ethIp ? ETH.gatewayIP().toString()  : String("-"); }
String netEthDns()  { return s_ethIp ? ETH.dnsIP().toString()      : String("-"); }
String netEthMac()  { return ETH.macAddress(); }
int    netEthSpeed()      { return s_ethLink ? ETH.linkSpeed() : 0; }
bool   netEthFullDuplex() { return s_ethLink && ETH.fullDuplex(); }
String netApIp()          { return s_apOn ? WiFi.softAPIP().toString() : String("-"); }
uint8_t netApStations()   { return s_apOn ? WiFi.softAPgetStationNum() : 0; }

String netPrimaryIp() {
  if (s_ethIp) return ETH.localIP().toString();
  if (s_apOn) return WiFi.softAPIP().toString();
  return "sin red";
}

bool netIsApClient(uint32_t ip) {
  if (!s_apOn) return false;
  uint32_t ap = (uint32_t)WiFi.softAPIP();
  return (ip & 0x00FFFFFFUL) == (ap & 0x00FFFFFFUL);   // /24, orden de bytes de lwIP
}

bool netTimeOk() { return time(nullptr) > 1600000000; }
