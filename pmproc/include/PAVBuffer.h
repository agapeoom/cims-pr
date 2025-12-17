#ifndef __PBuffer__
#define __PBuffer__


#include <deque>
#include <map>
#include <string>
#include <cstring>

#include <tbb/tbb.h>

#include "pbase.h"
//#include "TMpaUtil.h"
#include "PMPBase.h"
#include "PAVBase.h"
#include "CJBuffer.h"


class PAVBuffer 
{
public: 
   AVFrame  * _frame;
   AVPacket * _packet;

   bool				 _dtmf;
   unsigned int		 _seq;
   unsigned int		 _timestamp;
   unsigned int		 _dtmfCode; 

   std::deque <AVPacket * > _pktQue; 

   PAVTYPE		_type; // media type 
public:
   PAVBuffer() : _frame(NULL), _packet(NULL)
   {
    
   }

   std::string str()
   {
	  return formatStr("PAVBuffer[seq=%u,time=%u,dtmf=%d(%d), packet:[psize=%d,ppts=%d,pdts=%d,pdur=%d,stream=%d],"
								 "[frame:[linesize=%d, nb_samples=%d, pts=%d, pkt_pts=%d, pkt_dts=%d]", 
						_seq, _timestamp, _dtmf, _dtmfCode, _packet->size, 
						_packet->pts, _packet->dts, _packet->duration, _packet->stream_index,
						_frame->linesize[0], _frame->nb_samples, _frame->pts, _frame->pkt_pts, _frame->pkt_dts);
   }
   int pktcount() { return _pktQue.size(); }
  
   bool put(AVPacket * pkt)
   {
	  AVPacket * npkt = av_packet_alloc();
	  int ret = av_packet_ref(npkt, pkt);

	  if(ret < 0) 
	  {
		 av_packet_free(&npkt);
		 return false;
	  }
	  _pktQue.push_back(npkt);
	  return true;
   }

   bool get(AVPacket * pkt) 
   {
	  if(_pktQue.empty() || !pkt) return false;
	  av_packet_unref(pkt);

	  AVPacket * npkt = _pktQue.front();
	  _pktQue.pop_front();

	  int ret = av_packet_ref(pkt, npkt);
	  av_packet_free(&npkt);
	  
	  if(ret<0)
	  {
		 return false;
	  }
	  return true;
   }

   virtual bool init()
   {
	  int ret =0;
	  _dtmf = 0;
	  _seq = 0;
	  _timestamp = 0;
	  _dtmfCode = 0;
	  _type = PAVTYPE_NONE;

	  _packet = av_packet_alloc();
	  if(!_packet) 
	  {
		 final();
		 return false;
	  }
	  _frame = av_frame_alloc();
	  if(!_frame) 
	  {
		 final();
		 av_packet_free(&_packet);
		 _packet = NULL;
		 return false;
	  }
	  return true;
   }

   bool set(AVPacket * pkt, PAVTYPE type)
   {
	  if(!_packet) 
	  {
		 _packet = av_packet_alloc();
		 if(!_packet) return false;
	  } 
	  else 
	  {
		 av_packet_unref(_packet);
	  }

	  int ret = av_packet_ref(_packet, pkt);
	  if(ret!=0) 
	  {
		 return false;
	  }
	  _type = type; 
	  return true;
   }

   bool set_packet(unsigned int seq, unsigned int timestamp, unsigned int duration, unsigned char * data, int size)
   {
	  int ret = av_new_packet(_packet, size);
	  if(ret <0)
	  {
		 printf("# %s(%d):%s : faile to av_new_packet for audio!\n", __FILE__, __LINE__, __FUNCTION__);
		 return false;
	  }
	 
	  _packet->stream_index = 0;
	  _seq = seq;
	  _timestamp = timestamp;;
	  _packet->duration = duration;

	  memcpy(_packet->data, data, size);
	  _packet->pos = -1;

	  return true;
   }

   bool set_frame_audio(int sample_rate, int channels, unsigned char * data, int samples, unsigned int pts)
   {
	  av_frame_unref(_frame);
	  
	  _frame->sample_rate = sample_rate;  // 
	  _frame->nb_samples = samples;  // 
	  _frame->format =  AV_SAMPLE_FMT_S16;
	  _frame->channel_layout = av_get_default_channel_layout(channels);
	  _frame->channels = channels; 
	  _frame->pts = _frame->pkt_pts = _frame->pkt_dts = pts;
	  int ret = av_frame_get_buffer(_frame, 32);
	  if(ret <0)
	  {
		 printf("# %s(%d):%s : faile to av_frame_get_buffer for audio!(ret=%d)\n", __FILE__, __LINE__, __FUNCTION__, ret);
		 return false;
	  }

	  ret = av_frame_make_writable(_frame);
	  if(ret <0) 
	  {
		 printf("# %s(%d):%s : faile to av_frame_make_writable for audio!(ret=%d)\n", __FILE__, __LINE__, __FUNCTION__, ret);
		 return false;
	  }
	  memcpy(_frame->data[0], data, samples*2);
	  _frame->linesize[0] = samples*2;


	  return true;
   }

