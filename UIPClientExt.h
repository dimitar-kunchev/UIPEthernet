#include "UIPEthernet.h"

#ifndef __UIPCLIENT_EXT__
#define __UIPCLIENT_EXT__

class UIPClientExt : public UIPClient {

public:
  UIPClientExt();
  //UIPClientExt(struct uip_conn * c);
  //UIPClientExt(uip_userdata_t * data);
  int connectNB(IPAddress ip, uint16_t port);
  void stop();
  uint8_t connected();
  operator bool();
  uint16_t localPort();
  IPAddress remoteIP();
  uint16_t remotePort();

private:
  struct uip_conn* conn;
};

#endif
