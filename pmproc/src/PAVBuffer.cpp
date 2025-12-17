/***
 *
 ***/
#include "PAVBase.h"
#include "PAVBuffer.h"

bool PJittBuffer::init(int id,int nPTime)
{
   _nSID = id;
   _nPTime = nPTime;
   _JB_StartTimeMSec = 0;
   _JB_FetchCount = 0;
   _JB_BypassSID = 0;
   _JB_TalkSepMSec = 300;
   _JB_Param.Reset();

   PiResult_T ret;
   //Add code when rtpcomm start

   _JB_Param.UseAdaptive = 1;
   _JB_Param.AllowUnderMin = 1;
   _JB_Param.PTime = 20;
   _JB_Param.DropoutMax = 150;
   _JB_Param.MisorderMax = 2;

   _JB_Param.MinPrefetch = 4;
   _JB_Param.MaxPrefetch = 10;
   _JB_Param.InitPrefetch = 4;
   _JB_Param.BufferMax = 40;
   
   _JB_Param.UseTsAsIndex = 1;
   _JB_Param.DropsOnResShort = 0;
   _JB_Param.UseAdjust = 1;
   _JB_Param.CSID = _nSID;

   _mutex.lock();
   ret = _jbuff.Init(&_JB_Param);
   _mutex.unlock();
   if(ret != PiSuccess)
   {
      //printf("JB Init failed. res+%d, go no jb mode",ret);
      _JB_StartTimeMSec = 0;
      _JB_FetchCount = 0xFFFFFFFF; // guard infinite loop
      return false;
   }

   //_JB_StartTimeMSec = PICO::GetNowTimestampMS()+100;
   _JB_StartTimeMSec = PICO::GetNowTimestampMS()+60;
   _JB_FetchCount = 0;
   _JB_ZoneTrace.Init(_nSID,_JB_TalkSepMSec);
   _JB_ZoneTrace.Reset();
   return true;
}

bool PJittBuffer::final() //just reset buffer and parameter, not dealloc buffer
{
   _mutex.lock();
   _jbuff.Reset(1);//reset frame list and stats
   _mutex.unlock();
}

bool PJittBuffer::put(const PBuffer & buf)
{
   PICO::JBFrameDesc_T desc;
   desc.rxtime_msec = PICO::GetNowTimestampMS();
   desc.frmtype = PICO::E_JBFrameType_Normal;
   desc.pktlen =  buf.len();//pktlen;
   desc.seqnumber = buf.sequence();//sequenct number
   desc.timestamp = buf.timestamp();//timestamp;
   desc.pt = buf.pt();//paylaod type
   //desc.marker = 1;//marker
   //desc.isdtmf = 1;//set dtmf chk indicator 
   //desc.isdtmf = 0;//set dtmf chk indicator 
   //desc.isspeech = 1;// when amr-nb frame type below than 7 , it is speech
   desc.marker = 0; // buf.marker(); //marker
   desc.isspeech = buf.skip()?0:1;// when amr-nb frame type below than 7 , it is speech
   desc.index = -1; 


   PiResult_T ret;
   unsigned char discarded = 0;

   _mutex.lock();
   ret = _jbuff.PutFrame(_JB_ZoneTrace.IsSpeechZone,&desc,(pibyte*)buf.ptr(),(pibyte*)&discarded);
   _mutex.unlock();

   if(( ret != PiSuccess) &&
      ( ret != PiRErrAlready) &&
      ( ret != PiRErrTooLate )) 
   {
		 LOG(LINF,"%s:%s:%d JBID[%d] JB PutFrame Failed. res=%d\n",__FILE__,__func__,__LINE__, _nSID, ret);
         return false;
   }
   return true;
}

bool PJittBuffer::get(PBuffer& buf)
{
   PiResult_T ret;
   PICO::JBFrameDesc_T desc;

   unsigned long expcount;
   unsigned long nowtime;

   nowtime = PICO::GetNowTimestampMS();
  
   buf.len() = 0;
   //time checking
   if(_JB_StartTimeMSec > nowtime)
   {
      //not process time
#ifdef _DEBUG_JBUF_
      LOG(LINF,"%s:%s:%d JBID[%d] _JB_StartTimeMSec > nowtime\n",__FILE__,__func__,__LINE__, _nSID);
#endif
      return false;
   }

   expcount = (nowtime - _JB_StartTimeMSec)/_nPTime;

   if(_JB_FetchCount > expcount)
   {
#ifdef _DEBUG_JBUF_
      LOG(LINF,"%s:%s:%d _JB_FetchCount(%d) > expcount(%d)\n",__FILE__,__func__,__LINE__,_JB_FetchCount,expcount);
#endif      
      return false;
   }

   _JB_FetchCount++;

   _mutex.lock();
   ret = _jbuff.GetFrame(&desc, buf.ptr(), buf.size());
   _mutex.unlock();

   if(ret != PiSuccess)
   {
      //PiRErrJBSysFail;
#ifdef _DEBUG_JBUF_
      LOG(LINF,"%s:%s:%d JBID[%d] ret(%d) != PiSuccess\n",__FILE__,__func__,__LINE__, _nSID, ret);
#endif            
      return false;
   }
   buf.sequence() = desc.seqnumber;
   buf.timestamp() = desc.timestamp;
   buf.pt() = desc.pt;
      
   if(desc.frmtype==PICO::E_JBFrameType_Missing){
        _JB_ZoneTrace.Update(0);
        //*oresult = PICO::E_PktChkRes_JB_NotData ;
         buf.skip() = true;
    }else if(desc.frmtype==PICO::E_JBFrameType_ZeroPrefetch){
        _JB_ZoneTrace.Update(0);
         buf.skip() = true;
        //*oresult = PICO::E_PktChkRes_JB_NotData ;
    }else if(desc.frmtype==PICO::E_JBFrameType_ZeroEmpty){
        _JB_ZoneTrace.Update(0);
         buf.skip() = true;
        //*oresult = PICO::E_PktChkRes_JB_NotData ;
    }else if(desc.frmtype==PICO::E_JBFrameType_Normal){
        _JB_ZoneTrace.Update(desc.isspeech);
         buf.skip() = desc.isspeech!=1; // if speech  skip is false; // modified by jcryu
         buf.len() = desc.pktlen;   
        //*oresult = PICO::E_PktChkRes_RTP        ;
    }else {
#ifdef _DEBUG_JBUF_    
        LOG(LINF,"%s:%s:%d JBID[%d] SOMETHING WRONG JB FT(%d) unknown\n",__FILE__,__func__,__LINE__,_nSID, desc.frmtype);
#endif        
        //*oresult = PICO::E_PktChkRes_BadPkt ;
         return false;
    }
#ifdef _DEBUG_JBUF_    
        fprintf(stderr,"%s success\n",__func__);
#endif          
  return true;
}
 
