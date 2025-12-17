#include <map>
#include <deque>
#include <string>
#include <fstream>


#include "pbase.h"
#include "pmodule.h"
#include "phandler.h"

#include "PAVCodec.h"
#include "PAVFormat.h"
#include "PAVSession.h"

PAVSession::PAVSession(const std::string & name) : PHandler(name) {};
PAVSession::~PAVSession() {};

bool PAVSession::init(const std::string & ip, unsigned int port)
{
   ipLoc = ip;
   portLocA = port;
   portLocV = port + 2;

   bool bres; 
   bres = rtpA.init(ipLoc, portLocA);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init audio rtp!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   bres = rtpV.init(ipLoc, portLocV);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init video rtp!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   _bstart = false;
   _baudio = false;
   _bvideo = false;
   _bplay  = false;
   _brecord = false;

   return true;
}

void PAVSession::final()
{
   _reset();
   rtpA.final();
   rtpV.final();
}


bool PAVSession::join(unsigned int gid, int priority)
{
   PAutoLock lock(_mutex);
   _gid = gid;
   _priority = priority;
   return true;
}

bool PAVSession::start()
{
   PAutoLock lock(_mutex);
   if(_baudio || _bvideo)
   {
	  _bstart = true;
	  return true;
   }
   return false;
}

bool PAVSession::setPlayFile(const std::string fname, const std::string fmt)
{
   PAutoLock lock(_mutex);
   bool bres = _fileRx.init(fname, fmt, PAVFormat::RX);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init Play-File File !\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }
   
   _bplay = true;

   return true;
}

bool PAVSession::setRecordFile(const std::string fname, const std::string fmt)
{
   PAutoLock lock(_mutex);
   bool bres = _fileTx.init(fname, fmt, PAVFormat::TX);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init Record-File !\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   if(_baudio)
   {
	  bool bres = false;
	  if(!_fileTx.addstream(_paramA)) // set param.stream_index by addstream
	  {
		 printf("# %s(%d):%s : failed to init  audio addstream!\n", __FILE__, __LINE__, __FUNCTION__);
		 return false;
	  }
   }

   if(_bvideo)
   {
	  bool bres = false;
	  if(!_fileTx.addstream(_paramV)) // set param.stream_index by addstream
	  {
		 printf("# %s(%d):%s : failed to init  audio addstream!\n", __FILE__, __LINE__, __FUNCTION__);
		 return false;
	  }
   }

   _brecord = true;
   return true;
}

bool PAVSession::setRmtA(PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt1, unsigned char pt2)
{
   PAutoLock lock(_mutex);

   bool bres;

   _paramA.init(codec);
   strncpy(_paramA.ip, ip.c_str(), sizeof(_paramA.ip));
   _paramA.port = port;
   _paramA.pt1 = pt1;
   _paramA.pt2 = pt2;

   rxBufA.init();
   txBufA.init();

   bres =  rtpA.setRmt(_paramA);
   if(!bres)
   {
	 return false;
   } 

   //audio codec init ...
   bres = decA.init(_paramA);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init audio decoder!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   bres = resamplerRx.init(_paramA.samplerate, _paramA.channels, 16000, 1);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init resampler rx!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   //audio mix-filter  init ...
   bres = mixA.init(_gid);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init audio mix filter!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }
   
   bres = resamplerTx.init(16000, 1, _paramA.samplerate, _paramA.channels);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init resampler rx!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }


   bres = encA.init(_paramA);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init  audio encoder!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   _baudio = true;
   return true;
}


