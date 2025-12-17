#ifndef __TVoiceCoder__
#define __TVoiceCoder__



#include "TMBase.h"
#include "TCodecs.h"
#include "TMProcessor.h"



class TVoiceCoder : public  TMProcessor
{
public:
   enum MODE {
      ENC         = 0x00,
      DEC         = 0x01,
   };

private:
   MODE  _mode; 

   TCodec * _pcoder; 

   bool		   _bdtmf;
   TDtmfDetector  _dtmf;

public:
   TVoiceCoder() :_pcoder(NULL), _bdtmf(false){}
   virtual ~TVoiceCoder() { final(); }

   bool init(TCODEC codec, MODE mode)
   {
	  final();
	  _mode = mode;
	  switch(codec)
	  {
	  case TCODEC_AMR:
		 if(_mode == ENC) _pcoder = new TAmrEncoder();
		 else            _pcoder = new TAmrDecoder();
		 break;
	  case TCODEC_AMRWB:
		 if(_mode == ENC) _pcoder = new TAmrwbEncoder();
		 else            _pcoder = new TAmrwbDecoder();
		 break;
      }

	  if(!_pcoder) return false;

	  _bdtmf = false;
	  _dtmf.init();

      return _pcoder->init();

   }

   bool dtmf(bool on)
   {
	  if(on && _mode == DEC) 
	  {
		 _bdtmf = true;
	  } 
	  else 
	  {
		 _bdtmf = false;
	  }
   }

   bool final()
   {
	  if(!_pcoder) return true;

	  _dtmf.final();

	  delete _pcoder;
	  _pcoder = NULL;

	  return true;
   }

   virtual  bool proc(const TMBuffer& in, TMBuffer& out)
   {
	  bool res;

	  out._pt        = in._pt; 
	  out._marker    = in._marker;
	  out._seq       = in._seq;
	  out._timestamp = in._timestamp;
	  out._ssrc      = in._ssrc;
	  out._skip      = in._skip;
	  out._dtmf      = in._dtmf;
	  out._dtmfCode  = in._dtmfCode;

	  res = _pcoder->code(in, out);

	  if(res && _bdtmf) 
	  {
		 _dtmf.decode(out);
	  }
	  return res;
   }

};

class TVoiceCoder2 : public  TMProcessor
{
private:
   TCODEC _icodec;
   TCODEC _ocodec;

   TCodec * _decoder; 
   TCodec * _encoder; 
  
   TMBuffer  _tbuf;

   int _ihz;
   int _ohz;

   int _resmaplingX;  
   short        _lastSample;
public:
   TVoiceCoder2() :_decoder(NULL), _encoder(NULL){}
   virtual ~TVoiceCoder2() { final(); }

   bool init(const TCODEC icodec, const TCODEC ocodec)
   {
	  final();
	  _icodec = icodec;
	  _ocodec = ocodec;
      switch (_icodec)
      {
      case TCODEC_LPCM8K:
		 _decoder = NULL;  
		 _ihz = 8000;
		 break;
      case TCODEC_LPCM16K:
		 _decoder = NULL; 
		 _ihz = 16000;
		 break;
      case TCODEC_LPCM32K:
		 _decoder = NULL; 
		 _ihz = 32000;
		 break;
      case TCODEC_AMR:
		 _decoder = new TAmrDecoder(); 
		 _ihz = LPCM_RATE_DEFAULT;
		 break;
      case TCODEC_AMRWB:
		 _decoder = new TAmrwbDecoder(); 
		 _ihz = LPCM_RATE_DEFAULT;
		 break;
      //case TCODEC_EVS16K:
	  //	 _decoder = new TEvsDecoder(16000); 
	  //	 break;
      default:
		 _decoder = NULL;
		 _ihz = 0;
		 break;
	  }

      switch (_ocodec)
      {
      case TCODEC_LPCM8K:
		 _encoder = NULL;
		 _ohz = 8000;
		 break;
      case TCODEC_LPCM16K:
		 _encoder = NULL;
		 _ohz = 16000;
		 break;
      case TCODEC_LPCM32K:
		 _encoder = NULL;
		 _ohz = 32000;
		 break;
      case TCODEC_AMR:
		 _encoder = new TAmrEncoder(); 
		 _ohz = LPCM_RATE_DEFAULT;
		 break;
      case TCODEC_AMRWB:
		 _encoder = new TAmrwbEncoder(); 
		 _ohz = LPCM_RATE_DEFAULT;
		 break;
      //case TCODEC_EVS16K:
	  //	 _encoder = new TEvsEncoder(16000); 
	  //	 _ohz = 16000;
	  //	 break;
      default:
		 _encoder = NULL;
	  	 _ohz = 0;
		 break;
	  }

	  if(!_ihz || !_ohz) 
	  {
		 final();
		 return false;
	  }


	  if(_decoder) 
	  {
		 bool bRes = _decoder->init();
		 if(!bRes) 
		 {
			final();
			return false;
		 } 
	  }
	  if(_encoder) 
	  {
		 bool bRes = _encoder->init();
		 if(!bRes) 
		 {
			final();
			return false;
		 } 
	  }

	  _resmaplingX = _ohz>=_ihz?(_ohz/_ihz):-_ihz/_ohz;
      _lastSample = 0;

	  return true;
   }

   bool final()
   {
	  if(_decoder) delete _decoder;
	  if(_encoder) delete _encoder;

      _decoder = NULL;
	  _encoder = NULL;
	  return true;
   }

   virtual  bool proc(const TMBuffer& in, TMBuffer& out)
   {
	  out._pt        = in._pt; 
	  out._marker    = in._marker;
	  out._seq       = in._seq;
	  out._timestamp = in._timestamp;
	  out._ssrc      = in._ssrc;
	  out._skip      = in._skip;
	  out._dtmf      = in._dtmf;
	  out._dtmfCode  = in._dtmfCode;

      if(_decoder && _encoder) 
	  {
		 _decoder->code(in, _tbuf);
		 if(_resmaplingX > 1)
		 {
			_lastSample = _tbuf.upsample(_resmaplingX, _lastSample);
		 }
		 else if(_resmaplingX < 1)
		 {
			_tbuf.downsample(-_resmaplingX);
		 }
		 _encoder->code(_tbuf, out);
	  }
	  else if(_decoder && !_encoder)
	  {
		 _decoder->code(in, out);

		 if(_resmaplingX > 1)
		 {
			_lastSample = out.upsample(_resmaplingX, _lastSample);
		 }
		 else if(_resmaplingX < 1)
		 {
			out.downsample(-_resmaplingX);
		 }
	  } 
	  else if(!_decoder && _encoder)
	  {
		 if(_resmaplingX > 1)
		 {
			_lastSample = in.upsample(_resmaplingX, _lastSample);
		 }
		 else if(_resmaplingX < 1)
		 {
			in.downsample(-_resmaplingX);
		 }
		 _encoder->code(in, out);
	  } else 
	  {
		 if(_resmaplingX > 1)
		 {
			_lastSample = in.upsample(_resmaplingX, _lastSample);
		 }
		 else if(_resmaplingX < 1)
		 {
			in.downsample(-_resmaplingX);
		 }
		 out = in;
	  }
	  return true;
   }
};

#endif // __TVoiceCoder__
