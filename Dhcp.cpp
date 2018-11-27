// DHCP Library v0.3 - April 25, 2009
// Author: Jordan Terrell - blog.jordanterrell.com

#include <string.h>
#include <stdlib.h>
#include "utility/uipopt.h"
#if UIP_UDP
#include "Dhcp.h"
#if defined(ARDUINO)
  #include <Arduino.h>
#endif
#if defined(__MBED__)
  #include <mbed.h>
  #include "mbed/millis.h"
  #define delay(x) wait_ms(x)
#endif
#include "utility/logging.h"
#include "utility/uip.h"

DhcpClass::DhcpClass() : 
	_hostname(NULL)
{
}

int DhcpClass::beginWithDHCP(uint8_t *mac) {
	// Default hostname is ENC28J + last 3 bytes of mac address in hex
	uint8_t hostname_length = strlen(HOST_NAME_DEFAULT_PREFIX) + 6;
	char * buffer = (char *)malloc(hostname_length + 1); // extra null-terminator byte!
	memset(buffer, 0, hostname_length + 1);
	strcpy(buffer, HOST_NAME_DEFAULT_PREFIX);

    printByte((char*)&(buffer[hostname_length-6]), mac[3]);
    printByte((char*)&(buffer[hostname_length-4]), mac[4]);
    printByte((char*)&(buffer[hostname_length-2]), mac[5]);

	int res = beginWithDHCP(mac, buffer);
	free(buffer);
	return res;
}

int DhcpClass::beginWithDHCP(uint8_t *mac, char * hostname)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::beginWithDHCP(uint8_t *mac) DEBUG_V1:Function started"));
    #endif
    _dhcpLeaseTime=0;
    _dhcpT1=0;
    _dhcpT2=0;
    _lastCheck=0;

	// prepare the hostname
	if (_hostname != NULL) {
		free(_hostname);
	}
	_hostname = (char *)malloc(strlen(hostname)+1);
	strcpy(_hostname, hostname);

    // zero out _dhcpMacAddr
    memset(_dhcpMacAddr, 0, 6); 
    reset_DHCP_lease();

    memcpy((void*)_dhcpMacAddr, (void*)mac, 6);
    _dhcp_state = STATE_DHCP_START;
    int res = request_DHCP_lease();
	//free(_hostname);
	//_hostname = NULL;
	return res;
}

void DhcpClass::reset_DHCP_lease(void){
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::reset_DHCP_lease(void) DEBUG_V1:Function started"));
    #endif
    // zero out _dhcpipv4struct.SubnetMask, _dhcpipv4struct.GatewayIp, _dhcpipv4struct.LocalIp, _dhcpipv4struct.DhcpServerIp, _dhcpipv4struct.DnsServerIp
    memset(&_dhcpipv4struct, 0, sizeof(_dhcpipv4struct));
}

