extern "C"
{
#import "utility/uip-conf.h"
#import "utility/uip.h"
#import "utility/uip_arp.h"
#import "string.h"
}
#include "UIPEthernet.h"
#include "UIPClientExt.h"
#include "Dns.h"

UIPClientExt::UIPClientExt() :
    conn(NULL)
{
}

int
UIPClientExt::connectNB(IPAddress ip, uint16_t port)
{
    stop();
    uip_ipaddr_t ipaddr;
    uip_ip_addr(ipaddr, ip);
    conn = uip_connect(&ipaddr, htons(port));
    return conn ? 1 : 0;
}

void
UIPClientExt::stop()
{
  if (!data && conn)
    conn->tcpstateflags = UIP_CLOSED;
  else
    {
      conn = NULL;
      UIPClient::stop();
    }
}

uint8_t
UIPClientExt::connected()
{
  return *this ? UIPClient::connected() : 0;
}

UIPClientExt::operator bool()
{
  UIPEthernetClass::tick();
  if (conn && !data && (conn->tcpstateflags & UIP_TS_MASK) != UIP_CLOSED)
    {
      if ((conn->tcpstateflags & UIP_TS_MASK) == UIP_ESTABLISHED)
        {
          data = (uip_userdata_t*) conn->appstate;
#ifdef UIPETHERNET_DEBUG_CLIENT
          Serial.print(F("connected, state: "));
          Serial.print(data->state);
          Serial.print(F(", first packet in: "));
          Serial.println(data->packets_in[0]);
#endif
        }
      return true;
    }
  return data && (!(data->state & UIP_CLIENT_REMOTECLOSED) || data->packets_in[0] != NOBLOCK);
}

uint16_t
UIPClientExt::localPort() {
  if (!conn) return 0;
  return htons(conn->lport);
}

IPAddress
UIPClientExt::remoteIP() {
  if (!conn) return IPAddress();
  return ip_addr_uip(conn->ripaddr);
}

uint16_t
UIPClientExt::remotePort() {
  if (!conn) return 0;
  return htons(conn->rport);
}
