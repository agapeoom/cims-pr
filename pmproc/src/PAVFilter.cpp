#include <ios>
#include <iostream>
#include <fstream>

#include "base.h"

#include "PMPBase.h"
#include "PAVBase.h"
#include "PAVBuffer.h"


#include "PAVFilter.h"
#include "PAVCodec.h"

#define NUM_MIX_DIV1 10 
#define NUM_MIX_DIV2 10 
#define  MAX_SES_PER_GROUP (NUM_MIX_DIV1 * NUM_MIX_DIV2)
#define MAX_POSITIONS 9


/******************************************************************************************
+------------------+   +------------------+   +--------+---------+    +--------+---------+
| 1                |   | 1                |   |        |    2    |    |   1    |    2    |
|                  |   |                  |   +--------|         |    |        |         |
|                  |   |         +------+ |   |   1    +---------+    +--------+---------+
|                  |   |         |  2   | |   +--------+    3    |    |   3    |    4    |
|                  |   |         +------+ |   |        |         |    |        |         |
+------------------+   +------------------+   +--------+---------+    +--------+---------+

+--------+---------+   +------------+-----+   +-----+-----+------+    +-----+-----+------+
|   1    |    2    |   |            |  2  |   |  1  |  3  |  6   |    |  1  |  3  |   6  |
|        |         |   |     1      +-----+   +     +-----+      +    +     +-----+------+
+-----+------+-----+   |            |  3  |   |-----|  4  |------|    |-----|  4  |   7  |
|   3 |  4   |  5  |   +-----+------+-----+   +     +-----+      +    +     +-----+------+
|     |      |     |   |   4 |  5   |  6  |   |  2  |  5  |  7   |    |  2  |  5  |   8  |
+-----+------+-----+   +-----+------+-----+   +-----+-----+------+    +-----+-----+------+

... (MAX:16)
******************************************************************************************/
typedef struct {
   int x; // percent
   int y; // percent
   int width; // percent
   int height; // percent
} MixPos;
   

static MixPos MixPosInfo1[] = {
   {  0,  0,100,100}
};

static MixPos MixPosInfo2[] = {
   {  0,  0,100,100},
   {  55, 55, 40, 40}
};

static MixPos MixPosInfo3[] = {
   {  0, 25, 50, 50}, { 50,  0, 50, 50},
					  { 50, 50, 50, 50}
};

static MixPos MixPosInfo4[] = {
   {  0,  0, 50, 50}, { 50,  0, 50, 50},
   {  0, 50, 50, 50}, { 50, 50, 50, 50}
};

static MixPos MixPosInfo5[] = {
			{  0,  0, 50, 50}, { 50,  0, 50, 50},
   {  0, 50, 33, 50}, { 33, 50, 34, 50}, { 67, 50, 33, 50}
};

static MixPos MixPosInfo6[] = {
   {  0,  0, 66, 66},					  { 66,  0, 34, 33},
										  { 66, 33, 34, 33},
   {  0, 66, 33, 34}, { 33, 66, 33, 34},  { 66, 66, 34, 34}
};

static MixPos MixPosInfo7[] = {
   {  0,  0, 33, 50},
   {  0, 50, 33, 50},  
									{ 67,  0, 33, 50},
									{ 67, 50, 33, 50},
				  { 33,  0, 34, 33},
				  { 33, 33, 34, 34},
				  { 33, 67, 34, 33}
};

static MixPos MixPosInfo8[] = {
   {  0,  0, 34, 50},
   {  0, 50, 34, 50},              
						{ 34,  0, 33, 34}, { 67,  0, 33, 34},
						{ 34, 34, 33, 33}, { 67, 34, 33, 33},
						{ 34, 67, 33, 33}, { 67, 67, 33, 33},
};

static MixPos MixPosInfo9[] = {
   {  0,  0, 34, 34}, { 34,  0, 33, 34}, { 67,  0, 33, 34},
   {  0, 34, 34, 33}, { 34, 34, 33, 33}, { 67, 34, 33, 33},
   {  0, 67, 34, 33}, { 34, 67, 33, 33}, { 67, 67, 33, 33}
};


// ffmpeg -i input.mp4 -i image.png -filter_complex "[0:v][1:v] overlay=25:25:enable='between(t,0,20)'" -pix_fmt yuv420p -c:a copy output.mp4
// ffmpeg -i input -i logo1 -i logo2 -filter_complex 'overlay=x=10:y=H-h-10,overlay=x=W-w-10:y=H-h-10' output 

class PByteStream 
{
public:
   enum {MAX_DATA_SIZE = 2048 };
   PByteStream() {};
   ~PByteStream() {};

private:
   unsigned char _data[MAX_DATA_SIZE];
   unsigned int  _offset; // offset of start
   unsigned int  _len; // data len

   unsigned int  _usize;

public:
   bool init(int usize)
   {
	  _offset = 0;
	  _len = 0;
	  _usize = usize;
	  return false;
   }

   bool put(unsigned char * ptr, unsigned int len)
   {
	  if(_len+len > MAX_DATA_SIZE) return false;

	  int offset = (_offset + _len) % MAX_DATA_SIZE;

	  if(offset + len > MAX_DATA_SIZE) 
	  {
		 int len1 = MAX_DATA_SIZE - offset; 
		 int len2 = len - len1; 
		 memcpy(&_data[offset], ptr, len1);
		 
		 memcpy(&_data[0], &ptr[len1], len2);
		 _len += len;
	  } 
	  else 
	  {
		 memcpy(&_data[offset], ptr, len);
		 _len += len;
	  }
	  return true;
   }

