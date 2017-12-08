// DNS Library Extension v0.1 - Dec 08, 2017
// Author: Dimitar Kunchev d.kunchev@gmail.com

#include "DnsNB.h"

#define DNS_PORT        53

int DNSClientNB::getHostByNameStart (const char * aHostname, IPAddress& aResult) {
  if (inet_aton(aHostname, aResult)) {
      // It is, our work here is done
      return 1;
  }

  // Check we've got a valid DNS server to use
  if (iDNSServer == INADDR_NONE) {
      return DNS_INVALID_SERVER;
  }

  attempts = 0;
   // bad... causes memory fragmentation, but I doubt this will be a problem in this particular use case
  requestedHostname = (char *)malloc(strlen(aHostname));
  strcpy(requestedHostname, aHostname);
  return attemptRequest();
}

int DNSClientNB::attemptRequest() {
  if (requestedHostname == NULL) {
    return DNS_INTERNAL_ERROR;
  }
  int ret;
  if (iUdp.begin(1024+(millis() & 0xF)) == 1) {
    ret = iUdp.beginPacket(iDNSServer, DNS_PORT);
    if (ret != 0) {
      // Now output the request data
      ret = BuildRequest(requestedHostname);
      if (ret != 0) {
        // And finally send the request    
        ret = iUdp.endPacket();
        if (ret != 0) {
          packet_sent_ms = millis();
          attempts ++;
          return 0;
        }
      }
    }
  }
  return -1;  
}

int16_t DNSClientNB::checkResult (IPAddress& aAddress) {
  if (iUdp.parsePacket() > 0) {
    /// Hurray! We have a response
    free(requestedHostname);
    requestedHostname = NULL;
    uint16_t res = ParseResponse(aAddress);
    // end the use of the socket
    iUdp.stop();
    return res;
  } else {
    /// Check for timeout
    if ((millis() - packet_sent_ms) > DNS_REQUEST_TIMEOUT) {
      if (attempts < DNS_REQUEST_ATTEMPTS) {
        attemptRequest();
        return DNS_WAITING;
      } else {
        return DNS_TIMED_OUT;
      }
    } else {
      return DNS_WAITING;
    }
  }
}

