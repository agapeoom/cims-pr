#ifndef __PMPManager__
#define __PMPManager__

#include "PMPBase.h"

class PMPManager
{
public:
   // 1. 
   PMPManager(); // assigned  local rtp 
   // 4. 
   virtual ~PMPManager();  // 

   static PMPManager * create(unsigned int groups, 
							  unsigned int sessions, unsigned int sesPerGroup,
							  const std::string ip, unsigned int baseport) 
   { 
	  _mgr = new PMPManager();
	  if(!_mgr) return NULL;
	  
	  if(!_mgr->init(groups, sessions, sesPerGroup, ip, baseport))
	  {
		 delete _mgr; 
		 return NULL;
	  }

	  return _mgr;
   }

   static PMPManager * mgr() { return _mgr; }

   // 0. initialize all rtp socket & resource
   bool init(unsigned int groups, unsigned int sessions, unsigned int sesPerGroup, 
			 const std::string ip, unsigned int baseport);
   bool final();

   // 1. alloc session  
   bool alloc(unsigned int uid, unsigned int gid, int priority);

   // 2. get local-rtp address infomation.
   std::string & ipRtp(unsigned int uid);    
   unsigned int portRtpA(unsigned int uid);
   unsigned int portRtpV(unsigned int uid);

   // 3. set remote-rtp address & media parameters ...
   bool setRmtA(unsigned int uid, PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt, unsigned char ptDtmf);
   bool setRmtV(unsigned int uid, PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt);
  
   bool setPlayFile(unsigned int uid, const std::string fname, const std::string fmt);
   bool setRecordFile(unsigned int uid, const std::string fname, const std::string fmt);

   
   // 4 run rtp session thread ...
   bool start(unsigned int uid);

   // 5. stop rtp session thread & dealloc ..
   bool dealloc(unsigned int uid);

private:
   void		   * _ctx;
   static PMPManager  * _mgr;
};

#endif //__PMPManager__