   bool get(unsigned char * ptr, unsigned int & len)
   {
	  if(_len<_usize) return false;

	  if(_offset + _usize > MAX_DATA_SIZE) 
	  {
		 int len1 = MAX_DATA_SIZE - _offset; 
		 int len2 = _usize - len1; 
		
		 memcpy(ptr, &_data[_offset], len1);
		 memcpy(ptr+len1, &_data[0], len2);

		 _offset = len2;
		 _len -= _usize;
	  } 
	  else 
	  {
		 memcpy(ptr, &_data[_offset], _usize);
		 _offset += _usize;
		 _len -= _usize;
	  }
	  len = _usize;
   }

};

class PVMixer 
{
private:
   struct Member
   {
	  int			 index; // index of position
	  int			 pos; // index of position
	  void	   *	 uid;
	  int			 priority;
	  unsigned int	 pts;
	  bool			 boutput;
   };
private:
   unsigned int _gid;  //group id
   unsigned int _sessions; // sessions per group
   //input buffer

   PAVBuffer    _mbuf;
   PAVBuffer    _ibuf[MAX_SES_PER_GROUP];
   PAVBuffer    _obuf[MAX_SES_PER_GROUP];
   PAVBuffer    _tbuf;

   unsigned	int _indexOfPos[MAX_SES_PER_GROUP];

   std::map <void *, Member>		_members;
   std::deque <unsigned int> _idleQue;

   PAVEncoder  _encoder;

   PMutex	_mutex;


   PAVParam _param;
   double _timeInterval;

   unsigned int _lastPts;
   struct timeval _tvTimeStart;

   struct SwsContext *_resize;

   MixPos *  _pos;
  
public:
   PVMixer() {};
   ~PVMixer() {};

   void _relocate()
   {
	  std::vector<std::pair<int, void*>> vec; // for viewing position
   
	  for(auto data : _members)
	  {
		 int priority = data.second.priority;
		 void * uid = data.first;
		 vec.push_back(std::make_pair(priority, uid));
	  }

	  std::sort(vec.begin(), vec.end());
	  //std::sort(vec.end(), vec.begin());
	  
	  int count = vec.size();
	  for(int pos=0; pos<count; pos++)
	  {
		 void * uid = vec[pos].second;
		 _members[uid].pos = pos; // index of position  
		 _indexOfPos[pos]=_members[uid].index;
	  }

	  int num = _members.size();
	  num = num < MAX_POSITIONS ? num : MAX_POSITIONS;
	  switch(num)
	  {
		 case 1: _pos = MixPosInfo1; break;
		 case 2: _pos = MixPosInfo2; break;
		 case 3: _pos = MixPosInfo3; break;
		 case 4: _pos = MixPosInfo4; break;
		 case 5: _pos = MixPosInfo6; break;
		 case 6: _pos = MixPosInfo6; break;
		 case 7: _pos = MixPosInfo7; break;
		 case 8: _pos = MixPosInfo8; break;
		 case 9: _pos = MixPosInfo9; break;
		 default: _pos = MixPosInfo9; break;
	  }

	  AVFrame * frame = _mbuf.frame();
	  _mbuf.set_frame_black(_param.width, _param.height);
   }

   bool init(unsigned int gid, unsigned int sessions, const PAVParam & param)
   {
	  PAutoLock lock(_mutex);
	  _sessions = sessions;
	  LOG(LINF,"PVMixer[gid:%d]->init()\n", gid);
	  _gid = gid;

	  _idleQue.clear();
	  _members.clear();

	  _param = param;
	  if(!_encoder.init(_param)) 
	  {
		 printf("# %s(%d):%s : failed to init video encoder in PVMixer!\n", __FILE__, __LINE__, __FUNCTION__); 
		 return false;
	  }

	  for(unsigned int index=0; index < _sessions; index++)
	  {
		 _idleQue.push_back(index);
	  }

	  _param = param;

	  _timeInterval = (double)1000/(double)_param.fps;
	  _lastPts = 0;

	  _mbuf.init(); 
	  _tbuf.init();

	  gettimeofday(&_tvTimeStart, NULL);

	  return true;
   }

   void  final()
   {
	  PAutoLock lock(_mutex);
	  LOG(LINF, "PVMixer[gid:%d]->final()\n", _gid);
	  _gid = 0;
   }

   bool  alloc(void * uid, int priority = 0)
   {
	  PAutoLock lock(_mutex);
	  if (_members.find(uid) != _members.end())
	  {
		 LOG(LERR, "PVMixer[gid:%d]->alloc(uid:%04d) :Error, can't alloc(already used session)\n", _gid, uid);
		 return false;
	  }

	  if(_idleQue.empty())
	  {
		 LOG(LERR, "PVMixer[gid:%d]->alloc(uid:%04d) :Error, can't alloc(no idle session)\n", _gid, uid);
		 return false;
	  }

	  unsigned int index = _idleQue.front();
	  _idleQue.pop_front();
	  _members[uid] = { index, -1, uid, priority, 0, false }; // index, pos, uid, priority, pts, boutput
	  
	  _relocate();

	  Member & member = _members[uid];
	  _ibuf[index].init();
	  _ibuf[index].set_frame_black(_param.width, _param.height);

	  _obuf[index].init(); 


	  LOG(LINF, "# PVMixer[gid:%d]->alloc(uid:%04d)-index:%d\n", _gid, uid, index);
	  return true;
   }