//return:0 on error, 1 if request is sent and response is received
int DhcpClass::request_DHCP_lease(void){
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::request_DHCP_lease(void) DEBUG_V1:Function started"));
    #endif
    
    uint8_t messageType = 0;
  
    
  
    // Pick an initial transaction ID
    #if defined(ARDUINO)
       _dhcpTransactionId = random(1UL, 2000UL);
    #endif
    #if defined(__MBED__)
       _dhcpTransactionId = (rand() % 2000UL) + 1;
    #endif
    _dhcpInitialTransactionId = _dhcpTransactionId;

    _dhcpUdpSocket.stop();
    if (_dhcpUdpSocket.begin(DHCP_CLIENT_PORT) == 0)
    {
      // Couldn't get a socket
      return 0;
    }
    
    presend_DHCP();
    
    int result = 0;
    
    unsigned long startTime = millis();
    
    while(_dhcp_state != STATE_DHCP_LEASED)
    {
        if(_dhcp_state == STATE_DHCP_START)
        {
            #if ACTLOGLEVEL>=LOG_DEBUG_V1
              LogObject.uart_send_strln(F("DhcpClass::request_DHCP_lease(void) DEBUG_V1:dhcp_state=STATE_DHCP_START -> send_DHCP_MESSAGE DHCP_DISCOVER"));
            #endif
            _dhcpTransactionId++;
            
            send_DHCP_MESSAGE(DHCP_DISCOVER, ((millis() - startTime) / 1000));
            _dhcp_state = STATE_DHCP_DISCOVER;
        }
        else if(_dhcp_state == STATE_DHCP_REREQUEST){
            #if ACTLOGLEVEL>=LOG_DEBUG_V1
              LogObject.uart_send_strln(F("DhcpClass::request_DHCP_lease(void) DEBUG_V1:dhcp_state=STATE_DHCP_REREQUEST -> send_DHCP_MESSAGE DHCP_REQUEST"));
            #endif
            _dhcpTransactionId++;
            send_DHCP_MESSAGE(DHCP_REQUEST, ((millis() - startTime)/1000));
            _dhcp_state = STATE_DHCP_REQUEST;
        }
        else if(_dhcp_state == STATE_DHCP_DISCOVER)
        {
            uint32_t respId;
            messageType = parseDHCPResponse(respId, false);
            if(messageType == DHCP_OFFER)
            {
                #if ACTLOGLEVEL>=LOG_DEBUG_V1
                  LogObject.uart_send_strln(F("DhcpClass::request_DHCP_lease(void) DEBUG_V1:dhcp_state=STATE_DHCP_DISCOVER,messageType=DHCP_OFFER -> send_DHCP_MESSAGE DHCP_REQUEST"));
                #endif
                // We'll use the transaction ID that the offer came with,
                // rather than the one we were up to
                _dhcpTransactionId = respId;
                send_DHCP_MESSAGE(DHCP_REQUEST, ((millis() - startTime) / 1000));
                _dhcp_state = STATE_DHCP_REQUEST;
            }
        }
        else if(_dhcp_state == STATE_DHCP_REQUEST)
        {
            uint32_t respId;
            messageType = parseDHCPResponse(respId, false);
            if(messageType == DHCP_ACK)
            {
                #if ACTLOGLEVEL>=LOG_DEBUG_V1
                  LogObject.uart_send_strln(F("DhcpClass::request_DHCP_lease(void) DEBUG_V1:dhcp_state=STATE_DHCP_REQUEST,messageType=DHCP_ACK"));
                #endif
                _dhcp_state = STATE_DHCP_LEASED;
                result = 1;
                //use default lease time if we didn't get it
                if(_dhcpLeaseTime == 0){
                    _dhcpLeaseTime = DEFAULT_LEASE;
                }
                //calculate T1 & T2 if we didn't get it
                if(_dhcpT1 == 0){
                    //T1 should be 50% of _dhcpLeaseTime
                    _dhcpT1 = _dhcpLeaseTime >> 1;
                }
                if(_dhcpT2 == 0){
                    //T2 should be 87.5% (7/8ths) of _dhcpLeaseTime
                    _dhcpT2 = _dhcpT1 << 1;
                }
                _renewInSec = _dhcpT1;
                _rebindInSec = _dhcpT2;
            }
            else if(messageType == DHCP_NAK)
                _dhcp_state = STATE_DHCP_START;
        }
        
        if(messageType == 255)
        {
            messageType = 0;
            _dhcp_state = STATE_DHCP_START;
        }
        
        if(result != 1 && ((millis() - startTime) > DHCP_TIMEOUT))
            break;
    #if defined(ESP8266)
       wdt_reset();
    #endif
    }
    
    // We're done with the socket now
    _dhcpUdpSocket.stop();
    _dhcpTransactionId++;

    return result;
}

void DhcpClass::presend_DHCP(void)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::presend_DHCP(void) DEBUG_V1:Function started (Empty function)"));
    #endif
}

