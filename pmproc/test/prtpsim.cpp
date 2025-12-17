#include <string>
#include <iostream>

#include "pbase.h"
#include "pmodule.h"
#include "prtpsock.h"

class PRtpSim : public PHandler
{
private: 
  PRtpSocket _rtpSock;
  char       _pktTx[1600];
  char       _pktRx[1600];

  unsigned int _time;
  int        _countTx;
  int        _countRx;
  int        _maxPkts;

  unsigned int _curDelay;
  //float        _sumDelay;
  //float        _avgDelay;

  struct timeval _tvStartTime; 
  struct timeval * _timeTx; 
  unsigned int * _timeDelay;

  PMutex     _mutex;

public:
  PRtpSim(const std::string & name) : PHandler(name) {
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
    _countTx = 0;
    _countRx = 0;
    _maxPkts = maxPkts;
    _curDelay = 0;

    _timeTx = new struct timeval[maxPkts];
    _timeDelay = new unsigned int[maxPkts];

    gettimeofday(&_tvStartTime, NULL);

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
    _time = 0;

    gettimeofday(&_tvStartTime, NULL);
    return bres;
  }

  bool proc() {
    PAutoLock lock(_mutex);
    _procTx();
    _procRx();
  }

  bool _procRx() 
  {
    std::string ipRmt;
    int portRmt;
    while(_countRx < _maxPkts) {
      int len = _rtpSock.recv(_pktRx, sizeof(_pktRx), ipRmt, portRmt);
      if(len<=0) break;


      struct RTPHEADER * phdr = (struct RTPHEADER *)_pktRx;

      int index = ntohs(phdr->seqnumber);


      struct timeval tvCurTime;
      gettimeofday(&tvCurTime, NULL);

      _timeDelay[index-1] = PDIFFTIME(tvCurTime, _timeTx[index-1]);  
      //printf("# %s RX PKT len=%d, ip=%s, port=%d, pt=%d, seq=%d, ts=%u, delay=%dmsec\n", 
      //        name().c_str(), len, ipRmt.c_str(), portRmt,
      //        phdr->payloadtype, ntohs(phdr->seqnumber), ntohl(phdr->timestamp), _timeDelay[index-1]);

      _curDelay = _timeDelay[index-1];
      //_sumDelay += (float)_curDelay;
      //_avgDelay = _sumDelay / (float)_maxPkts;

      _countRx = index;
    }
    return false;
  }

  bool _procTx() {
    struct timeval tvCurTime;
    gettimeofday(&tvCurTime, NULL);
    int pktCount  = (PDIFFTIME(tvCurTime, _tvStartTime) / 20) - _countTx ;

    //if(pktCount>0 && _countTx < _maxPkts) {
    //  printf("# pkt count : %d\n", pktCount);
    //}
    while(pktCount-- && _countTx < _maxPkts) {
      struct RTPHEADER * phdr = (struct RTPHEADER *)_pktTx;

      _countTx++; 
      _time += 160;
      phdr->seqnumber = htons(_countTx);
      phdr->timestamp = htonl(_time);

      gettimeofday(&_timeTx[_countTx-1], NULL);

      int ret = _rtpSock.send(_pktTx, 160+12);
      //printf("# %s TX PKT ret=%d, len=%d, pt=%d, seq=%d, ts=%u\n", 
      //        name().c_str(), ret, 160+12, 
      //        phdr->payloadtype, htons(phdr->seqnumber), htonl(phdr->timestamp));

    }
  }

  bool isDone() {
    return _countRx >= _maxPkts;
  }

  unsigned int pktRx() { return _countRx; };
  unsigned int pktTx() { return _countTx; };
  //float avgDelay() { return _avgDelay; };
  unsigned int curDelay() { return _curDelay; };

  bool proc(int id, const std::string & name, PEvent *spEvent) {return false;}

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

   bool init(int sessions, int pkts, 
             const std::string locip, unsigned int locport, 
             const std::string rmtip, unsigned int rmtport)
   {
	  _sessions = sessions;
      _maxDelay = 0;
      _countCheck = 0;
      _sumDelay = 0;

	  _pses  = new PRtpSim * [sessions];
	  std::string wname; 
	  for(int i=0; i<sessions; i++)
	  {
		 std::string hname = formatStr("PSimSes[%03d]", i+1);
		 _pses[i] = new PRtpSim(hname);

		 unsigned int port = locport + (i * 4);

		 bool bres = _pses[i]->init(locip, port, pkts); // pkts : max packets

		 if(!bres)
		 {
			printf("# %s(%d):%s : failed to init session(%s, port=%d)!\n", __FILE__, __LINE__, __FUNCTION__, hname.c_str(), port);
			return false;
		 }
		
		 port = rmtport + (i * 4);

		 _pses[i]->setRmt(rmtip, port); 
		 

		 if(i%100 == 0) {
			wname = formatStr("PWorker[%03d]", i);
			addWorker(wname, 5, 2048, true);
		 }
		 addHandler(wname, _pses[i]);

	  }
	  return true;
   }

   bool check() 
   {
	  float sumDelay = 0.;
	  unsigned int curDelay = 0.;
      unsigned int pktTx = 0;
      unsigned int pktRx = 0;

      bool done = true;
	  for(int i=0; i<_sessions; i++)
	  {
         pktTx += _pses[i]->pktTx();
         pktRx += _pses[i]->pktRx();
		 int delay = _pses[i]->curDelay();
         _maxDelay = delay > _maxDelay  ? delay : _maxDelay;

		 sumDelay += delay;
        
         done = done && _pses[i]->isDone();
	  }
      _sumDelay += sumDelay/_sessions;
      _countCheck++;

      curDelay = (unsigned int) (sumDelay / _sessions);

	  printf("Packet: Tx=%d, Rx=%d | Delay: Avg=%.2fmsec, Current(Last):%dmsec\n", 
             pktTx, pktRx, 
             _sumDelay/_countCheck, curDelay);

      return done;
   }

private:
   PRtpSim ** _pses;
   unsigned int _sessions;
   unsigned int _maxDelay;
   float _sumDelay;
   unsigned int _countCheck;
};

int main(int argc, char ** argv)
{
   int sessions = atoi(argv[1]);
   int pkts = atoi(argv[2]);
   int locport = atoi(argv[4]);
   int rmtport = atoi(argv[6]);

   PAVRtpSimModule  * prtpsim = NULL;

   prtpsim = new PAVRtpSimModule("PAVRtpSimModule");
   bool bres = prtpsim->init(sessions, pkts, argv[3], locport, argv[5], rmtport);

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

   int curCount = 0;
   while(true)
   {
       msleep(50);
       gettimeofday(&tvCurTime, NULL);
       int elapse = PDIFFTIME(tvCurTime, tvStartTime);
       int checkCount  = elapse / 1000;
     
       //printf("checkCount = %d, curCount=%d\n", checkCount, curCount);
       if(checkCount <= curCount) continue;

       curCount = checkCount;
       
       if(prtpsim->check()) { 
           break;
       }

   }

 
   return 0;
}