   bool  dealloc(void * uid) // ▒▒▒▒▒▒ buffer Index id return
   {
	  PAutoLock lock(_mutex);
	  if (_members.find(uid) == _members.end())
	  {
		 LOG(LERR, "PVMixer[gid:%d]->dealloc(uid:%04d) :Error, can't dealloc(not used session)\n", _gid, uid);
		 return false;
	  }
	  Member &member = _members[uid];
	  _idleQue.push_back(member.index);
	  _members.erase(uid);

	  _relocate();

	  LOG(LINF, "PVMixer[gid:%d]->dealloc(uid:0x%8x)-index:%d\n", _gid, uid, member.index);
	  // clear frame (black screen)

	  return true;
   }

   bool put(void * uid, const PAVBuffer& in)
   {
	  AVFrame * src;
	  AVFrame * dst;
	  PAutoLock lock(_mutex);

	  //1. find memeber
	  if (_members.find(uid) == _members.end())
	  {
		 return false;
	  }
	  //unsigned int index = _members[uid];
	  Member &member = _members[uid];

	  //2. set buffer at view position
	  if(member.pos >= MAX_POSITIONS || member.pos < 0) return true;
	  _ibuf[member.index].set_frame(in.frame());

#if 0
		 CTime_ ctime;
		 LOG(LINF, "%s M-PUTV [id=%04d/%04d] %s\n",
			   (LPCSTR)ctime.Format("%T"), member.index, uid, in.str().c_str());
#endif
#if 0
	  {
		 AVFrame * tframe = _ibuf[member.index].frame();
		 std::string fname = formatStr("mix-in-member-%04d.yuv", uid);
		 FILE *fp = fopen(fname.c_str(), "ab+");
		 if (fp != NULL)
		 {
			fwrite(tframe->data[0], 1, tframe->linesize[0]*_param.height, fp);
			fwrite(tframe->data[1], 1, tframe->linesize[1]*_param.height/2, fp);
			fwrite(tframe->data[2], 1, tframe->linesize[2]*_param.height/2, fp);
			fclose(fp);
		 }
	  }
#endif 
	  //3. mixing ... 
	  _mix();

	  return true;
   }