void DhcpClass::send_DHCP_MESSAGE(uint8_t messageType, uint16_t secondsElapsed)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::send_DHCP_MESSAGE(uint8_t messageType, uint16_t secondsElapsed) DEBUG_V1:Function started"));
    #endif
	uint8_t buffer_length = max(32, 18 + strlen(_hostname) + 2); // 2 extra bytes for good measure. The original code used 30 bytes but allocated 32...
    uint8_t buffer[buffer_length];
    memset(buffer, 0, buffer_length);
    /// @TODO: Actually if we are already bound to a DHCP server we should send the packet directly to that instead of always broadcasting them!
    IPAddress dest_addr( 255, 255, 255, 255 ); // Broadcast address

    if (-1 == _dhcpUdpSocket.beginPacket(dest_addr, DHCP_SERVER_PORT))
    {
        // FIXME Need to return errors
        return;
    }

    buffer[0] = DHCP_BOOTREQUEST;   // op
    buffer[1] = DHCP_HTYPE10MB;     // htype
    buffer[2] = DHCP_HLENETHERNET;  // hlen
    buffer[3] = DHCP_HOPS;          // hops

    // xid
    unsigned long xid = htonl(_dhcpTransactionId);
    memcpy(buffer + 4, &(xid), 4);

    // 8, 9 - seconds elapsed
    buffer[8] = ((secondsElapsed & 0xff00) >> 8);
    buffer[9] = (secondsElapsed & 0x00ff);

    // flags
    unsigned short flags = htons(DHCP_FLAGSBROADCAST);
    memcpy(buffer + 10, &(flags), 2);

    // ciaddr: already zeroed
    // yiaddr: already zeroed
    // siaddr: already zeroed
    // giaddr: already zeroed

    //put data in W5100 transmit buffer
    _dhcpUdpSocket.write(buffer, 28);

    memset(buffer, 0, buffer_length); // clear local buffer

    memcpy(buffer, _dhcpMacAddr, 6); // chaddr

    //put data in W5100 transmit buffer
    _dhcpUdpSocket.write(buffer, 16);

    memset(buffer, 0, buffer_length); // clear local buffer

    // leave zeroed out for sname && file
    // put in W5100 transmit buffer x 6 (192 bytes)
  
    for(int i = 0; i < 6; i++) {
        _dhcpUdpSocket.write(buffer, 32);
    }
  
    // OPT - Magic Cookie
    buffer[0] = (uint8_t)((MAGIC_COOKIE >> 24)& 0xFF);
    buffer[1] = (uint8_t)((MAGIC_COOKIE >> 16)& 0xFF);
    buffer[2] = (uint8_t)((MAGIC_COOKIE >> 8)& 0xFF);
    buffer[3] = (uint8_t)(MAGIC_COOKIE& 0xFF);

    // OPT - message type
    buffer[4] = dhcpMessageType;
    buffer[5] = 0x01;
    buffer[6] = messageType; //DHCP_REQUEST;

    // OPT - client identifier
    buffer[7] = dhcpClientIdentifier;
    buffer[8] = 0x07;
    buffer[9] = 0x01;
    memcpy(buffer + 10, _dhcpMacAddr, 6);

    // OPT - host name
    buffer[16] = hostName;
    buffer[17] = strlen(_hostname); // length of hostname
	memcpy(buffer+18, _hostname, strlen(_hostname));

    //put data in W5100 transmit buffer
    _dhcpUdpSocket.write(buffer, 18 + strlen(_hostname));

    if(messageType == DHCP_REQUEST)
    {
        buffer[0] = dhcpRequestedIPaddr;
        buffer[1] = 0x04;
        buffer[2] = _dhcpipv4struct.LocalIp[0];
        buffer[3] = _dhcpipv4struct.LocalIp[1];
        buffer[4] = _dhcpipv4struct.LocalIp[2];
        buffer[5] = _dhcpipv4struct.LocalIp[3];

        buffer[6] = dhcpServerIdentifier;
        buffer[7] = 0x04;
        buffer[8] = _dhcpipv4struct.DhcpServerIp[0];
        buffer[9] = _dhcpipv4struct.DhcpServerIp[1];
        buffer[10] = _dhcpipv4struct.DhcpServerIp[2];
        buffer[11] = _dhcpipv4struct.DhcpServerIp[3];

        //put data in W5100 transmit buffer
        _dhcpUdpSocket.write(buffer, 12);
    }
    // memset(buffer, 0, buffer_length);   
    buffer[0] = dhcpParamRequest;
    buffer[1] = 0x06;
    buffer[2] = subnetMask;
    buffer[3] = routersOnSubnet;
    buffer[4] = dns;
    buffer[5] = domainName;
    buffer[6] = dhcpT1value;
    buffer[7] = dhcpT2value;
    buffer[8] = endOption;
    
    //put data in W5100 transmit buffer
    _dhcpUdpSocket.write(buffer, 9);

    _dhcpUdpSocket.endPacket();
}

