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

	  _pses  = new PRtpSim * [sessions];
	  std::string wname; 
	  for(int i=0; i<sessions; i++)
	  {
		 std::string hname = formatStr("PAVSession[%03d]", i+1);
		 _pses[i] = new PRtpSim(hname);

		 unsigned int port = locport + (i * 4);

		 bool bres = _pses[i]->init(locip, port, pkts); // pkts : max packets

		 if(!bres)
		 {
			printf("# %s(%d):%s : failed to init session(%s)!\n", __FILE__, __LINE__, __FUNCTION__, hname.c_str());
			return false;
		 }
		
		 port = rmtport + (i * 4);

		 _pses[i]->setRmt(codec, rmtip); 
		 

		 if(i%100 == 0) {
			wname = formatStr("PAVSesWorker[%03d]", i);
			addWorker(wname, 5, 2048, true);
		 }
		 addHandler(wname, _pses[i]);

	  }
	  return true;
   }

   void check() 
   {
	  float curDelay = 0.;
	  float  avgDelay = 0;
      unsigned int pktTx = 0;
      unsigned int pktRx = 0;

      _bdone = true;
	  for(int i=0; i<_sessions; i++)
	  {
         pktTx += _pses[i]->pktTx()
         pktRx += _pses[i]->pktRx()
		 curDelay += _pses[i]->curDelay();
		 avgDelay += _pses[i]->avgDelay();

         _bdone = bdone && _pses[i]->isDone();
	  }
	  printf("Total Packet Tx: %d, Rx: %d, Avg Delay: %fmsec, Current Delay: %fmsec\n", 
             pktTx, pktRx, 
             avgDelay/_sessions, curDelay/_sessions);
   }

private:
   PRtpSim ** _pses;
   unsigned int _sessions;

};

int main(int argc, char ** argv)
{
   int sessions = atoi(argv[1]);
   int locport = atoi(argv[4]);
   int rmtport = atoi(argv[6]);

   PAVRtpSimModule  * prtpsim = NULL;

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
} 

 
   return 0;
}