   void _mix()
   {
	  struct timeval tvCurTime;
	  gettimeofday(&tvCurTime, NULL);
	  unsigned int nDiffTimeMs = PDIFFTIME(tvCurTime, _tvTimeStart);

	  unsigned int curPts = (unsigned int)(nDiffTimeMs / _timeInterval); 
	 
	  if(curPts<=_lastPts) return;

	  _lastPts = curPts;

	  AVFrame * src;
	  AVFrame * dst;

#if 0
		 CTime_ ctime;
		 LOG(LINF, "%s MIX \n", (LPCSTR)ctime.Format("%T"));
#endif
	  int count = _members.size();
	  for(int pos=0; pos<count; pos++)
	  {
		 if(pos < MAX_POSITIONS) 
		 {
			//3. resize frame
			int width = _param.width * _pos[pos].width / 100;
			int height = _param.height * _pos[pos].height / 100;
			_tbuf.set_frame_black(width, height);
			
			src = _ibuf[_indexOfPos[pos]].frame();
			dst = _tbuf.frame();

			SwsContext * sws = sws_getContext(src->width, src->height, AV_PIX_FMT_YUV420P, width, height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
			if(!sws) return ;

			int ret = sws_scale(sws, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
			sws_freeContext(sws);
			if(ret < 0) continue;

			//4. mix video frame(overlay)
			src = _tbuf.frame();
			dst = _mbuf.frame();

			int planes = 3; // AV_PIX_FMT_YUV420P
			for(int p=0; p<planes; p++)
			{
			   int X1 =  src->linesize[p];
			   int Y1 =  src->height * X1 / src->width;
			   int X2 =  dst->linesize[p];
			   int Y2 =  dst->height * X2 / dst->width;

			   int posx = X2 * _pos[pos].x / 100;
			   int posy = Y2 * _pos[pos].y / 100;

			   unsigned char *  pos1 = src->data[p];  
			   unsigned char *  pos2 = dst->data[p] + (posy * X2) + posx;

			   for(int y=0; y<Y1;y++)
			   {
				  memcpy(pos2, pos1, X1);
				  pos1 += X1;
				  pos2 += X2;
			   }
			}
		 }
	  }
	  
#if 0
	  {
		 AVFrame * tframe = _mbuf.frame();
		 std::string fname = formatStr("mix-out-member-all.yuv");
		 FILE *fp = fopen(fname.c_str(), "ab+");
		 if (fp != NULL)
		 {
			fwrite(tframe->data[0], 1, tframe->linesize[0]*_param.height, fp);
			fwrite(tframe->data[1], 1, tframe->linesize[1]*_param.height/2, fp);
			fwrite(tframe->data[2], 1, tframe->linesize[2]*_param.height/2, fp);
			fclose(fp);
		 }
	  }
#endif 

	  src = _mbuf.frame(); //mixed buffer
	  src->pts = _lastPts; 
	  _encoder.put(_mbuf); 

	  PAVBuffer buf;
	  buf.init();

	  while(_encoder.get(buf))
	  {
		 AVPacket * pkt = buf._packet;

		 for(int pos=0; pos<count; pos++)
		 {
			_obuf[pos].put(pkt);
		 }
	  }
   }


   bool get(void * uid, PAVBuffer& out)
   {
	  PAutoLock lock(_mutex);

	  if (_members.find(uid) == _members.end())
	  {
		 return false;
	  }

	  Member &member = _members[uid];
	
	  if(member.pos < 0) return false;
	  
	  AVPacket pkt; 
	  av_init_packet(&pkt);
	  bool bres = _obuf[member.pos].get(&pkt);
	  if(!bres) return false;
	  out.set(&pkt, PAVTYPE_VIDEO);
	  return true;
   }
};


class PAMixer 
{
public:
   enum { MAX_AUDIO_FRAME_SIZE = 1024 };
private:

   unsigned int _gid;  //group id
   unsigned int _sessions; // sessions per group

   double     _timeInterval;
   unsigned int _lastPts;
   struct timeval _tvTimeStart;


   //input buffer
   PByteStream      _stream[MAX_SES_PER_GROUP];
   unsigned int     _seq[MAX_SES_PER_GROUP];
   unsigned int     _timestamp[MAX_SES_PER_GROUP];
   unsigned int     _pts[MAX_SES_PER_GROUP];

   PJittBuffer      _ibufA[MAX_SES_PER_GROUP];


   //output buffer 
   PRingBuffer  _obufA[MAX_SES_PER_GROUP];

   //for mixing
   PBuffer		  _mbufA[NUM_MIX_DIV1][NUM_MIX_DIV2]; //buffer for mixing
   PBuffer		  _tbufA[NUM_MIX_DIV1][NUM_MIX_DIV2]; //buffer for mixing
   PBuffer		  _midbuf[NUM_MIX_DIV1];
   PBuffer		  _allbuf; 
 
   unsigned int  _index[NUM_MIX_DIV1][NUM_MIX_DIV2];

   std::map <void *, unsigned int> _usedMap;
   std::deque <unsigned int> _idleQue;

   PMutex	_mutex;

public:
   PAMixer() {};
   ~PAMixer() {};

   bool init(unsigned int gid, unsigned int sessions)
   {
	  _sessions = sessions;
	  LOG(LINF,"PAMixer[gid:%d]->init()\n", gid);
	  _mutex.lock();
	  _gid = gid;

	  _idleQue.clear();
	  _usedMap.clear();

	  for(unsigned int index=0; index < _sessions; index++)
	  {
		 _stream[index].init(640);
		 _seq[index] = 0;
		 _timestamp[index] = 0;
		 _pts[index] = 0;

		 _ibufA[index].init(index, 20);
		 _idleQue.push_back(index);
	  }
	  _timeInterval = 20; //msec
	  _lastPts = 0;

	  gettimeofday(&_tvTimeStart, NULL);
	  _mutex.unlock();
	  return true;
   }

   void final()
   {
	  LOG(LINF, "PAMixer[gid:%d]->final()\n", _gid);
	  _mutex.lock();
	  _gid = 0;
	  _mutex.unlock();
   }


   bool  alloc(void * uid, int priority = 0)
   {
	  _mutex.lock();
	  if (_usedMap.find(uid) != _usedMap.end())
	  {
		 LOG(LERR, "PAMixer[gid:%d]->alloc(uid:%04d) :Error, can't alloc(already used session)\n", _gid, uid);
		 _mutex.unlock();
		 return false;
	  }

	  if(_idleQue.empty())
	  {
		 LOG(LERR, "PAMixer[gid:%d]->alloc(uid:%04d) :Error, can't alloc(no idle session)\n", _gid, uid);
		 _mutex.unlock();
		 return false;
	  }

	  unsigned int index = _idleQue.front();
	  _idleQue.pop_front();
	  _usedMap[uid] = index;

	  
	  LOG(LINF, "PAMixer[gid:%d]->alloc(uid:%04d)-index:%d\n", _gid, uid, index);
	  _mutex.unlock();
	  return true;
   }

   bool dealloc(void * uid) 
   {
	  _mutex.lock();
	  if(_usedMap.find(uid) == _usedMap.end())
	  {
		 LOG(LERR, "PAMixer[gid:%d]->dealloc(uid:%04d) :Error, can't dealloc(not used session)\n", _gid, uid);
		 _mutex.unlock();
		 return false;
	  }
	  unsigned int index = _usedMap[uid];
	  _idleQue.push_back(index);
	  _usedMap.erase(uid);
	  LOG(LINF, "PAMixer[gid:%d]->dealloc(uid:%04d)-index:%d\n", _gid, uid, index);
	  _mutex.unlock();
	  return true;
   }

   bool put(void * uid, const PAVBuffer& in)
   {
	  bool bres = false;
	  _mutex.lock();
	  if (_usedMap.find(uid) == _usedMap.end())
	  {
	     _mutex.unlock();
		 return false;
	  }
	  unsigned int index = _usedMap[uid];
	  _mutex.unlock();
	 
	 
	  _stream[index].put(in._frame->data[0], in._frame->nb_samples * 2);
#if 0
			   std::string fname = formatStr("mdump0-%02d.pcm", index);
			   std::ofstream file;
			   file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
			   //file.seekp(0, ios::end);
			   file.write(in._frame->data[0], in._frame->nb_samples * 2);
			   printf("# %s : file write(index=%d, len=%d)\n", fname.c_str(), index, in._frame->nb_samples * 2);
			   file.close();
#endif 

	  PBuffer buf;

	  //printf("# MIX[%02d]-put(seq=%d, timestamp=%d, len=%d)\n", 
	  //			   index, in._seq, in._timestamp, in._frame->nb_samples * 2);
	  while(_stream[index].get(buf.ptr(), buf.len()))
	  {
		 //buf.sequence() = in._seq; 
		 //buf.timestamp() = in._timestamp; 
		 buf.skip() = false; 
		 buf.sequence() = _seq[index]; 
		 buf.timestamp() = _timestamp[index]; 
		 _seq[index]++;
		 _timestamp[index] += 160;
		 bres = _ibufA[index].put(buf);

#ifdef _MDEBUG
		 CTime_ ctime;
		 LOG(LINF, "%s M-PUTA [id=%04d/%04d] %s",
			   (LPCSTR)ctime.Format("%T"), index, uid, in.str().c_str());
#endif
#if 0
			   std::string fname = formatStr("mdump1-%02d.pcm", index);
			   std::ofstream file;
			   file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
			   //file.seekp(0, ios::end);
			   file.write(buf.ptr(), buf.len());
			   printf("# %s : file write(index=%d, len=%d)\n", fname.c_str(), index, buf.len());
			   file.close();
#endif 
	  }
	  return bres;
   }

   bool get(void * uid, PAVBuffer& out)
   {
	  _mutex.lock();
	  auto it = _usedMap.find(uid);
	  if (it == _usedMap.end())
	  {
		 _mutex.unlock();
		 return false;
	  }
	  
	  if(it == _usedMap.begin())
	  {
		 _mix();
	  }
   
	  unsigned int index = _usedMap[uid];

	  _mutex.unlock();

	  PBuffer buf;
	  bool bres = _obufA[index].get(buf);
	  if(!bres) 
	  {
		 return false;
	  }

	  out.set_frame_audio(16000, 1, buf.ptr(), buf.len()/2, _pts[index]); 
	  if(!bres) 
	  {
		 return false;
	  }
	  _pts[index] += buf.len()/2; // 2byte / 1sample

	  return true;
   }
   
   bool _mix()
   {
	  struct timeval tvCurTime;
	  gettimeofday(&tvCurTime, NULL);
	  unsigned int nDiffTimeMs = PDIFFTIME(tvCurTime, _tvTimeStart);

	  unsigned int curPts = (unsigned int)(nDiffTimeMs / _timeInterval); 
	 
	  if(curPts<=_lastPts) return;

	  _lastPts = curPts;


	  unsigned int tot_count = 0;
	  unsigned int div_count = 0;
	  unsigned int count = 0;
	  unsigned int i1 = 0;
	  unsigned int i2 = 0;
	  //unsigned int iv = 0;

	  if(_usedMap.size() == 0)
	  {
		 return false;
	  }

	  for (auto &oIndex : _usedMap)
	  {
		 i2 = tot_count % NUM_MIX_DIV2;
		 i1 = tot_count / NUM_MIX_DIV2;
		 
		 //_mbufA[i1][i2].clear(); //mix buffer
		 _mbufA[i1][i2].reset(640); //mix buffer
		 _mbufA[i1][i2].len() = 640;
		 _index[i1][i2] = oIndex.second; //mix buffer index

		 _ibufA[oIndex.second].get(_tbufA[i1][i2]); //temp buffer

#ifdef _MDEBUG
		 CTime_ ctime;
		 printf("%s M-GET [id=%04d] %s",
			   (LPCSTR)ctime.Format("%T"), _index[i1][i2],
			   _tbufA[i1][i2].str().c_str());
#endif
#if 0
			   std::string fname = formatStr("mdump2-%02d.pcm", _index[i1][i2]);
			   std::ofstream file;
			   file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
			   //file.seekp(0, ios::end);
			   file.write(_tbufA[i1][i2].ptr(), _tbufA[i1][i2].len());
			   printf("# %s : file write(index=%d, len=%d)\n", fname.c_str(), _index[i1][i2], _tbufA[i1][i2].len());
			   file.close();
#endif 
		 tot_count++;
	  }

	  _allbuf.reset(640);
	  _allbuf.len() = 640;

	  // 2. mix audio 
	  div_count = i1+1;
	  count = 0;
	  //printf(" # count : %d/%d\n", div_count, tot_count);
	  for(i1=0; i1 < div_count; i1++)
	  {
		 _midbuf[i1].clear();
		 for(i2=0; i2<NUM_MIX_DIV2 && count < tot_count; i2++, count++)
		 {
			_midbuf[i1].mix(_tbufA[i1][i2]);

			unsigned int count2 = 0;
			for(unsigned int iself=0; iself<NUM_MIX_DIV2 && count2 < tot_count; iself++, count2++)
			{
			   if(iself == i2) continue;
			   //printf(" # mix : [%d,%d]\n", i1, i2);
			   _mbufA[i1][i2].mix(_tbufA[i1][iself]);
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
			   _mbufA[i1][i2].mix(_midbuf[iself1]);
			}
		 }
		 _allbuf.mix(_midbuf[i1]);
	  }

	  count = 0;
	  for(i1=0; i1 < div_count; i1++)
	  {
		 for(i2=0; i2<NUM_MIX_DIV2 && count < tot_count; i2++, count++)
		 {
			if(_mbufA[i1][i2].len() == 640) 
			{
#if 0
			   std::string fname = formatStr("mdump3-%02d.pcm", _index[i1][i2]);
			   std::ofstream file;
			   file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
			   //file.seekp(0, ios::end);
			   file.write(_mbufA[i1][i2].ptr(), _mbufA[i1][i2].len());
			   printf("%s : file write(index=%d, len=%d)\n", fname.c_str(), _index[i1][i2], _mbufA[i1][i2].len());
			   file.close();
#endif 
			   _obufA[_index[i1][i2]].put(_mbufA[i1][i2]);
			   //printf("### audio 1\n");
			} 
			else
			{
#if 0
			   std::ofstream file;
			   file.open("dump1.pcm", std::ios::binary|std::ios::ate|std::ios::app);
			   //file.seekp(0, ios::end);
			   file.write(_allbuf.ptr(), _allbuf.len());
			   printf("dump1 : file write(index=%d, len=%d)\n", _index[i1][i2], _allbuf.len());
			   file.close();
#endif 
			   //_obufA[_index[i1][i2]].put(_allbuf);
			   //printf("### audio 2\n");
			}
#ifdef _MDEBUG
			CTime_ ctime;
			printf("%s M-PUT [id=%04d] %s",
				  (LPCSTR)ctime.Format("%T"), _index[i1][i2],
				  _tbufA[i1][i2].str().c_str());
#endif
		 }
	  }
#if 0
			CTime_ ctime;
			printf("%s M-MIX [id=%04d]\n",
				  (LPCSTR)ctime.Format("%T"), _index[0][0]);
		 
		 {
			   std::string fname = formatStr("mdump3-all.pcm");
			   std::ofstream file;
			   file.open(fname, std::ios::binary|std::ios::ate|std::ios::app);
			   //file.seekp(0, ios::end);
			   file.write(_allbuf.ptr(), _allbuf.len());
			   printf("%s : file write(index=all, len=%d)\n", fname.c_str(), _allbuf.len());
			   file.close();
		 }
#endif 

	  return true;
   }
};

class PAMixManager 
{
private:
   unsigned int _groups;
   unsigned int _sessions;
   PAMixer  * _pamixer;

   std::deque <unsigned int> _idleQue;

   std::map<void * , unsigned int> _sesMap; // uid : gid;
   std::map<unsigned int, unsigned int> _groupMap; // gid : mid;
   std::map<unsigned int, std::map<void *, void *>> _sesOfGroupMap; // gid : [sindex : sindex];

   std::string _name;
public:
   PAMixManager() 
   {  _name = "PAMixManager"; }
   ~PAMixManager() { final();}

   std::string & name() { return _name; }
   // sessions : sessions per 1group
   bool init(unsigned int groups, unsigned int sesPerGroup) 
   {
      _groups = groups;
      _sessions = sesPerGroup;

	  _pamixer = new PAMixer[_groups];
	  if(!_pamixer)
	  {
		 LOG(LERR, "Can't create %s (groups=%d, ses/group=%d)\n", 
					 _name.c_str(), groups, sesPerGroup);
		 return false;
	  }
   
	  unsigned int gcount = 0;
	  bool bres = false;

	  for(int i=0; i<_groups; i++) 
	  {
		 _pamixer[i].init(i, sesPerGroup);
		 _idleQue.push_back(i);
	  }

	  return true;
   }

   void  final()
   {
	  if(!_pamixer) return true;
	  delete [] _pamixer;
	  _pamixer = NULL;

      _idleQue.clear();

      _groupMap.clear();
      _sesOfGroupMap.clear();
	  _sesMap.clear();
   }


   bool alloc(void * uid, unsigned int gid, unsigned int priority = 0)
   {
	  if(_sesMap.find(uid) != _sesMap.end()) 
	  {
		 LOG(LERR, "%s->alloc(gid:%d, uid:%04d):Error, can't alloc (already used)!\n", name().c_str(), gid, uid); 
		 return false;
	  }

	  auto it = _groupMap.find(gid);
	  if(it != _groupMap.end()) 
	  {
		 if(_sesOfGroupMap[gid].find(uid) != _sesOfGroupMap[gid].end()) 
		 {
			LOG(LERR, "%s->alloc(gid:%d, uid:%04d):Error, in-correct ses of group map(critical error)!\n", 
				  name().c_str(), gid, uid); 

			_sesOfGroupMap[gid].erase(uid);
			return false;
		 }
	  }
	  else 
	  {
		 //new group ... 
		 if(_idleQue.empty()) 
		 {
			LOG(LERR, "%s->alloc(gid:%d, uid:%04d):Error, can't alloc PAMixer(no idle session)!\n", name().c_str(), gid, uid); 
			return false;
		 }
		 
		 unsigned int mid = _idleQue.front();
		 _idleQue.pop_front();

		 _groupMap[gid] = mid; 
		 _sesOfGroupMap[gid][uid] = uid;

		 LOG(LERR, "# RSC_INF : ADD-GROUP[used:%d/%d, idle:%d, tot:%d]\n", 
							  _groupMap.size(), _sesOfGroupMap.size(), _idleQue.size(), _groups);
	  }

	  _sesMap[uid] = gid;

	  _sesOfGroupMap[gid][uid] = uid;

	  _pamixer[_groupMap[gid]].alloc(uid, priority);

	  LOG(LINF, "%s->alloc(gid:%d, uid:%04d):done!\n", name().c_str(), gid, uid); 
	  return true; 
   }
   
   void dealloc(void * uid)
   {
	  if(_sesMap.find(uid) == _sesMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find uid!\n", name().c_str(), uid); 
		 return;
	  }
   
	  unsigned int gid = _sesMap[uid]; 
	  _sesMap.erase(uid);

	  if(_groupMap.find(gid) == _groupMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find gid(gid:%d, in _groupMap)!\n", 
					 name().c_str(), uid, gid); 
		 //return;
	  }


	  if(_sesOfGroupMap.find(gid) == _sesOfGroupMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find gid(gid:%d, in _sesOfGroupMap)!\n", 
					 name().c_str(), uid, gid); 
		 //return;
	  }
//
	  _sesOfGroupMap[gid].erase(uid);
	  LOG(LERR, "%s->dealloc(uid:%d, gid:%d, members:%d))!\n", name().c_str(), uid, gid, _sesOfGroupMap[gid].size());

	  if(_sesOfGroupMap[gid].size() <= 0) 
	  {
	     unsigned int mid = _groupMap[gid]; 
		 _pamixer[mid].dealloc(uid);

		 _idleQue.push_back(mid);
		 _groupMap.erase(gid);
		 _sesOfGroupMap.erase(gid);
		 LOG(LERR, "# RSC_INF : DEL-GROUP[used:%d/%d, idle:%d, tot:%d]\n", 
							  _groupMap.size(), _sesOfGroupMap.size(), _idleQue.size(), _groups);
		 printf("# RSC_INF : DEL-GROUP[used:%d/%d, idle:%d, tot:%d]\n\n", 
							  _groupMap.size(), _sesOfGroupMap.size(), _idleQue.size(), _groups);
	  }
	  return;
   }