   bool set_frame_black(int width, int height)
   {

	  av_frame_unref(_frame);

	  _frame->format = AV_PIX_FMT_YUV420P;
	  _frame->width  = width;
	  _frame->height = height;
	  int ret = av_image_alloc(_frame->data, _frame->linesize, _frame->width, _frame->height, _frame->format, 32);
	  if (ret < 0) {
		 fprintf(stderr, "failed to av_image_alloc()!\n");
		 return false;
	  }
	  memset(_frame->data[0], 16, _frame->linesize[0]*height);
	  memset(_frame->data[1], 128, _frame->linesize[1]*(height>>1));
	  memset(_frame->data[2], 129, _frame->linesize[2]*(height>>1));
	  return true;
   }

   bool set_frame_color(int width, int height)
   {

	  av_frame_unref(_frame);

	  _frame->format = AV_PIX_FMT_YUV420P;
	  _frame->width  = width;
	  _frame->height = height;
	  int ret = av_image_alloc(_frame->data, _frame->linesize, _frame->width, _frame->height, _frame->format, 32);
	  if (ret < 0) {
		 fprintf(stderr, "failed to av_image_alloc()!\n");
		 return false;
	  }

	  int y, x;
	  for (y = 0; y < height; y++) {
		 for (x = 0; x < width; x++) {
			_frame->data[0][y * _frame->linesize[0] + x] = x + y + 1 * 3;
		 }
	  }
	  /* Cb and Cr */
	  for (y = 0; y < height/2; y++) {
		 for (x = 0; x < width/2; x++) {
			_frame->data[1][y * _frame->linesize[1] + x] = 128 + y + 1 * 2;
			_frame->data[2][y * _frame->linesize[2] + x] = 64 + x + 1 * 5;
		 }
	  }
	  return true;
   }

   bool set_frame(AVFrame * src)
   {
	  av_frame_unref(_frame);
	  _frame->format = src->format;
	  _frame->width = src->width;
	  _frame->height = src->height;
	  _frame->channels = src->channels;
	  _frame->channel_layout = src->channel_layout;
	  _frame->nb_samples = src->nb_samples;

	  int ret = av_frame_copy_props(_frame, src);
	  if(ret<0)
	  {
		 printf("# failed to av_frame_copy_props()!\n");
		 return false;
	  }

	  ret = av_frame_get_buffer(_frame, 32);
	  if(ret<0)
	  {
		 printf("# failed to av_frame_get_buffer()!\n");
		 return false;
	  }
	  
	  ret = av_frame_copy(_frame, src);
	  if(ret<0)
	  {
		 printf("# failed to av_frame_copy()!\n");
		 av_frame_unref(_frame);
		 return false;
	  }

	  return false;
   }

   bool get_frame_audio(int &sample_rate, int &channels, unsigned char * data, int &samples)
   {
	  if(_frame->format != AV_SAMPLE_FMT_S16)
	  {
		 printf("# %s(%d):%s : not surpported frame format!(_frame->format=%d)\n", __FILE__, __LINE__, __FUNCTION__, _frame->format);
		 return false;
	  }
	  if(_frame->linesize[0] <= 0) 
	  {
		 printf("# %s(%d):%s : empty frame data!(_frame->data=0x%x, _frame->linesize[0]=%d)\n", __FILE__, __LINE__, __FUNCTION__, _frame->data, _frame->linesize[0]);
		 return false;
	  }

	  sample_rate = _frame->nb_samples; 
	  channels = _frame->channels;
      sample_rate = _frame->sample_rate;
      samples = _frame->nb_samples;
	  memcpy(data, _frame->data[0], samples*2);

	  return true;
   }

   bool alloc_frame_video(int width, int height)
   {
	  //av_frame_unref(_frame);
	  _frame->format = AV_PIX_FMT_YUV420P;
	  _frame->width = width;
	  _frame->height = height;
	  int ret = av_frame_get_buffer(_frame, 32);
	  if(ret <0)
	  {
		 printf("# %s(%d):%s : faile to av_frame_get_buffer for video!\n", __FILE__, __LINE__, __FUNCTION__);
		 return false;
	  }
	  return true;
   }
#if 0
   void dealloc_frame() 
   {
	  av_frame_unref(_frame);
   }
#endif
   virtual void final()
   {
	  if(_frame) {
		 av_frame_free(&_frame);
		 _frame = NULL;
	  }
	  if(_packet) {
		 av_packet_free(&_packet);
		 _packet = NULL;
	  }
	  return;
   }

