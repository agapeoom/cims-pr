#include <string>
#include <iostream>

#include "pbase.h"
#include "pmodule.h"

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

int main(int argc, char ** argv)
{
   int sessions = atoi(argv[2]);
   int locport = atoi(argv[4]);
   int rmtport = atoi(argv[6]);

   PAVRtpModule  * prtp = NULL;

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

   return 0;
}
