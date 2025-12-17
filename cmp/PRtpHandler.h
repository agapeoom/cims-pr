#include <string>
#include <iostream>

#include "pbase.h"
#include "pmodule.h"
#include "PAVRtpSession.h"
#include "PAVRtpSimSession.h"

#include "PMPBase.h"

#define PT_AUDIO1   98  // amr-nb
#define PT_AUDIO2   100 //dtmf  

#define INVALID_SOCKET           (-1)

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

class PRtpHandler : public PHandler
{
  

};


class PAVRtpModule : public PModule
{
public:
   PAVRtpModule(const std::string & name) : PModule(name)
   {

   }

   ~PAVRtpModule() 
   {

   }

   bool init(int sessions, const std::string locip, unsigned int locport, const std::string rmtip, unsigned int rmtport)
   {
	  PAVCODEC codec = PAVCODEC_AMRWB;

	  _sessions = sessions;

	  _pses  = new PAVRtpSession * [sessions];
	  std::string wname; 
	  for(int i=0; i<sessions; i++)
	  {
		 std::string hname = formatStr("PAVSession[%03d]", i+1);
		 _pses[i] = new PAVRtpSession(hname);

		 unsigned int port = locport + (i * 4);

		 bool bres = _pses[i]->init(locip, port);
		 if(!bres)
		 {
			printf("# %s(%d):%s : failed to init session(%s)!\n", __FILE__, __LINE__, __FUNCTION__, hname.c_str());
			return false;
		 }
		
		 port = rmtport + (i * 4);

		 _pses[i]->setRmtA(codec, rmtip, port, PT_AUDIO1, PT_AUDIO2);
		 _pses[i]->start();
		 

		 if(i%100 == 0) {
			wname = formatStr("PAVSesWorker[%03d]", i);
			addWorker(wname, 5, 2048, true);
		 }
		 addHandler(wname, _pses[i]);

	  }
	  return true;
   }


private:
   PAVRtpSession ** _pses;
   unsigned int _sessions;

};

class PAVRtpSimModule : public PModule
{
public:
   PAVRtpSimModule(const std::string & name) : PModule(name)
   {

   }

   ~PAVRtpSimModule() 
   {

   }

   bool init(int sessions, const std::string locip, unsigned int locport, const std::string rmtip, unsigned int rmtport)
   {
	  PAVCODEC codec = PAVCODEC_AMRWB;

	  _sessions = sessions;

	  _pses  = new PAVRtpSimSession * [sessions];
	  std::string wname; 
	  for(int i=0; i<sessions; i++)
	  {
		 std::string hname = formatStr("PAVSession[%03d]", i+1);
		 _pses[i] = new PAVRtpSimSession(hname);

		 unsigned int port = locport + (i * 4);

		 bool bres = _pses[i]->init(locip, port);
		 if(!bres)
		 {
			printf("# %s(%d):%s : failed to init session(%s)!\n", __FILE__, __LINE__, __FUNCTION__, hname.c_str());
			return false;
		 }
		
		 port = rmtport + (i * 4);

		 _pses[i]->setPlayFile("test.3gp", "3gp");
		 
		 _pses[i]->setRmtA(codec, rmtip, port, PT_AUDIO1, PT_AUDIO2);
		 _pses[i]->start();
		 

		 if(i%100 == 0) {
			wname = formatStr("PAVSesWorker[%03d]", i);
			addWorker(wname, 5, 2048, true);
		 }
		 addHandler(wname, _pses[i]);

	  }
	  return true;
   }

   void print() 
   {
	  int pktTxA = 0;
	  int pktRxA = 0;
	  float sum = 0.;

	  int tpktTx = 0;
	  int tpktRx = 0;
	  float  tdelay = 0;
	  for(int i=0; i<_sessions; i++)
	  {
		 pktTxA = _pses[i]->pktTxA();
		 pktRxA = _pses[i]->pktRxA();
		
		 float delayA = _pses[i]->delayA();
		 if(i==0) 
			printf("# %03d : RTP Audio Packet Tx : %d, Rx : %d, Avg Delay : %f msec\n", i+1, pktTxA, pktRxA, delayA);
		 
		 tpktTx += pktTxA;
		 tpktRx += pktRxA;
		 tdelay += delayA;
	  }
	  printf("Total Packet Tx : %d, Rx : %d, Avg Delay : %f msec\n", tpktTx, tpktRx, tdelay/_sessions);
   }

private:
   PAVRtpSimSession ** _pses;
   unsigned int _sessions;

};

int main(int argc, char ** argv)
{


   int mode     = atoi(argv[1]);
   int sessions = atoi(argv[2]);
   int locport = atoi(argv[4]);
   int rmtport = atoi(argv[6]);

   PAVRtpModule  * prtp = NULL;
   PAVRtpSimModule  * prtpsim = NULL;

   if(mode == 1) {
	prtpsim = new PAVRtpSimModule("PAVRtpSimModule");
	bool bres = prtpsim->init(sessions, argv[3], locport, argv[5], rmtport);

	//0. initialize rtp sockets  
	if(!bres)
	{
		printf("# failed to init rtp session!\n");
		exit(1);
	}

	struct timeval tvStartTime, tvCurTime;
	printf("# Initialize RTP Simulator\n");
	gettimeofday(&tvStartTime, NULL);
	printf("# Running RTP Simulator!\n");

	while(true)
	{
		msleep(5000);

		gettimeofday(&tvCurTime, NULL);
		int interval = PDIFFTIME(tvCurTime, tvStartTime);
		prtpsim->print();
		if(interval > 65000)
		{
			prtpsim->print();
			break;
		}
		printf("# elapsed time : %d msec\n", interval);;
	}
   } else {
	prtp = new PAVRtpModule("PAVRtpSimModule");

	bool bres = prtp->init(sessions, argv[3], locport, argv[5], rmtport);

	//0. initialize rtp sockets  
	if(!bres)
	{
		printf("# failed to init rtp session!\n");
		exit(1);
	}
	printf("# Running Media Processing Agent!\n");
	while(true)
	{
		msleep(5000);
	}
   }


 
   return 0;
}
