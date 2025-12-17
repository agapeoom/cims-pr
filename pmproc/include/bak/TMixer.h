#pragma once

#ifndef __TMixer__
#define __TMixer__
#include <map>
#include <utility>
#include <chrono>
#include <thread>


#include "hug711.h"

#include "base.h"
#include "pbase.h"

#include "TMBase.h"
#include "TCodec.h"
#include "TMBuffer.h"
#include "TRtpProcessor.h"
#include "TMFile.h"
#include "PNumFiles.h"
#include "PAVFile.h"

//#define _MDEBUG
#define   NUM_MIX_DIV1    30
#define   NUM_MIX_DIV2    10

class TGroupMixer
{
private:
   unsigned int _sessions; //max session per group
   unsigned int _frames;
   unsigned int _frameSize;

   unsigned int _gid;  // for logging & debugging
   TJitterBuffer* _pinBuffer;
   TRingBuffer* _poutBuffer;

   TMBuffer   _tmpBuf;
   TMBuffer   _tmpBuf2;

   // 2 step mixing
   unsigned int _index[NUM_MIX_DIV1][NUM_MIX_DIV2];
   TMBuffer    _pBuffer[NUM_MIX_DIV1][NUM_MIX_DIV2];
   TMBuffer    _pMixBuffer[NUM_MIX_DIV1][NUM_MIX_DIV2];
   TMBuffer    _pMidBuffer[NUM_MIX_DIV1];
   TMBuffer   _mixAllBuffer;

   TCODEC	   _codec;

   // for group control
   int			  _bgcode;
   TSharedMFiles  _bgfile; 


   int			  _mcode;
   TSharedMFiles  _mfile;

   bool			  _bRecord;
   PWavWriter	  _recfile;

   bool			  _bOpenListen;
   TRtpComm		  _rtpComm;


   PMutex           _mutex;

   std::map <unsigned int, unsigned int> _usedMap; // session index, buffer Index(virtual session Index)
   std::map <unsigned int, int> _usedCodec; // session index, codec
   std::deque <unsigned int> _idleQue;

public:
   TGroupMixer(const unsigned int& sessions)
   {
      _sessions = sessions;
      _frames = MAX_BUF_FRAMES;
      _frameSize = FRAME_SIZE_LPCM;

	  switch(LPCM_RATE_DEFAULT)
	  {
	  case 8000:
		 _codec = TCODEC_LPCM8K;
		 break;
	  case 16000:
		 _codec = TCODEC_LPCM16K;
		 break;
	  case 32000:
		 _codec = TCODEC_LPCM32K;
		 break;
	  default:
		 _codec = TCODEC_NONE;
		 break;
	  }

      _pinBuffer = new TJitterBuffer[sessions];
      _poutBuffer = new TRingBuffer[sessions]; //+1 for RTSP Streaming
#if 0
      _pmixBuffer = new TMBuffer[sessions + 1];     //+1 for RTSP Streaming
#else 
	  //
	  if((NUM_MIX_DIV1 * NUM_MIX_DIV2) < _sessions) 
	  {
		 LOG(LERR, "TGroupMixer() : Error, can't create group mixer!(_session=%d)", _sessions);
		 exit(-1);
	  }
#endif
//
      for (unsigned int bid=0; bid < _sessions; bid++)
      {
		 _pinBuffer[bid].init(bid, 20);
//         _idleQue.push_back(bid);
      }
	  //init(0);
   }
   ~TGroupMixer()
   {
	  delete [] _pinBuffer;
	  delete [] _poutBuffer;
#if 0 
	  delete [] _pmixBuffer;
#endif
   }

   bool init(unsigned int gid)
   {  
	  LOG(LINF, "TGroupMixer[gid:%d]->init()\n", gid);
      _mutex.lock();
	  _gid = gid;
	  _bgcode = 0;
	  _mcode = 0;
	  _bRecord = false;
	  _bOpenListen = false;

	  _idleQue.clear();
	  _usedMap.clear();
	  _usedCodec.clear();

      for (unsigned int bid=0; bid < _sessions; bid++)
      {
         _idleQue.push_back(bid);
      }
      _mutex.unlock();
	  return true;
   }

   bool final()
   {
	  LOG(LINF, "TGroupMixer[gid:%d]->final()\n", _gid);
      _mutex.lock();
	  _gid = 0;
	  if(_bgcode) 
	  {
		 _bgfile.init();
	  } 
	  if(_bRecord) 
	  {
		 _recfile.close();
	  } 
	  if(_bOpenListen) 
	  {
		 _rtpComm.final();
	  }

	  //_idleQue.clear();
	  //_usedMap.clear();
	  //_usedCodec.clear();

      _mutex.unlock();
	  return true;
   }
	  
