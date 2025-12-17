#ifndef __TAFilter__
#define __TAFilter__

// Audio Filter
// DTMF Detector

#include <string>

#include "hudtmf.h"

#include "TMBuffer.h"


enum TDTMF_CODE {
   DTMF_0 = 0,
   DTMF_1 = 1,
   DTMF_2 = 2,
   DTMF_3 = 3,
   DTMF_4 = 4,
   DTMF_5 = 5,
   DTMF_6 = 6,
   DTMF_7 = 7,
   DTMF_8 = 8,
   DTMF_9 = 9,
   DTMF_STAR = 10, // *
   DTMF_PND = 11, // #
   DTMF_A = 12,
   DTMF_B = 13,
   DTMF_C = 14,
   DTMF_D = 15,
};

class TDtmfDetector
{
private:
   CHuDtmfDetector _dtmf;
   unsigned int _hz;
   unsigned int _ptime;
   unsigned int _ltime; // last timestamp 

   TMBuffer		  _tmpBuf;
public:
   TDtmfDetector() {};
   ~TDtmfDetector() {};

   bool init()
   {
      int minDur	   = DTMF_MIN_DURATION; 
      int nTimeWinSize = 30; //? 
      int nPowerThresh = 100;
      bool bDetectOnce = true; // ?
	  _hz = LPCM_RATE_DEFAULT; // 16000;

	  _ptime = 160;
	  _ltime = 0; 

      int rc = _dtmf.Open(_hz, nTimeWinSize, minDur, nPowerThresh, bDetectOnce);

	  _tmpBuf.reset(FRAME_SIZE_LPCM);
	  _tmpBuf.len() = FRAME_SIZE_LPCM;
      if (rc != 0) return false;
      return true;

   }

   bool decode(TMBuffer& in)
   {

	  int dtmfCode;
	  int duration;
	  unsigned int rc = 0; 
	  
	  if(in.skip()) 
	  {
		 rc = _dtmf.Decode(_tmpBuf.ptr(), _tmpBuf.len(), (CHuDtmf::DTMF_CODE *)&dtmfCode, &duration);
	  } 
	  else 
	  {
		 rc = _dtmf.Decode(in.ptr(), in.len(), (CHuDtmf::DTMF_CODE *)&dtmfCode, &duration);
		 _ltime = in._timestamp;
	  }
#if 0
	  int dtime = in._timestamp - _ltime;
	  if(dtime>_ptime) 
	  {
		 int loop = dtime / _ptime - 1;
		 loop  = loop > 8 ? 8 : loop ; 
		 for(int i=0;i<loop; i++)
		 {
			rc = _dtmf.Decode(_tmpBuf.ptr(), _tmpBuf.len(), (CHuDtmf::DTMF_CODE *)&dtmfCode, &duration);
			if (rc != 0) continue;

		 }
	  }
#endif

      if (rc != 0) return false;
	  if(dtmfCode > DTMF_D) return false;

	  //printf("# DTMF detection : ts=%ld, code=%d, duration=%d\n\n", _ltime, dtmfCode, duration);
	  
	  in._dtmf = true;
	  in._dtmfCode = dtmfCode;
      return true;
   }

   void final()
   {
      _dtmf.Close();
   }

};

#endif  //__TAFilter__
