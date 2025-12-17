#include <string>


#include "PMPManager.h"


PMPSession::PMPSession() {}
PMPSession::~PMPSession()  {}

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


bool PMPManager::alloc(unsigned int uid, unsigned int gid)
{
   return ((PAVSesModule *)_ctx)->alloc(uid, gid);
}


bool PMPManager::dealloc(unsigned int uid, unsigned int gid)
{
   return ((PAVSesModule *)_ctx)->dealloc(uid);
}

/*
bool PMPManager::join(unsigned int uid, unsigned int gid)
{
   return ((PAVSesModule *)_ctx)->join(uid, gid);
}
*/

bool PMPManager::setRmtA(PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt1, unsigned char pt2)
{  
   return ((PAVSesModule *)_ctx)->setRmtA(uid, codec, ip, port, pt1, pt2);
}

bool PMPManager::setRmtV(PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt, int width, int height, int bitrate)
{
   return ((PAVSesModule *)_ctx)->setRmtV(uid, codec, ip, port, pt, width, height, bitrate);
}

std::string & PMPManager::ipRtpA(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->ipRtpA(uid);
}

unsigned int PMPManager::portRtpA(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->portRtpA(uid);
}

std::string & PMPManager::ipRtpV(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->ipRtpV(uid);
}

unsigned int PMPManager::portRtpV(unsigned int uid)
{
   return ((PAVSesModule *)_ctx)->portRtpV(uid);
}