bool PAVSession::setRmtV(PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt)
{
   PAutoLock lock(_mutex);
   
   _paramV.init(codec);
   strncpy(_paramV.ip, ip.c_str(), sizeof(_paramV.ip));
   _paramV.port = port;
   _paramV.pt1 = pt;
   _paramV.width = PAV_VIDEO_WIDTH; //width;
   _paramV.height = PAV_VIDEO_HEIGHT; //height;
   _paramV.fps = PAV_VIDEO_FPS; //height;


   bool bres = false;

   rxBufV.init();
   txBufV.init();

   bres = decV.init(_paramV);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init video decoder!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   bres = mixV.init(_gid, _priority, _paramV);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init video mix filter!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   bres = encV.init(_paramV);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init video encoder!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   }

   bres =  rtpV.setRmt(_paramV);
   if(!bres)
   {
	  printf("# %s(%d):%s : failed to init video rtp!\n", __FILE__, __LINE__, __FUNCTION__);
	  return false;
   } 

   _bvideo = true;
   return true;
}

void PAVSession::stop()
{
   PAutoLock lock(_mutex);

   _reset();

}

void PAVSession::_reset()
{
   _bstart = false;
   
   if(_baudio) 
   {
	  _baudio = false;
	  decA.final();
	  encA.final();
	  mixA.final();
   } 
   if(_bvideo)
   {
	  _bvideo = false;
	  decV.final();
	  encV.final();
	  mixV.final();
   }
   if(_bplay) 
   {
	  _fileRx.final();
   }
   if(_brecord) 
   {
	  _fileTx.final();
   }
}

bool PAVSession::proc() 
{
   PAutoLock lock(_mutex);
   if(!_bstart) return false;

   _procRxRtpA();
   _procRxRtpV();
   
   _procRxFile();

   _procTxRtpA();
   _procTxRtpV();

   return true;
};

bool PAVSession::_procRxFile() 
{
   if(!_bplay) false;

   PAVBuffer rxBuf;
   rxBuf.init();
   
   while(_fileRx.get(rxBuf)) 
   {
	  //printf("# RX_FILE : %s\n", rxBuf.str().c_str());

	  if(_baudio && rxBuf._type == PAVTYPE_AUDIO) 
	  {
		 decA.put(rxBuf);

		 //msleep(10);
		 while(decA.get(rxBuf))
		 {
#if 0
			{
			   std::string fname = formatStr("dump1-%d.pcm", portLocA);
			   std::ofstream file;
			   file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
			   file.write(rxBuf._frame->data[0], rxBuf._frame->nb_samples *2); 
			   printf("# %s : file write(len=%d)\n", fname.c_str(), rxBuf._frame->nb_samples *2);
			   file.close();
			}
#endif 
			resamplerRx.put(rxBuf);
			while(resamplerRx.get(rxBuf))
			{
#if 0
			   {
				  std::string fname = formatStr("dump2-%d.pcm", portLocA);
				  std::ofstream file;
				  file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
				  file.write(rxBuf._frame->data[0], rxBuf._frame->nb_samples *2); 
				  printf("# %s : file write(len=%d)\n", fname.c_str(), rxBuf._frame->nb_samples *2);
				  file.close();
			   }
			   printf("# RX_DEC_A : %s\n", rxBuf.str().c_str());
#endif 
			   mixA.put(rxBuf);
			}
		 }
	  } 
	  else if(_bvideo && rxBuf._type == PAVTYPE_VIDEO)
	  {
		 decV.put(rxBuf);

		 while(decV.get(rxBuf))
		 {
			//printf("# RX_DEC_V : \n");
			mixV.put(rxBuf);
		 }
	  }
	  else if(rxBuf._type == PAVTYPE_EOS) 
	  {
		 _fileRx.rewind();
		 break;
	  }
	  
   }
   return true;
};


bool PAVSession::proc(int id, const std::string & name, PEvent *spEvent)
{
   return false;
}

