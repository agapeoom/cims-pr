#ifndef __TMBuffer__
#define __TMBuffer__


#include <deque>
#include <map>
#include <string>
#include <cstring>

#include <tbb/tbb.h>

#include "pbase.h"
#include "TMpaUtil.h"
#include "TMBase.h"
#include "CJBuffer.h"

class TMBuffer
{
public:
   int           _codec;  // codec of contents (_ptr)
   int           _format; // 0:origianl(IF2/File), 1:octet align(amr/amrwb), 2: bwe(amr/amrwb)
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
   TMBuffer()
   {
        _size = MAX_FRAME_LENGTH;
		clear();
        //_ptr = new unsigned char[_size];
    }

   TMBuffer(unsigned int size)
   {
       _size = size;
		clear();
       //_ptr = new unsigned char[_size];
   }

    ~TMBuffer()
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
    
	int & codec() { return _codec; }
	int & format() { return _format; }
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
    void set(TMBuffer& buf)
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
    bool mix(TMBuffer& buf)
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
	   return formatStr("TMBuffer[size=%d, len=%d, codec=%d, pt=%d, "
			 "marker=%d, seq=%u, timestamp=%u, ssrc=%u, skip=%d]\n",
			 _size, _len, _codec, _pt,
			 _marker, _seq, _timestamp, _ssrc, _skip);
						 
	}
/*
   // if ptr -> new alloc
   TMBuffer& operator=(const TMBuffer& in)
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
class TJitterBuffer
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
   //TMBuffer * _ptr;

    PMutex	   _mutex;
public:
   TJitterBuffer()
   {
      //_frames = MAX_BUF_FRAMES;
      //_ptr = new TMBuffer[_frames];
		_JB_Used = 0;
      _JB_StartTimeMSec = 0;
      _JB_FetchCount = 0xFFFFFFFF; // guard infinite loop
      _nPTime = 20L;
   }

   ~TJitterBuffer()
   {
      //delete [] _ptr;
   }
	bool init(int id, int nPTime);
	bool final();
	bool put(const TMBuffer & buf);
	bool get(TMBuffer& buf);//RTP packet(+RTP HDR) saved to buf, not payload for reducing bufcopy
		
};


class TRingBuffer
{
private:

   //unsigned int _frames; // max frames (jitter buffer depth)
   //TMBuffer* _ptr;
	tbb::concurrent_bounded_queue<TMBuffer> _queue;

   int _posPut; 
   int _posGet; 

public:
   TRingBuffer()
   {
	  _queue.set_capacity(10);
   }

   ~TRingBuffer()
   {
      //delete [] _ptr;
   }

   bool put(TMBuffer& buf)
   {
	  return _queue.try_push(buf);
   }

   bool get(TMBuffer& buf)
   {
	  buf.len() = 0;
	  return _queue.try_pop(buf);
   }
};


#endif // __TMBuffer__

