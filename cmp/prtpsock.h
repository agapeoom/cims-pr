#ifndef __PRTP_SOCK__
#define __PRTP_SOCK__


#include <string>
#include <iostream>

#include "pbase.h"
#include "pmodule.h"
//#include "PAVRtpSession.h"
//#include "PAVRtpSimSession.h"

#include "PMPBase.h"


#define INVALID_SOCKET           (-1)

struct RTPHEADER
{
    unsigned char  cc:4;
    unsigned char  extension:1;
    unsigned char  padding:1;
    unsigned char  version:2;
    unsigned char  payloadtype:7;
    unsigned char  marker:1;
    unsigned short seqnumber;
    unsigned int   timestamp;
    unsigned int   ssrc;

};

class CRtpSocket 
{
private: 
   int _fd;
   std::string _ipLoc;
   unsigned int _portLoc;
   std::string _ipRmt;
   unsigned int _portRmt;

   struct sockaddr_in _addrLoc;
   struct sockaddr_in _addrRmt;
   struct sockaddr_in _addrRmtLast;

public : 
   CRtpSocket() { };
   ~CRtpSocket() { };

   bool init(const std::string ip, unsigned int port) 
   {  
      _fd = socket(AF_INET, SOCK_DGRAM, 0);
      if(_fd == INVALID_SOCKET) {
	      fprintf(stderr, "Can't create socket!(ip=%s, port=%d)\n", locip.c_str(), locport);
	      return false;
      }

      _addrLoc.sin_addr.s_addr = inet_addr(ip.c_str());
      _addrLoc.sin_port = htons((unsigned short)port);
   
      int ret =  bind(sockfd, (sockaddr*)&_addrLoc, sizeof(_addrLoc));
      if(ret == -1) {
	      fprintf(stderr, "Can't bind socket!(ip=%s, port=%d)\n", locip.c_str(), locport);
	      return false;
      }

      //setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&nSockOpt, sizeof(nSockOpt)) 

      int flags;
      flags = fcntl(_fd, F_GETFL, 0);
      fcntl(_fd, F_SETFL, flags | O_NONBLOCK);

      int nRet = 256 * 20;;
      setsockopt(_fd, SOL_SOCKET, SO_SNDBUF, (char*)&nRet, sizeof(nRet));

      nRet = 256 * 20;;
      setsockopt(_fd, SOL_SOCKET, SO_RCVBUF, (char*)&nRet, sizeof(nRet));

      memset(&_addrRmt, 0, sizeof(_addrRmt));
      memset(&_addrRmtLast, 0, sizeof(_addrRmtLast));
      return true;
   }

   bool final() {
      if(_fd != INVALID_SOCKET) {
         close(_fd);
         _fd = INVALID_SOCKET;
         return true;
      }
      return false;
   }
 
   bool setRmt(const std::string ip, unsigned int port) { 
      _ipRmt = ip;
      _portRmt = port;
      
      _addrRmt.sin_family = AF_INET;
      _addrRmt.sin_addr.s_addr = inet_addr(ip.c_str());
      _addrRmt.sin_port = htons((unsigned short)port);

      return true;
   }

   int send(char * pkt, int len)
   {
      return sendto(_fd, pkt, len, 0, (sockaddr*)&_addrRmt, sizeof(struct sockaddr));
   }

   int recv(char * pkt, int len, std::string & ipRmt, int &portRmt) 
   {
      socklen_t nSockAddrLen = sizeof(struct sockaddr);
     
      int nLen = recvfrom(_fd, pkt, len, 0, (struct sockaddr *)&_sockAddrRmtLast, (socklen_t*)&sockAddrLen); 
      if(nLen == SOCKET_ERROR) 
      {
         return -1;
      }
      portRmt = ntohs(_sockAddrRmtLast.sin_port);
      ipRmt = (char *) inet_ntoa(_sockAddrRmtLast.sin_addr);

      return nLen;
   }

};

#endif // __PRTP_SOCK__