   // 
   //  가상의 buffer Index id return
   // encoding 최소화를 위해codec 정보 입력
   // mixing 결과가 동일한 경우 encoding 수행 해서 세션 버퍼에 입력
   // 버퍼에 인코딩 정보로 codec 입력
   bool  alloc(const unsigned int& sid, int codec) 
   {
      _mutex.lock();
      if (_usedMap.find(sid) != _usedMap.end())
      {
		 LOG(LERR, "TGroupMixer[gid:%d]->alloc(sid:%04d) :Error, can't alloc(already used session)\n", _gid, sid);
		 _mutex.unlock();
         return false;
      }

	  if(_idleQue.empty()) 
	  {
		 LOG(LERR, "TGroupMixer[gid:%d]->alloc(sid:%04d) :Error, can't alloc(no idle session)\n", _gid, sid);
		 _mutex.unlock();
         return false;
	  }
      unsigned int bid = _idleQue.front();
      _idleQue.pop_front();
      _usedMap[sid] = bid;
      _usedCodec[sid] = codec;

	  LOG(LINF, "TGroupMixer[gid:%d]->alloc(sid:%04d)-bid:%d\n", _gid, sid, bid);
      _mutex.unlock();
      return true;
   }

   bool  dealloc(const unsigned int& sid) // 가상의 buffer Index id return
   {
      _mutex.lock();
      if (_usedMap.find(sid) == _usedMap.end())
      {
		 LOG(LERR, "TGroupMixer[gid:%d]->dealloc(sid:%04d) :Error, can't dealloc(not used session)\n", _gid, sid);
		 _mutex.unlock();
         return false;
      }
	  unsigned int bid = _usedMap[sid];
      _idleQue.push_back(bid);
      _usedMap.erase(sid);
      _usedCodec.erase(sid);
	  LOG(LINF, "TGroupMixer[gid:%d]->dealloc(sid:%04d)-bid:%d\n", _gid, sid, bid);
  	  _mutex.unlock();
      return true;
   }

  // control 
   bool bgm(int code)
   {
	  LOG(LINF, "TGroupMixer[gid:%d]->bgm(code=%d)\n", _gid, code);
      _mutex.lock();
	  _bgcode = code; 
	  _bgfile.init();
      _mutex.unlock();
	  return true;
   }

   bool ment(int code)
   {
	  LOG(LINF, "TGroupMixer[gid:%d]->ment(code=%d)\n", _gid, code);
      _mutex.lock();
	  _mcode = code; 
	  _mfile.init();
      _mutex.unlock();
	  return true;
   }

   bool record(bool on, const std::string & fname)
   {
	  bool bRes = false;
	  LOG(LINF, "TGroupMixer[gid:%d]->record(%s, fname=%s)\n", 
						    _gid, on?"on":"off", fname.c_str());
      _mutex.lock();
	  _bRecord = on;
	  if(_bRecord) 
	  {
		 bRes = _recfile.open(fname);
		 if(!bRes) _bRecord = false;
	  } 
	  else 
	  {
		 bRes = _recfile.close();
	  }
      _mutex.unlock();

	  return bRes;
   }

   bool cast(bool on, 
			   const std::string ipLoc, unsigned int portLoc,
			   const std::string ipRmt, unsigned int portRmt)
   {
	  bool bRes = false;
	  LOG(LINF, "TGroupMixer[gid:%d]->cast(%s, ipLoc=%s, portLoc=%d, ipRmt=%s, portRmt=%d)\n", 
						    _gid, on?"on":"off", 
							ipLoc.c_str(), portLoc, ipRmt.c_str(), portRmt);
      _mutex.lock();
	  _bOpenListen = on;

	  if(_bOpenListen) 
	  {
		 bRes = _rtpComm.init(ipLoc.c_str(), portLoc, false);
		 if(bRes) {
			bRes = _rtpComm.start(TCODEC_LPCM16K, 
							  ipRmt.c_str(), portRmt, 
							  "TGroup", _gid, 97, 101, false);
		 }
		 if(!bRes) _bOpenListen = false;
	  }
	  else 
	  {
		 bRes = _rtpComm.final();
	  }
      _mutex.unlock();

	  return bRes;
   }

   // 1. put()  -> 2. mix() -> 3. get()
   bool put(const unsigned int& sid, const TMBuffer& buf)
   {
	  bool bres = false;
      _mutex.lock();
      if (_usedMap.find(sid) == _usedMap.end())
	  {
		 _mutex.unlock();
		 return false;
	  }
      unsigned int bid = _usedMap[sid];
  	  _mutex.unlock();

	  if(buf.skip() == 0)
	  {
		 bres = _pinBuffer[bid].put(buf);
#ifdef _MDEBUG
      CTime_ ctime;
      LOG(LINF, "%s M-PUT [id=%04d/%04d] %s", 
				  (LPCSTR)ctime.Format("%T"), bid, sid, buf.str().c_str());
#endif 
	  }
      return bres; 
   }

