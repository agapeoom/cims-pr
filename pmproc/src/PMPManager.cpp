#include <string>



#include "PAVSession.h"
#include "PMPManager.h"

static PMPManager * PMPManager::_mgr = NULL;

PMPManager::PMPManager() : _ctx(NULL)
{

}

PMPManager::~PMPManager() 
{
   final();
}


bool PMPManager::init(unsigned int groups, unsigned int sessions, unsigned int sesPerGroup, const std::string ip, unsigned int baseport)
{
   if(_ctx) 
   { 
	  return false;
   }
   
   _ctx = new PAVSesModule("PAVSesModule");
   if(!_ctx)
   {
	  return false;
   }

   bool bres = ((PAVSesModule *)_ctx)->init(groups, sessions, ip, baseport); 

   if(!bres) 
   {  
	  final(); 
	  return false;
   }
   return true;
}

bool PMPManager::final()
{
   if(_ctx) {
	  delete (PAVSesModule*)_ctx;
	  _ctx = NULL;
	  return true;
   }
   return false;
}


bool PMPManager::alloc(unsigned int uid, unsigned int gid, int priority)
{
   return ((PAVSesModule *)_ctx)->alloc(uid, gid, priority);
}

bool PMPManager::start(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->start(uid);
}

bool PMPManager::dealloc(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->dealloc(uid);
}

/*
bool PMPManager::join(unsigned int uid, unsigned int gid)
{
   return ((PAVSesModule *)_ctx)->join(uid, gid);
}
*/

bool PMPManager::setRmtA(unsigned int uid, PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt1, unsigned char pt2)
{  
   return ((PAVSesModule *)_ctx)->setRmtA(uid, codec, ip, port, pt1, pt2);
}

bool PMPManager::setRmtV(unsigned int uid, PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt)
{
   return ((PAVSesModule *)_ctx)->setRmtV(uid, codec, ip, port, pt);
}

bool PMPManager::setPlayFile(unsigned int uid, const std::string fname, const std::string fmt)
{
   return ((PAVSesModule *)_ctx)->setPlayFile(uid, fname, fmt);
}

bool PMPManager::setRecordFile(unsigned int uid, const std::string fname, const std::string fmt)
{
   return ((PAVSesModule *)_ctx)->setRecordFile(uid, fname, fmt);
}



std::string & PMPManager::ipRtp(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->ipRtp(uid);
}

unsigned int PMPManager::portRtpA(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->portRtpA(uid);
}


unsigned int PMPManager::portRtpV(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->portRtpV(uid);
}