   bool put(void * uid, PAVBuffer & in) 
   {
	  if(_sesMap.find(uid) == _sesMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find uid!\n", name().c_str(), uid); 
		 return;
	  }
   
	  unsigned int gid = _sesMap[uid]; 
	  unsigned int mid = _groupMap[gid];
	  return _pamixer[mid].put(uid, in);

   }
   
   bool get(void * uid, PAVBuffer & out) 
   {
	  if(_sesMap.find(uid) == _sesMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find uid!\n", name().c_str(), uid); 
		 return;
	  }
   
	  unsigned int gid = _sesMap[uid]; 
	  unsigned int mid = _groupMap[gid];
	  return _pamixer[mid].get(uid, out);
   }
};

class PVMixManager 
{
private:
   unsigned int _groups;
   unsigned int _sessions;
   PVMixer  * _pvmixer;
   PAVParam * _pparam;

   std::deque <unsigned int> _idleQue;

   std::map<void * , unsigned int> _sesMap; // uid : gid;
   std::map<unsigned int, unsigned int> _groupMap; // gid : mid;
   std::map<unsigned int, std::map<void *, void *>> _sesOfGroupMap; // gid : [sindex : sindex];

   std::string _name;
public:
   PVMixManager() 
   {
	  _name = "PVMixManager";
   }
   ~PVMixManager() { final();}