   bool get(const unsigned int& sid, TMBuffer& buf)
   {
      _mutex.lock();
      if (_usedMap.find(sid) == _usedMap.end())
	  {
		 _mutex.unlock();
		 return false;
	  }

      unsigned int bid = _usedMap[sid];
	  _mutex.unlock();

      return _poutBuffer[bid].get(buf);
   }
   
   bool mix()
   {
      // SID maps
	  _tmpBuf.clear(); // & reset() ?
	  // read bgm & mixing
	  if(_bgcode != 0) 
	  {
		 TSharedMFiles::STATUS res = _bgfile.get(TCODEC_LPCM16K, _bgcode, _tmpBuf);
		 if(res == TSharedMFiles::END) 
		 {
			_bgfile.init();
			res = _bgfile.get(TCODEC_LPCM16K, _bgcode, _tmpBuf);
		 }
		
		 if(res != TSharedMFiles::DATA)  _tmpBuf.clear();
	  } 

	  if(_mcode != 0) 
	  {
		 _tmpBuf2.clear();
		 TSharedMFiles::STATUS res = _mfile.get(TCODEC_LPCM16K, _mcode, _tmpBuf2);
		 if(res == TSharedMFiles::END) 
		 {
			_mcode = 0;
			_mfile.init();
		 }
		
		 if(res != TSharedMFiles::DATA)  _tmpBuf2.clear();

		 _tmpBuf.mix(_tmpBuf2);
	  } 

	  unsigned int tot_count = 0;
	  unsigned int div_count = 0;
	  unsigned int count = 0; 
	  unsigned int i1 = 0;
	  unsigned int i2 = 0;
	  
      _mutex.lock();
	  if(_usedMap.size() == 0)
	  {
		 _mutex.unlock();
		 return false;
	  }
      for (auto &oIndex : _usedMap)
      {
		 i2 = tot_count % NUM_MIX_DIV2;
		 i1 = tot_count / NUM_MIX_DIV2;

		 _pMixBuffer[i1][i2].set(_tmpBuf);
		 //_pBuffer[i1][i2].clear();
		 _index[i1][i2] = oIndex.second;
         
		 _pinBuffer[oIndex.second].get(_pBuffer[i1][i2]);

		 //_pBuffer[i1][i2].codec() = _codec;
#ifdef _MDEBUG
		 CTime_ ctime;
         printf("%s M-GET [id=%04d] %s", 
								 (LPCSTR)ctime.Format("%T"), _index[i1][i2], 
								 _pBuffer[i1][i2].str().c_str());
#endif
		 tot_count++;
      }
      _mutex.unlock();

	  if(_tmpBuf.skip()) 
	  {
		 _mixAllBuffer.set(LPCM_SID, _frameSize, 0, false);
	  }
	  else 
	  {
		 _mixAllBuffer.set(_tmpBuf);
	  }
	  //_mutex.unlock();

	  div_count = i1+1;

	  //printf(" # count : %d/%d\n", div_count, tot_count);
	  count = 0;
	  for(i1=0; i1 < div_count; i1++) 
	  {
		 _pMidBuffer[i1].clear();
		 for(i2=0; i2<NUM_MIX_DIV2 && count < tot_count; i2++, count++)
		 {
			_pMidBuffer[i1].mix(_pBuffer[i1][i2]);

		    unsigned int count2 = 0;	
			for(unsigned int iself=0; iself<NUM_MIX_DIV2 && count2 < tot_count; iself++, count2++)
			{
			   if(iself == i2) continue;
			   //printf(" # mix : [%d,%d]\n", i1, i2);
			   _pMixBuffer[i1][i2].mix(_pBuffer[i1][iself]);
			}
		 }
	  }

	  count = 0;
	  for(i1=0; i1 < div_count; i1++)
	  {
		 for(i2=0; i2<NUM_MIX_DIV2 && count < tot_count; i2++, count++)
		 {
			for(unsigned int iself1=0; iself1<div_count; iself1++)
			{
			   if(iself1 == i1) continue;
			   _pMixBuffer[i1][i2].mix(_pMidBuffer[iself1]);
			}
		 }
		 _mixAllBuffer.mix(_pMidBuffer[i1]);
	  }
	  
	  count = 0;
	  for(i1=0; i1 < div_count; i1++)
	  {
		 for(i2=0; i2<NUM_MIX_DIV2 && count < tot_count; i2++, count++)
		 {
			 _poutBuffer[_index[i1][i2]].put(_pMixBuffer[i1][i2]);
#ifdef _MDEBUG
			 CTime_ ctime;
			 printf("%s M-PUT [id=%04d] %s", 
				   (LPCSTR)ctime.Format("%T"), _index[i1][i2], 
				   _pBuffer[i1][i2].str().c_str());
#endif
		 }
	  }

	  if(_bRecord)
	  {
		 _recfile.write(_mixAllBuffer);
	  }

	  if(_bOpenListen)
	  {  
		 _rtpComm.send(_mixAllBuffer);
	  }

      return true;
   }
};

#endif // __TMixer__