uint8_t DhcpClass::parseDHCPResponse(uint32_t& transactionId, bool async)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::parseDHCPResponse(uint32_t& transactionId, bool async) DEBUG_V1:Function started"));
    #endif
    uint8_t type = 0;
    uint8_t opt_len = 0;

    if (!async) {
    	unsigned long startTime = millis();
    	while(_dhcpUdpSocket.parsePacket() <= 0)
		{
			if((millis() - startTime) > DHCP_RESPONSE_TIMEOUT)
			{
				return 255;
			}
			delay(50);
		}
    }
    // start reading in the packet
    RIP_MSG_FIXED fixedMsg;
    _dhcpUdpSocket.read((char*)&fixedMsg, sizeof(RIP_MSG_FIXED));
  
    if(fixedMsg.op == DHCP_BOOTREPLY && _dhcpUdpSocket.remotePort() == DHCP_SERVER_PORT)
    {
        transactionId = ntohl(fixedMsg.xid);
        if(memcmp(fixedMsg.chaddr, _dhcpMacAddr, 6) != 0 || (transactionId < _dhcpInitialTransactionId) || (transactionId > _dhcpTransactionId))
        {
            // Need to read the rest of the packet here regardless
            _dhcpUdpSocket.flush();
            return 0;
        }

        memcpy(_dhcpipv4struct.LocalIp, fixedMsg.yiaddr, 4);

        // Skip to the option part
        // Doing this a byte at a time so we don't have to put a big buffer
        // on the stack (as we don't have lots of memory lying around)
        for (int i =0; i < (240 - (int)sizeof(RIP_MSG_FIXED)); i++)
        {
            _dhcpUdpSocket.read(); // we don't care about the returned byte
        }

        while (_dhcpUdpSocket.available() > 0) 
        {
            switch (_dhcpUdpSocket.read()) 
            {
                case endOption :
                    break;
                    
                case padOption :
                    break;
                
                case dhcpMessageType :
                    opt_len = _dhcpUdpSocket.read();
                    type = _dhcpUdpSocket.read();
                    break;
                
                case subnetMask :
                    opt_len = _dhcpUdpSocket.read();
                    _dhcpUdpSocket.read((char*)_dhcpipv4struct.SubnetMask, 4);
                    break;
                
                case routersOnSubnet :
                    opt_len = _dhcpUdpSocket.read();
                    _dhcpUdpSocket.read((char*)_dhcpipv4struct.GatewayIp, 4);
                    for (int i = 0; i < opt_len-4; i++)
                    {
                        _dhcpUdpSocket.read();
                    }
                    break;
                
                case dns :
                    opt_len = _dhcpUdpSocket.read();
                    _dhcpUdpSocket.read((char*)_dhcpipv4struct.DnsServerIp, 4);
                    for (int i = 0; i < opt_len-4; i++)
                    {
                        _dhcpUdpSocket.read();
                    }
                    break;
                
                case dhcpServerIdentifier :
                    opt_len = _dhcpUdpSocket.read();
                    if( IPAddress(_dhcpipv4struct.DhcpServerIp) == IPAddress(0,0,0,0) ||
                        IPAddress(_dhcpipv4struct.DhcpServerIp) == _dhcpUdpSocket.remoteIP() )
                    {
                        _dhcpUdpSocket.read((char*)_dhcpipv4struct.DhcpServerIp, sizeof(_dhcpipv4struct.DhcpServerIp));
                    }
                    else
                    {
                    	// Skip over the rest of this option
                        while (opt_len--)
                        {
                            _dhcpUdpSocket.read();
                        }
                    }
                    break;

                case dhcpT1value : 
                    opt_len = _dhcpUdpSocket.read();
                    _dhcpUdpSocket.read((char*)&_dhcpT1, sizeof(_dhcpT1));
                    _dhcpT1 = ntohl(_dhcpT1);
                    break;

                case dhcpT2value : 
                    opt_len = _dhcpUdpSocket.read();
                    _dhcpUdpSocket.read((char*)&_dhcpT2, sizeof(_dhcpT2));
                    _dhcpT2 = ntohl(_dhcpT2);
                    break;

                case dhcpIPaddrLeaseTime :
                    opt_len = _dhcpUdpSocket.read();
                    _dhcpUdpSocket.read((char*)&_dhcpLeaseTime, sizeof(_dhcpLeaseTime));
                    _dhcpLeaseTime = ntohl(_dhcpLeaseTime);
                    _renewInSec = _dhcpLeaseTime;
                    break;

                default :
                    opt_len = _dhcpUdpSocket.read();
                    // Skip over the rest of this option
                    while (opt_len--)
                    {
                        _dhcpUdpSocket.read();
                    }
                    break;
            }
        }
    }

    // Need to skip to end of the packet regardless here
    _dhcpUdpSocket.flush();

    return type;
}