   std::string & name() { return _name; }
   // sessions : sessions per 1group
   bool init(unsigned int groups, unsigned int sesPerGroup)
   {
      _groups = groups;
      _sessions = sesPerGroup;

	  _pvmixer = new PVMixer[_groups];
	  _pparam  = new PAVParam[_groups];
	  if(!_pvmixer || !_pparam)
	  {
		 LOG(LERR, "Can't create %s (groups=%d, ses/group=%d)\n", 
					 _name.c_str(),groups, sesPerGroup);
		 return false;
	  }
   
	  unsigned int gcount = 0;
	  bool bres = false;

	  for(int i=0; i<_groups; i++) 
	  {
		 _idleQue.push_back(i);
	  }

	  return true;
   }

   void  final()
   {
	  if(!_pvmixer) return true;
	  delete [] _pvmixer;
	  _pvmixer = NULL;

      _idleQue.clear();

      _groupMap.clear();
      _sesOfGroupMap.clear();
	  _sesMap.clear();
   }


   bool alloc(void * uid, unsigned int gid, unsigned int priority, const PAVParam & param)
   {
	  if(_sesMap.find(uid) != _sesMap.end()) 
	  {
		 LOG(LERR, "%s->alloc(gid:%d, uid:%04d):Error, can't alloc (already used)!\n", name().c_str(), gid, uid); 
		 return false;
	  }

	  auto it = _groupMap.find(gid);
	  if(it != _groupMap.end()) 
	  {
		 if(_sesOfGroupMap[gid].find(uid) != _sesOfGroupMap[gid].end()) 
		 {
			LOG(LERR, "%s->alloc(gid:%d, uid:%04d):Error, in-correct ses of group map(critical error)!\n", 
				  name().c_str(), gid, uid); 

			_sesOfGroupMap[gid].erase(uid);
			return false;
		 }
	  }
	  else 
	  {
		 //new group ... 
		 if(_idleQue.empty()) 
		 {
			LOG(LERR, "%s->alloc(gid:%d, uid:%04d):Error, can't alloc PAMixer(no idle session)!\n", name().c_str(), gid, uid); 
			return false;
		 }
		 

		 unsigned int gindex = _idleQue.front();
		 if(!_pvmixer[gindex].init(gid, _sessions, param))
		 {
			LOG(LERR, "# %s->alloc(gid:%d, uid:%04d) : failed to init Video Mixer(%s)!\n", 
					     name().c_str(), gid, uid, param.str().c_str());
			return false;
		 }
		 _pparam[gindex] = param;

		 _idleQue.pop_front();

		 _groupMap[gid] = gindex; 
		 _sesOfGroupMap[gid][uid] = uid;

		 LOG(LERR, "# RSC_INF : ADD-GROUP[used:%d/%d, idle:%d, tot:%d]\n", 
							  _groupMap.size(), _sesOfGroupMap.size(), _idleQue.size(), _groups);
	  }

	  unsigned int gindex  = _groupMap[gid]; 

	  if(param.codec != _pparam[gindex].codec || param.width != _pparam[gindex].width || param.height != _pparam[gindex].height)
	  {
		 LOG(LERR, "# %s->alloc(gid:%d, uid:%04d) : invalid video parameter(%s/%s)]\n", 
						   name().c_str(), gid, uid, param.str().c_str(), _pparam[gindex].str().c_str());
		 return false;
	  }

	  _sesMap[uid] = gid;

	  _sesOfGroupMap[gid][uid] = uid;
	  _pvmixer[gindex].alloc(uid, priority); 

	  LOG(LINF, "%s->alloc(gid:%d, uid:%04d):done!\n", name().c_str(), gid, uid); 
	  return true; 
   }
   