   virtual AVFrame * frame() 
   {
	  return _frame;
   }

   virtual AVPacket * packet() 
   {
	  return _packet;
   }

};



class PBuffer
{
public:
   enum { MAX_FRAME_LENGTH = 1024 };
   unsigned char _pt; //payloadtype
   unsigned char _marker;

   // RTP Header Info Sequence & timestamp 
   unsigned int _seq;
   unsigned int _timestamp;
   unsigned int _ssrc;
   bool         _skip;    // 1 : No-DATA or SID 일경우 무시하기 위해서 

   bool		    _dtmf;        //
   int		    _dtmfCode;    //
private:
   unsigned int _size; // max buffer size (byte)
   unsigned int _len; // data length (bytes of contents)

   //unsigned char      *  _ptr;
   unsigned char _ptr[MAX_FRAME_LENGTH];

public:
   PBuffer()
   {
        _size = MAX_FRAME_LENGTH;
		clear();
        //_ptr = new unsigned char[_size];
    }

   PBuffer(unsigned int size)
   {
       _size = size;
		clear();
       //_ptr = new unsigned char[_size];
   }

    ~PBuffer()
    {
        //delete [] _ptr;
    }

    void set(const unsigned char * ptr, unsigned int len, unsigned int offset=0, bool skip = false)
	{
       memcpy(&_ptr[offset], ptr, len);
	   _len = len + offset;
	   _skip = skip; 
	}

    unsigned char* ptr() { return _ptr; }
    const unsigned char* cptr() { return _ptr; }
    
	unsigned char * ptr(unsigned int offset)
    {
       return &_ptr[offset];
    }

	const unsigned char * cptr(unsigned int offset)
    {
       return &_ptr[offset];
    }

    const unsigned int& size() { return _size; }
    unsigned int& len() { return _len; }
    
	unsigned char& pt() { return _pt; }
	unsigned char& marker() { return _marker; }
    unsigned int& sequence() { return _seq; }
    unsigned int& timestamp() { return _timestamp; }
	unsigned int& ssrc() { return _ssrc; }

    bool empty() { return _len == 0; }

    bool& skip() { return _skip; }

    void clear() {
       _len = 0;
       _seq = 0;
       _timestamp = 0;
       _skip = true;
	   _marker = 0;
	   _ssrc = 0;
	   _dtmf = false;
	   _dtmfCode = 0;
    }

    void reset(unsigned int size=0) 
	{
	  clear();
	  if(size>0)
		 memset(_ptr, 0, size);
      else 
		 memset(_ptr, 0, _size);
	}

    // operator =
    void set(PBuffer& buf)
    {      
       _len = buf.len();

	   if(_len>0) 
		 memcpy(_ptr, buf.ptr(), buf.len());

       // RTP Header Info Sequence & timestamp 
       _seq = buf._seq;
       _timestamp = buf._timestamp;
       _skip = buf._skip;
    }

    // 16bits Linear-PCM Mixing
    // operator +
    bool mix(PBuffer& buf)
    {
	   if(_skip) {
	     // no mixing
		 if(buf.skip()) return true;

		 set(buf);
		 return true;
	   }

	   if (buf.skip()) return true;
	   if (_len != buf.len()) return false;

	   short* src1 = (short*)_ptr;
	   short* src2 = (short*)buf.ptr();
	   unsigned int len = _len / 2;

       int sum;
	   int mul;
	   for (unsigned int i = 0; i < len; i++)
	   { 
		   sum = src1[i] + src2[i];
		   if(sum<0x7FFF && sum>-0x7FFF) 
		   {
			   src1[i] = sum;
			   continue;
		   } else {
		       mul = src1[i] * src2[i];
			   //if(mul>0)
			   src1[i] = (sum>0)?(sum-(mul/0x7FFF)):(sum+(mul/0x7FFF));
		   }
	   }
	   return true;

	} 

	short upsample(int rate, short prevSample)  // return last sample
	{
	   // output ought to be 6 times as large as input (48000/8000).
       short lastSample = 0; 
	   if(_skip) { return lastSample; }

	   unsigned int nsamples = _len / 2;

	   short   * ptrIn =  (short *)_ptr;
	   short   * ptrOut = (short *)_ptr;

	   lastSample =	ptrIn[nsamples-1];

	   for (int i=nsamples-1; i>0;  i--)
	   {
	      for(int x=rate-1;x>=0;x--) 
		  {
		    ptrOut[i*rate+x] = ptrIn[i-1]*(rate-x)/rate + ptrIn[i]*x/rate;
		  }
       }
	   // sum  previous sample
	   for(int x=rate-1; x>=0; x--) 
	   {
		    ptrOut[x] = prevSample*(rate-x)/rate + ptrIn[0]*x/rate;
	   }
	   _len = _len * rate; 
	   return lastSample; 
    }