/*
    returns:
    0/DHCP_CHECK_NONE: nothing happened
    1/DHCP_CHECK_RENEW_FAIL: renew failed
    2/DHCP_CHECK_RENEW_OK: renew success
    3/DHCP_CHECK_REBIND_FAIL: rebind fail
    4/DHCP_CHECK_REBIND_OK: rebind success
*/
int DhcpClass::checkLease(void){
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::checkLease(void) DEBUG_V1:Function started"));
    #endif

    //this uses a signed / unsigned trick to deal with millis overflow
    unsigned long now = millis();
    signed long snow = (long)now;
    int rc=DHCP_CHECK_NONE;
    if (_lastCheck != 0){
        signed long factor;
        //calc how many ms past the timeout we are
        factor = snow - (long)_secTimeout;
        //if on or passed the timeout, reduce the counters
        if ( factor >= 0 ){
            //next timeout should be now plus 1000 ms minus parts of second in factor
            _secTimeout = snow + 1000 - factor % 1000;
            //how many seconds late are we, minimum 1
            factor = factor / 1000 +1;
            
            //reduce the counters by that mouch
            //if we can assume that the cycle time (factor) is fairly constant
            //and if the remainder is less than cycle time * 2 
            //do it early instead of late
            if(_renewInSec < factor*2 )
                _renewInSec = 0;
            else
                _renewInSec -= factor;
            
            if(_rebindInSec < factor*2 )
                _rebindInSec = 0;
            else
                _rebindInSec -= factor;
        }

        //if we have a lease but should renew, do it
        if (_dhcp_state == STATE_DHCP_LEASED && _renewInSec <=0){
            _dhcp_state = STATE_DHCP_REREQUEST;
            rc = 1 + request_DHCP_lease();
        }

        //if we have a lease or is renewing but should bind, do it
        if( (_dhcp_state == STATE_DHCP_LEASED || _dhcp_state == STATE_DHCP_START) && _rebindInSec <=0){
            //this should basically restart completely
            _dhcp_state = STATE_DHCP_START;
            reset_DHCP_lease();
            rc = 3 + request_DHCP_lease();
        }
    }
    else{
        _secTimeout = snow + 1000;
    }

    _lastCheck = now;
    return rc;
}

IPAddress DhcpClass::getLocalIp(void)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::getLocalIp(void) DEBUG_V1:Function started"));
    #endif
    return IPAddress(_dhcpipv4struct.LocalIp);
}

IPAddress DhcpClass::getSubnetMask(void)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::getSubnetMask(void) DEBUG_V1:Function started"));
    #endif
    return IPAddress(_dhcpipv4struct.SubnetMask);
}

IPAddress DhcpClass::getGatewayIp(void)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::getGatewayIp(void) DEBUG_V1:Function started"));
    #endif
    return IPAddress(_dhcpipv4struct.GatewayIp);
}

IPAddress DhcpClass::getDhcpServerIp(void)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::getDhcpServerIp(void) DEBUG_V1:Function started"));
    #endif
    return IPAddress(_dhcpipv4struct.DhcpServerIp);
}

IPAddress DhcpClass::getDnsServerIp(void)
{
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::getDnsServerIp(void) DEBUG_V1:Function started"));
    #endif
    return IPAddress(_dhcpipv4struct.DnsServerIp);
}

void DhcpClass::printByte(char * buf, uint8_t n ) {
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::printByte(char * buf, uint8_t n ) DEBUG_V1:Function started"));
    #endif
  char *str = &buf[1];
  buf[0]='0';
  do {
    unsigned long m = n;
    n /= 16;
    char c = m - 16 * n;
    *str-- = c < 10 ? c + '0' : c + 'A' - 10;
  } while(n);
}


