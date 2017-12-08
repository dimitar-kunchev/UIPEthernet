// DNS Library Extension v0.1 - Dec 08, 2017
// Author: Dimitar Kunchev d.kunchev@gmail.com

/// This extension wraps over the Dns library that comes with UIPEthernet and adds support for asynchronous requests

#ifndef __DNS_NB_H__
#define __DNS_NB_H__

#include "Dns.h"

#define DNS_REQUEST_TIMEOUT 10000
#define DNS_REQUEST_ATTEMPTS 3

// Possible return codes from ProcessResponse - duplicated from original Dns.cpp (why were they declared in the cpp file I have no clue)
#define DNS_SUCCESS          1
#define DNS_TIMED_OUT        -1
#define DNS_INVALID_SERVER   -2
#define DNS_TRUNCATED        -3
#define DNS_INVALID_RESPONSE -4
#define DNS_WAITING -10

#define DNS_INTERNAL_ERROR  -11

class DNSClientNB: public DNSClient
{
  public:
    // void begin (const IPAddress& aDNSServer); // this is declared in parent class - just documenting it here because it needs to be called first
    int getHostByNameStart(const char * aHostname, IPAddress& aResult);
    int16_t checkResult (IPAddress& aAddress);
  private:
    char * requestedHostname;
    
    byte attempts;
    byte state;
    unsigned long packet_sent_ms;

    int attemptRequest();
};

#endif