	void downsample(int rate) 
	{
	   // output ought to be 6 times as large as input (48000/8000).
	   if(_skip) { return ; }
	   unsigned int nsamples = _len / 2;
	   short   * ptrIn =  (short *)_ptr;
	   short   * ptrOut = (short *)_ptr;

	   for (int i=0, x=0; i < nsamples;  i+=rate, x++)
	   {
		 ptrOut[x] = ptrIn[i];
       }

	   _len = _len / rate;
	   return;
    }

	void fadein() 
	{
	   // output ought to be 6 times as large as input (48000/8000).
	   if(_len == 0) { return ; }
	   unsigned int nsamples = _len / 2;
	   short   * ptr =  (short *)_ptr;

	   float   fadeGain = 0.0f;
	   float   fadeInc = fadeGain / (float)(nsamples - 10);

	   for (int i=0; i<nsamples && i<=100;  i++)
	   {
		 //if(i%16==0) printf("\n");
		 fadeGain = fadeGain + fadeInc;
		 
		 //printf("%05d ", ptr[i]);
		 if(fadeGain<1) ptr[i] = ptr[i] * fadeGain; 
		 //printf("/%05d ", ptr[i]);
       }
    }

	void fadeout() 
	{
	   // output ought to be 6 times as large as input (48000/8000).
	   if(_len == 0) { return ; }
	   unsigned int nsamples = _len / 2;
	   short   * ptr =  (short *)_ptr;

	   float   fadeGain = 1.0f;
	   float   fadeDec = fadeGain / (float)(nsamples - 10);

	   for (int i=0; i<nsamples;  i++)
	   {
		 fadeGain = fadeGain - fadeDec;
		 //if(i%16==0) printf("\n");
		 //printf("%05d", ptr[i]);
		 if(fadeGain>0) ptr[i] = ptr[i] * fadeGain; 
		 else ptr[i] = 0;
		 //printf("%05d ", ptr[i]);
       }
    }

    std::string str()
	{
	   return formatStr("PBuffer[size=%d, len=%d, pt=%d, "
			 "marker=%d, seq=%u, timestamp=%u, ssrc=%u, skip=%d]\n",
			 _size, _len, _pt,
			 _marker, _seq, _timestamp, _ssrc, _skip);
						 
	}
/*
   // if ptr -> new alloc
   PBuffer& operator=(const PBuffer& in)
   {
      delete[] _ptr;
      *this = in;

      _ptr = new unsigned char[_size];
      memcpy(_ptr, in.ptr(), _len);
      return *this;
   }
*/
};

//only frame size 20msec 
class PJittBuffer
{
private:
	PICO::CJBuffer _jbuff;
	PICO::JBZoneTrace_T _JB_ZoneTrace;
	PICO::JBParam_T _JB_Param;
	unsigned long _JB_StartTimeMSec;
	unsigned long _JB_FetchCount;
	unsigned long _JB_Used;
	unsigned long _JB_BypassDTMF;
	unsigned long _JB_BypassSID;
	unsigned int  _JB_TalkSepMSec;
	unsigned int _nSID;//session id
	unsigned long _nPTime;// frame ptime //ex> amr 20 ,g711 30
   //unsigned int _frames; // max frames (jitter buffer depth)
   //PBuffer * _ptr;

    PMutex	   _mutex;
public:
   PJittBuffer()
   {
      //_frames = MAX_BUF_FRAMES;
      //_ptr = new PBuffer[_frames];
		_JB_Used = 0;
      _JB_StartTimeMSec = 0;
      _JB_FetchCount = 0xFFFFFFFF; // guard infinite loop
      _nPTime = 20L;
   }

   ~PJittBuffer()
   {
      //delete [] _ptr;
   }
	bool init(int id, int nPTime);
	bool final();
	bool put(const PBuffer & buf);
	bool get(PBuffer& buf);//RTP packet(+RTP HDR) saved to buf, not payload for reducing bufcopy
		
};


class PRingBuffer
{
private:

   //unsigned int _frames; // max frames (jitter buffer depth)
   //PBuffer* _ptr;
	tbb::concurrent_bounded_queue<PBuffer> _queue;

   int _posPut; 
   int _posGet; 

public:
   PRingBuffer()
   {
	  _queue.set_capacity(10);
   }

   ~PRingBuffer()
   {
      //delete [] _ptr;
   }

   bool put(PBuffer& buf)
   {
	  return _queue.try_push(buf);
   }

   bool get(PBuffer& buf)
   {
	  buf.len() = 0;
	  return _queue.try_pop(buf);
   }
};


#endif // __PBuffer__