bool PAVSession::_procRxRtpA()
{
   if(!_baudio) return false;

   while(rtpA.get(rxBufA)) 
   {
	  if(_bplay) continue;

	  //printf("# RX_RTP_A : %s\n", rxBufA.str().c_str());
	  decA.put(rxBufA);
	  
	  while(decA.get(rxBufA))
	  {
#if 0
		 {
			std::string fname = formatStr("dump1-%d.pcm", portLocA);
			std::ofstream file;
			file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
			file.write(rxBufA._frame->data[0], rxBufA._frame->nb_samples *2); 
			printf("# %s : file write(len=%d)\n", fname.c_str(), rxBufA._frame->nb_samples *2);
			file.close();
		 }
#endif 
		 resamplerRx.put(rxBufA);
		 while(resamplerRx.get(rxBufA))
		 {
#if 0
		 {
			std::string fname = formatStr("dump2-%d.pcm", portLocA);
			std::ofstream file;
			file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
			file.write(rxBufA._frame->data[0], rxBufA._frame->nb_samples *2); 
			printf("# %s : file write(len=%d)\n", fname.c_str(), rxBufA._frame->nb_samples *2);
			file.close();
		 }
#endif 
			//printf("# RX_DEC_A : \n");
			mixA.put(rxBufA);
		 }
	  }
   }

   return true;
}

bool PAVSession::_procTxRtpA()
{
   if(!_baudio) return false;

   if(mixA.get(txBufA))
   {
	  //printf("# TX_RESAMPLE_A : %s\n", txBufA.str().c_str());
	  resamplerTx.put(txBufA);
	  while(resamplerTx.get(txBufA))
	  {
		 //printf("# TX_ENC_A : %s\n", txBufA.str().c_str());

#if 0
		 std::ofstream file;
		 file.open("dump.pcm", ios::binary|ios::ate|ios::app);
		 //file.seekp(0, ios::end); 
		 file.write(txBufA.frame()->data[0], txBufA.frame()->linesize[0]);
		 printf("file write(len=%d)\n", txBufA.frame()->linesize[0]);
		 file.close();
#endif 
	     //printf("# TX_ENC_A : \n");
		 encA.put(txBufA);

		 while(encA.get(txBufA))
		 {
			//printf("# TX_RTP_A : %s\n", txBufA.str().c_str());
			rtpA.put(txBufA);

			if(_brecord)
			{
			   _fileTx.put(txBufA);
			}
		 }
		 break;
	  }
   }
   return true;
}


bool PAVSession::_procRxRtpV()
{
   if(!_bvideo) return false;
#if 1
   while(rtpV.get(rxBufV)) 
   {
	  if(_bplay) continue;
	  //printf("# RX_RTP_V : %s\n", rxBufV.str().c_str());
	  decV.put(rxBufV);

	  while(decV.get(rxBufV))
	  {
		 //printf("# RX_DEC_V : \n");
		 mixV.put(rxBufV);
	  }
   }
#endif 

   return true;
}

bool PAVSession::_procTxRtpV()
{
   if(!_bvideo) return false;

   if(mixV.get(txBufV))
   {
	  //printf("# TX_ENC_V : %s\n", txBufV.str().c_str());
	  //encV.put(txBufV);
	  
	  //while(encV.get(txBufV))
	  //{
#if 0
		 {
			FILE *fp = fopen("data2.h264", "ab+");
			if (fp != NULL)
			{
			   AVPacket * pkt = txBufV._packet; 
			   //AVPacket * pkt = txBufV.packet();
			   fwrite(pkt->data, 1, pkt->size, fp);
			   fclose(fp);
			}

		 }
#endif
		 //printf("# TX_RTP_V : %s\n", txBufV.str().c_str());
		 rtpV.put(txBufV);

		 if(_brecord)
		 {
			_fileTx.put(txBufV);
		 }
	  //}
   }

   return true;
}

////////////////////////////////////////////

PAVSesModule::PAVSesModule(const std::string & name) : PModule(name){};
PAVSesModule::~PAVSesModule() {};