//--------//
#if UIP_ASYNC_DHCP
int DhcpClass::beginWithDHCPAsync(uint8_t *mac, char * hostname) {
#if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::beginWithDHCPAsync(uint8_t *mac) DEBUG_V1:Function started"));
    #endif
    _dhcpLeaseTime=0;
    _dhcpT1=0;
    _dhcpT2=0;
    _lastCheck=0;

	// prepare the hostname
	if (_hostname != NULL) {
		free(_hostname);
	}
	_hostname = (char *)malloc(strlen(hostname) + 1);
	strcpy(_hostname, hostname);

    // zero out _dhcpMacAddr
    memset(_dhcpMacAddr, 0, 6);
    reset_DHCP_lease();

    memcpy((void*)_dhcpMacAddr, (void*)mac, 6);
    _dhcp_state = STATE_DHCP_START;
    int res = request_DHCP_lease_async();
	//free(_hostname);
	//_hostname = NULL;
	return res;
}

int DhcpClass::request_DHCP_lease_async(void){
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::request_DHCP_lease(void) DEBUG_V1:Function started"));
    #endif

    // Pick an initial transaction ID
    #if defined(ARDUINO)
       _dhcpTransactionId = random(1UL, 2000UL);
    #endif
    #if defined(__MBED__)
       _dhcpTransactionId = (rand() % 2000UL) + 1;
    #endif
    _dhcpInitialTransactionId = _dhcpTransactionId;

    _dhcpUdpSocket.stop();
    if (_dhcpUdpSocket.begin(DHCP_CLIENT_PORT) == 0)
    {
      // Couldn't get a socket
      return 0;
    }

    presend_DHCP();

    _ra_startTime = millis();

    return 1;
}

