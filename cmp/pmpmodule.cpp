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

class PRtpTrans : public PHandler
{
private: 
  PRtpSocket _rtpSock;
  char       _pkt[1600];

  PMutex     _mutex;

public:
  PRtpTrans(const std::string & name) {

  }

  virtual ~PRtpTrans() {

  }

  bool init(const std::string & ipLoc, unsigned int portLoc)
  {
    PAutoLock lock(_mutex);
    return _rtpSock.init(ipLoc, portLoc);
  }

  bool final()
  {
    PAutoLock lock(_mutex);
    _rtpSock.final();
  }

  bool setRmt(const std::string & ipRmt, unsigned int portRmt) {
    PAutoLock lock(_mutex);
    return _rtpSock.setRmt(ipRmt, portRmt);
  }

  bool proc() 
  {
    std::string ipRmt;
    int portRmt;
    PAutoLock lock(_mutex);
    while(true) {
      int len = _rtpSock.recv(_pkt, sizeof(_pkt), ipRmt, portRmt);

      if(len<=0) break;
      printf("# %s RX PKT len=%d, ip=%s, port=%d\n", _name.c_str(), len, ipRmt.c_str(), portRmt);
      _rtpSock.send(_pkt, len);

    }
    return false;
  }
  bool proc(int id, const std::string & name, PEvent *spEvent) {return false;}

};


class PRtpSim : public PHandler
{
private: 
  PRtpSocket _rtpSock;
  char       _pktTx[1600];
  char       _pktRx[1600];

  int        _countTx;
  int        _countRx;
  int        _maxPkts;

  unsigned int _curDelay;
  float        _umDelay;
  float        _avgDelay;

  struct timeval _timeTx; 
  unsigned int * _timeDelay;

  PMutex     _mutex;

public:
  PRtpSim(const std::string & name) {
    _timeTx = NULL;
    _timeDelay = NULL;
  }

  virtual ~PRtpSim() {

  }
  
  bool init(const std::string & ipLoc, unsigned int portLoc, int maxPkts)
  {
    PAutoLock lock(_mutex);

    memset(_pktTx, 0, sizeof(_pktTx));
    memset(_pktRx, 0, sizeof(_pktRx));
    struct RTPHEADER * phdr = (struct RTPHEADER *)_pktTx;
    phdr->version = 0; 
    _count = 0;
    _maxPkts = maxPkts;


    _timeTx = new struct timeval[maxPkts];
    _timeDelay = new unsigned int[maxPkts];


    return _rtpSock.init(ipLoc, portLoc);
  }

  bool final()
  {
    PAutoLock lock(_mutex);

    if(_timeTx) delete [] _timeTx;
    if(_timeDelay) delete [] _timeDelay;
    _rtpSock.final();
  }

  bool setRmt(const std::string & ipRmt, unsigned int portRmt) {
    PAutoLock lock(_mutex);
    
    bool bres = _rtpSock.setRmt(ipRmt, portRmt);
    _procRx();

    _countTx = 0;
    _countRx = 0;

    return bres;
  }

  bool proc() {
    _procTx();
    _procRx();
  }

  bool _procRx() 
  {
    std::string ipRmt;
    int portRmt;
    PAutoLock lock(_mutex);
    while(_countRx < _maxPkts) {
      int len = _rtpSock.recv(_pktRx, sizeof(_pktRx), ipRmt, portRmt);

      if(len<=0) break;
      printf("# %s RX PKT len=%d, ip=%s, port=%d\n", _name.c_str(), len, ipRmt.c_str(), portRmt);


      struct RTPHEADER * phdr = (struct RTPHEADER *)_pktRx;

      _countRx = phder->seqnumber; 

      struct timeval tvCurTime;
      gettimeofday(&tvCurTime, NULL);

      _timeDelay[_countRx-1] = PDIFFTIME(tvCurTime, _timeTx[_countRx-1]);  

      _curDelay = _timeDelay[_countRx-1];
      _sumDelay += (float)_curDelay;
      _avgDelay = _sumDelay / (float)_maxPkts;
    }
    return false;
  }

  bool _procTx() {
    struct timeval tvCurTime;
    gettimeofday(&tvCurTime, NULL);
    int pktCount  = (PDIFFTIME(tvCurTime, _tvStartTime) / 20) - _pktCount ;
    _pktCount = pktCount;

    while(pktCount--) {
      struct RTPHEADER * phdr = (struct RTPHEADER *)_pktTx;

      if(_countTx >= _maxPkts) {
        _bdoneTx = true;
        break;
      }
      _countTx++;
      phdr->seqnumber == _count;
      phdr->timestamp+= 160;

      _rtpSock.send(_pktTx, 160+12);

    }
  }

  bool isDone() {
    return _countRx >= _maxPkts
  }

  float avgDelay() { return _avgDelay; };
  unsigned int curDelay() { return _curDelay; };

  bool proc(int id, const std::string & name, PEvent *spEvent) {return false;}

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

		 _pses[i]->setRmt(rmtip, port);
		 //_pses[i]->start();
		 

		 if(i%100 == 0) {
			wname = formatStr("PAVSesWorker[%03d]", i);
			addWorker(wname, 5, 2048, true);
		 }
		 addHandler(wname, _pses[i]);

	  }
	  return true;
   }


private:
   PRtpTrans ** _pses;
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
   PRtpSim ** _pses;
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