bool PAVSesModule::init(unsigned int groups, unsigned int sessions, const std::string ip, unsigned int baseport)
{
   _groups = sessions;
   _sessions = sessions;

   av_register_all();
   //avcodec_register_all();
   //avformat_network_init();

   bool bres = false;
   for(int i=0; i<sessions; i++)
   {
	  std::string hname = formatStr("PAVSession[%03d]", i);
	  PAVSession * pses = new PAVSession(hname);
   
	  _sesMap[i] = pses;
	  unsigned int port = baseport+(i*4);
	  bres = _sesMap[i]->init(ip, port);
			 
	  if(!bres)
	  {
		 printf("# %s(%d):%s : failed to init session(%s)!\n", __FILE__, __LINE__, __FUNCTION__, hname.c_str());
		 return false;
	  }

	  std::string wname = formatStr("PAVSesWorker[%03d]", i);
	  addWorker(wname, 20, 2048, true);
	  addHandler(wname, _sesMap[i]);

	  printf("#Start : %s/%s - ip:%s, port=%d\n", hname.c_str(), wname.c_str(), ip.c_str(), port);
	  _idle.push_back(i);
   }

   return true;
}

bool PAVSesModule::alloc(unsigned int uid, unsigned int gid, int priority) 
{
   if(_used.find(uid) != _used.end()) 
   {

	  return false; 
   }

   if(_idle.empty())
   {
	  return false;
   }

   unsigned int index = _idle.front();
   _idle.pop_front();
   _used[uid] = index; 

   _sesMap[_used[uid]]->join(gid, priority);

   printf("# Alloc Session!(uid=%d)\n", uid);

   return true;
}

bool PAVSesModule::setRmtA(unsigned int uid, PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt1, unsigned char pt2)
{
   if(_used.find(uid) == _used.end()) 
   {
	  return false;
   }
  
   return _sesMap[_used[uid]]->setRmtA(codec, ip, port, pt1, pt2);;
}

bool PAVSesModule::setRmtV(unsigned int uid, PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt)
{
   if(_used.find(uid) == _used.end()) 
   {
	  return false;
   }
  
   return _sesMap[_used[uid]]->setRmtV(codec, ip, port, pt);
}

bool PAVSesModule::setPlayFile(unsigned int uid, const std::string fname, const std::string fmt)
{
   if(_used.find(uid) == _used.end()) 
   {
	  return false;
   }
  
   return _sesMap[_used[uid]]->setPlayFile(fname, fmt);
}

bool PAVSesModule::setRecordFile(unsigned int uid, const std::string fname, const std::string fmt)
{
   if(_used.find(uid) == _used.end()) 
   {
	  return false;
   }
  
   return _sesMap[_used[uid]]->setRecordFile(fname, fmt);
}



bool PAVSesModule::start(unsigned int uid)
{
   if(_used.find(uid) == _used.end()) 
   {
	  return false;
   }
  
   return _sesMap[_used[uid]]->start();
}



bool PAVSesModule::dealloc(unsigned int uid)
{
   if(_used.find(uid) == _used.end()) 
   {
	  printf("# %s(%d):%s : failed to Dealloc Session!(uid=%d, not used)!\n", __FILE__, __LINE__, __FUNCTION__, uid);
	  return false;
   }

   _idle.push_back(_used[uid]);

   _sesMap[_used[uid]]->stop();;

   _used.erase(uid);
   printf("# Dealloc Session!(uid=%d)\n", uid);
   return true;
}


static std::string nullstr = "";

std::string  & PAVSesModule::ipRtp(unsigned int uid)
{
   if(_used.find(uid) == _used.end()) 
   {
	  return nullstr;
   }
   return _sesMap[_used[uid]]->ipRtp();;
}

unsigned int PAVSesModule::portRtpA(unsigned int uid)
{
   if(_used.find(uid) == _used.end()) 
   {
	  return 0;
   }
   return _sesMap[_used[uid]]->portRtpA();;
}

unsigned int PAVSesModule::portRtpV(unsigned int uid)
{
   if(_used.find(uid) == _used.end()) 
   {
	  return 0;
   }
   return _sesMap[_used[uid]]->portRtpV();;
}

void PAVSesModule::final() 
{

}