int DhcpClass::pollDHCPAsync(void){
	#if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::pollDHCPAsync(void) DEBUG_V1:Function started"));
    #endif
	uint8_t messageType = 0;
	int result = 0;

	if(_dhcp_state == STATE_DHCP_START)
	{
		#if ACTLOGLEVEL>=LOG_DEBUG_V1
		  LogObject.uart_send_strln(F("DhcpClass::pollDHCPAsync(void) DEBUG_V1:dhcp_state=STATE_DHCP_START -> send_DHCP_MESSAGE DHCP_DISCOVER"));
		#endif
		_dhcpTransactionId++;

		// if we are sending a discover message we should reset the IP of the server we might have been bound to. Otherwise if the IP of the DHCP has changed
		// the response will be ignored, because it does not match. We will eventually time out but that takes a few minutes and is not really needed
		_dhcpipv4struct.DhcpServerIp[0] = 0;
		_dhcpipv4struct.DhcpServerIp[1] = 0;
		_dhcpipv4struct.DhcpServerIp[2] = 0;
		_dhcpipv4struct.DhcpServerIp[3] = 0;
		send_DHCP_MESSAGE(DHCP_DISCOVER, ((millis() - _ra_startTime) / 1000));
		_dhcp_state = STATE_DHCP_DISCOVER;
		_ra_dhcp_request_startTime = millis();
	}
	else if(_dhcp_state == STATE_DHCP_REREQUEST){
		#if ACTLOGLEVEL>=LOG_DEBUG_V1
		  LogObject.uart_send_strln(F("DhcpClass::pollDHCPAsync(void) DEBUG_V1:dhcp_state=STATE_DHCP_REREQUEST -> send_DHCP_MESSAGE DHCP_REQUEST"));
		#endif
		_dhcpTransactionId++;
		send_DHCP_MESSAGE(DHCP_REQUEST, ((millis() - _ra_startTime)/1000));
		_dhcp_state = STATE_DHCP_REQUEST;
		_ra_dhcp_request_startTime = millis();
	}
	else if(_dhcp_state == STATE_DHCP_DISCOVER)
	{
		#if ACTLOGLEVEL>=LOG_DEBUG_V1
		  LogObject.uart_send_strln(F("DhcpClass::pollDHCPAsync(void) DEBUG_V1:dhcp_state=STATE_DHCP_DISCOVER -> check response available"));
		#endif
		uint32_t respId;
		int ra = check_async_response_available();
		if (ra == 1) {
			messageType = parseDHCPResponse(respId, true);
			if(messageType == DHCP_OFFER)
			{
				#if ACTLOGLEVEL>=LOG_DEBUG_V1
				  LogObject.uart_send_strln(F("DhcpClass::pollDHCPAsync(void) DEBUG_V1:dhcp_state=STATE_DHCP_DISCOVER,messageType=DHCP_OFFER -> send_DHCP_MESSAGE DHCP_REQUEST"));
				#endif
				// We'll use the transaction ID that the offer came with,
				// rather than the one we were up to
				_dhcpTransactionId = respId;
				send_DHCP_MESSAGE(DHCP_REQUEST, ((millis() - _ra_startTime) / 1000));
				_dhcp_state = STATE_DHCP_REQUEST;
			}
		} else if (ra == 255) {
			// timeout
			messageType = ra;
		}
	}
	else if(_dhcp_state == STATE_DHCP_REQUEST)
	{
		uint32_t respId;
		int ra = check_async_response_available();
		if (ra == 1) {
			messageType = parseDHCPResponse(respId, true);

			if(messageType == DHCP_ACK)
			{
				#if ACTLOGLEVEL>=LOG_DEBUG_V1
				  LogObject.uart_send_strln(F("DhcpClass::pollDHCPAsync(void) DEBUG_V1:dhcp_state=STATE_DHCP_REQUEST,messageType=DHCP_ACK"));
				#endif
				_dhcp_state = STATE_DHCP_LEASED;
				result = 1;
				//use default lease time if we didn't get it
				if(_dhcpLeaseTime == 0){
					_dhcpLeaseTime = DEFAULT_LEASE;
				}
				//calculate T1 & T2 if we didn't get it
				if(_dhcpT1 == 0){
					//T1 should be 50% of _dhcpLeaseTime
					_dhcpT1 = _dhcpLeaseTime >> 1;
				}
				if(_dhcpT2 == 0){
					//T2 should be 87.5% (7/8ths) of _dhcpLeaseTime
					_dhcpT2 = _dhcpT1 << 1;
				}
				_renewInSec = _dhcpT1;
				_rebindInSec = _dhcpT2;

				#if ACTLOGLEVEL>=LOG_DEBUG_V1
				  LogObject.uart_send_str (F("DhcpClass DEBUG_V1 renew in "));
				  LogObject.uart_send_str (_renewInSec);
				  LogObject.uart_send_str (F(" rebind in "));
				  LogObject.uart_send_strln (_rebindInSec);
				#endif
			}
			else if(messageType == DHCP_NAK)
				_dhcp_state = STATE_DHCP_START;
		} else if (ra == 255) {
			messageType = 255;
		}
	}

	if(messageType == 255)
	{
		messageType = 0;
		_dhcp_state = STATE_DHCP_START;
	}

	if (_dhcp_state == STATE_DHCP_LEASED) {
		// end the async process and return OK
		request_DHCP_lease_async_end();
		return 1;
	}

	if(result != 1 && ((millis() - _ra_startTime) > DHCP_TIMEOUT)) {
		#if ACTLOGLEVEL>=LOG_DEBUG_V1
		  LogObject.uart_send_strln(F("DhcpClass::pollDHCPAsync(void) DEBUG_V1:DHCP_TIMEOUT reached, restarting"));
		#endif

		// reset the state and try again. We could add some timeout timer here really...

		_dhcp_state = STATE_DHCP_START;
		_dhcpLeaseTime=0;
		_dhcpT1=0;
		_dhcpT2=0;
		_lastCheck=0;

		request_DHCP_lease_async_end();
		reset_DHCP_lease();
		// request_DHCP_lease();

		return -1; // should we return an error if we reset and try again foreger?
	}

	#if defined(ESP8266)
       wdt_reset();
    #endif

    return 0;
}

int DhcpClass::request_DHCP_lease_async_end(void){
   // We're done with the socket now
   _dhcpUdpSocket.stop();
   _dhcpTransactionId++;
}

int DhcpClass::check_async_response_available() {
	if (_dhcpUdpSocket.parsePacket() <= 0) {
		if((millis() - _ra_dhcp_request_startTime) > DHCP_RESPONSE_TIMEOUT) {
			return 255;
		}
	} else {
		return 1;
	}
	return 0;
}

/// Async lease check