   void dealloc(void * uid)
   {
	  if(_sesMap.find(uid) == _sesMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find uid!\n", name().c_str(), uid); 
		 return;
	  }
   
	  unsigned int gid = _sesMap[uid]; 
	  _sesMap.erase(uid);

	  if(_groupMap.find(gid) == _groupMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find gid(gid:%d, in _groupMap)!\n", 
					 name().c_str(), uid, gid); 
		 //return;
	  }


	  if(_sesOfGroupMap.find(gid) == _sesOfGroupMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find gid(gid:%d, in _sesOfGroupMap)!\n", 
					 name().c_str(), uid, gid); 
		 //return;
	  }
	  _sesOfGroupMap[gid].erase(uid);

	  unsigned int mid = _groupMap[gid]; 
	  _pvmixer[mid].dealloc(uid);

	  LOG(LERR, "%s->dealloc(uid:%d, gid:%d)!\n", name().c_str(), uid, gid); 
	  if(_sesOfGroupMap[gid].size() <= 0) 
	  {

		 _idleQue.push_back(mid);
		 _groupMap.erase(gid);
		 _sesOfGroupMap.erase(gid);
		 LOG(LERR, "# RSC_INF : DEL-GROUP[used:%d/%d, idle:%d, tot:%d]\n", 
							  _groupMap.size(), _sesOfGroupMap.size(), _idleQue.size(), _groups);
		 printf("# RSC_INF : DEL-GROUP[used:%d/%d, idle:%d, tot:%d]\n\n", 
							  _groupMap.size(), _sesOfGroupMap.size(), _idleQue.size(), _groups);
	  }
	  return;
   }

   bool put(void * uid, PAVBuffer & in) 
   {
	  if(_sesMap.find(uid) == _sesMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find uid!\n", name().c_str(), uid); 
		 return;
	  }
   
	  unsigned int gid = _sesMap[uid]; 
	  unsigned int mid = _groupMap[gid]; 
	  return _pvmixer[mid].put(uid, in);

   }
   
   bool get(void * uid, PAVBuffer & out) 
   {
	  if(_sesMap.find(uid) == _sesMap.end()) 
	  {
		 LOG(LERR, "%s->dealloc(uid:%04d):Error, can't find uid!\n", name().c_str(), uid); 
		 return;
	  }
   
	  unsigned int gid = _sesMap[uid]; 
	  unsigned int mid = _groupMap[gid]; 
	  return _pvmixer[mid].get(uid, out);
   }
};

