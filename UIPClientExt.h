/*
 UIPClientExt.h - Arduino implementation of a uIP wrapper class.
 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */
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