int DhcpClass::checkLease_async(void){
    #if ACTLOGLEVEL>=LOG_DEBUG_V1
      LogObject.uart_send_strln(F("DhcpClass::checkLease_async(void) DEBUG_V1:Function started"));
    #endif

    //this uses a signed / unsigned trick to deal with millis overflow

    unsigned long now = millis();
    signed long snow = (long)now;

    if (_lease_check_async_state > 0) {
		#if ACTLOGLEVEL>=LOG_DEBUG_V1
			LogObject.uart_send_strln(F("DhcpClass - in renew/rebind"));
		#endif
    	/// we have started the process of renewal or rebinding! we are waiting for async result from request_DHCP_lease_async()
    	// state is either STATE_DHCP_START or STATE_DHCP_REQUEST
    	int rc = pollDHCPAsync();
    	if (rc != 0) {
    		// we are no longer waiting for result

    		if (rc < 0) {
    			// there was a problem. This is the point where we need to check if we need to rebind
    			// is there anthing we should do here? The pollDHCPAsync functino will set the state to STATE_DHCP_START, rebind timeout will drop < 0 so
    			// next call will enter the check for renew/rebind branch and start the rebind...
    			if (_lease_check_async_state == 1) {
					// We are renewing
					rc = DHCP_CHECK_RENEW_FAIL;
				}
				if (_lease_check_async_state == 2) {
					// We are rebinding
					rc = DHCP_CHECK_REBIND_FAIL;
				}
    		} else {
    			// all went well
    			if (_lease_check_async_state == 1) {
    				// We are renewing
    				rc = DHCP_CHECK_RENEW_OK;
    			}
    			if (_lease_check_async_state == 2) {
    				// We are rebinding
    				rc = DHCP_CHECK_REBIND_OK;
    			}
    		}
    		_lease_check_async_state = 0;
    		_lastCheck = now;
    	}
		#if ACTLOGLEVEL>=LOG_DEBUG_V1
			LogObject.uart_send_str(F("DhcpClass:checkLease_async() return "));
			LogObject.uart_send_strln(rc);
		#endif
    	return rc;
    } else {
    	/// Do the normal check for renew/rebind, but call the async methods
		#if ACTLOGLEVEL>=LOG_DEBUG_V1
			LogObject.uart_send_strln(F("DhcpClass - check for renew/rebind"));
		#endif
        int rc=DHCP_CHECK_NONE;
        if (_lastCheck != 0){
            signed long factor;
            //calc how many ms past the timeout we are
            factor = snow - (long)_secTimeout;
            //if on or passed the timeout, reduce the counters
            if ( factor >= 0 ){
                //next timeout should be now plus 1000 ms minus parts of second in factor
                _secTimeout = snow + 1000 - factor % 1000;
                //how many seconds late are we, minimum 1
                factor = factor / 1000 +1;

                //reduce the counters by that mouch
                //if we can assume that the cycle time (factor) is fairly constant
                //and if the remainder is less than cycle time * 2
                //do it early instead of late
                if(_renewInSec < factor*2 )
                    _renewInSec = 0;
                else
                    _renewInSec -= factor;

                if(_rebindInSec < factor*2 )
                    _rebindInSec = 0;
                else
                    _rebindInSec -= factor;
            }

            //if we have a lease but should renew, do it
            if (_dhcp_state == STATE_DHCP_LEASED && _renewInSec <=0){
				#if ACTLOGLEVEL>=LOG_DEBUG_V1
					LogObject.uart_send_strln(F("DhcpClass:checkLease_async() - must renew "));
				#endif
                _dhcp_state = STATE_DHCP_REREQUEST;
                rc = 1 + request_DHCP_lease_async();
                _lease_check_async_state = 1; // raise the flag for the next call to this method
            }

            //if we have a lease or is renewing but should bind, do it
            if( (_dhcp_state == STATE_DHCP_LEASED || _dhcp_state == STATE_DHCP_START) && _rebindInSec <=0){
                //this should basically restart completely
				#if ACTLOGLEVEL>=LOG_DEBUG_V1
					LogObject.uart_send_strln(F("DhcpClass:checkLease_async() - must rebind "));
				#endif
                _dhcp_state = STATE_DHCP_START;
                reset_DHCP_lease();
                rc = 3 + request_DHCP_lease_async();
                _lease_check_async_state = 2; // raise the flag for the next call to this method
            }
        }
        else{
            _secTimeout = snow + 1000;
        }

        _lastCheck = now;
		#if ACTLOGLEVEL>=LOG_DEBUG_V1
			LogObject.uart_send_str(F("DhcpClass:checkLease_async() return "));
			LogObject.uart_send_strln(rc);
		#endif
        return rc;
    }

}

#endif

#endif