static PAMixManager  _mixMgrA;
static PVMixManager  _mixMgrV;
static bool bresA = _mixMgrA.init(20, 10);
//static bool bresV = _mixMgrV.init(20, 10, PAVCODEC_H264, PAV_VIDEO_WIDTH, PAV_VIDEO_HEIGHT, PAV_VIDEO_FPS);
static bool bresV = _mixMgrV.init(20, 10);


PAMixFilter::PAMixFilter() {};
PAMixFilter::~PAMixFilter() {};


bool PAMixFilter::init(unsigned int group)
{
   return _mixMgrA.alloc(this, group); // group : group id
}

void PAMixFilter::final()
{
   return _mixMgrA.dealloc(this); // group : group id
}


bool PAMixFilter::put(PAVBuffer & in)
{
   return _mixMgrA.put(this, in); // group : group id
}

bool PAMixFilter::get(PAVBuffer & out)
{
   return _mixMgrA.get(this, out); // group : group id
}


PVMixFilter::PVMixFilter() {};
PVMixFilter::~PVMixFilter() {};

bool PVMixFilter::init(unsigned int group, int priority, const PAVParam & param)
{
   return _mixMgrV.alloc(this, group, priority, param);
}

void PVMixFilter::final()
{
   _mixMgrV.dealloc(this); // group : group id
}


bool PVMixFilter::put(PAVBuffer & in)
{
   return _mixMgrV.put(this, in); // group : group id
}

bool PVMixFilter::get(PAVBuffer & out)
{
   return _mixMgrV.get(this, out); // group : group id
}


