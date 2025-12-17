#ifndef __TRtpProcessor__
#define __TRtpProcessor__

#include <string>

#include "amtrtpdtmf.h"
#include "amtrtpcomm.h"
#include "TMBase.h"
#include "TMProcessor.h"

class TRtpComm
{
private: 
   TCODEC _codec;
   unsigned int _pt;
   unsigned int _ptDtmf;

   unsigned int _timescale;
   unsigned int _timeRcv;
   unsigned int _timeSnd;
   unsigned int _ptime;;
   unsigned int _seqSnd;

   unsigned int _lastTimestampDtmf;
   unsigned char _lastDurationDtmf;
   unsigned char _lastDigitDtmf;

   AMT::CRtpComm   _rtpComm;
   AMT::CRtpDtmf * _prtpDtmf;

public:
   TRtpComm() {  _prtpDtmf = new AMT::CRtpDtmf(&_rtpComm); }
   ~TRtpComm() {  delete _prtpDtmf; }

   bool init(const std::string & ipLoc, const unsigned int& portLoc, 
			 const bool bRtcp)
   {

	  if(!_init(ipLoc, portLoc, bRtcp)) return false;

	  return true;
   }
   
   bool start(const TCODEC  codec,  
             const std::string & ipRmt, const unsigned int& portRmt, 
			 const std::string & cname, const unsigned int & ssrc,
			 const unsigned char & pt, const unsigned char & ptDtmf,
			 const bool bRtcp)
   {
	  _codec = codec;
	  switch(_codec) 
	  {
	  case TCODEC_LPCM8K:
		 _timescale = 8000;
		 _ptime = 160;
		 break;
	  case TCODEC_LPCM16K:
		 _timescale = 16000;
		 _ptime = 320;
		 break;
	  case TCODEC_LPCM32K:
		 _timescale = 32000;
		 _ptime = 640;
		 break;
	  case TCODEC_AMR:
		 _timescale = 8000;
		 _ptime = 160;
		 break;
	  case TCODEC_AMRWB:
	  //case TCODEC_EVS:
		 _timescale = 16000;
		 _ptime = 320;
		 break;
	  default:
		 _timescale = 8000;
		 _ptime = 320;
		 break;
	  }

	  _pt = pt;
	  _ptDtmf = ptDtmf;

	  return _start(ipRmt, portRmt, cname, ssrc, pt, ptDtmf, bRtcp);
   }
   
   void * rtpDtmf() 
   {
	  return  _prtpDtmf;
   }
    // set local info & open RTP Socket 
   bool _init(const std::string & ip, const unsigned int& port, const bool bRtcp)
   {
       AMT::CRtpComm::IP_PORT addr;
       memset(&addr, 0, sizeof(AMT::CRtpComm::IP_PORT));

       addr.bRtpOn = true;                 // true: on
       addr.bRtcpOn = bRtcp;               // true: on
       addr.usRtcpMode = 0;   // 0:only socket open , 1:Send Report,Receive Report
       addr.ver = 1;//1:ipv4 2:ipv6 3:ipv4 && ipv6
       strcpy(addr.szAddr, ip.c_str());    // "" : ANY
       //addr.szAddr6[MAX_IP6_ADDR] = true;    // "" : ANY
       addr.nRtpPort = port;               // 0: ANY, -1: use recv-socket.
       addr.nRtcpPort = port + 1;              // 0: ANY, -1: use recv-socket.
       addr.nRtpBufLen = 1500;             // RTP Sender/Recever Socket buffer size.
       addr.nRtcpBufLen = 1500;             // RTCP Sender/Recever Socket buffer size. //  * 0: default=16384, else: min=256, max=262142 bytes


       unsigned int rc = _rtpComm.OpenSocket(&addr, NULL);
       if(rc!=0) return false;

       _rtpComm.SetSendTimestampMode(false); // why?
	   _timeRcv = 0;
	   _timeSnd = 0;
	   _seqSnd = 0;
       return true;
   }
  
   // set remote param. & start 
   bool _start(const std::string & ip, const unsigned int & port, 
               const std::string & cname, const unsigned int & ssrc, 
               const unsigned char & pt, const unsigned char & ptDtmf, bool bRtcp)
   {
      AMT::CRtpComm::RTP_PARAM  rtpParam;
      memset(&rtpParam, 0, sizeof(AMT::CRtpComm::RTP_PARAM));

      strcpy(rtpParam.cname, cname.c_str());
      rtpParam.nSSRC = ssrc;
	  //rtpParam.nSSRC = -1; // auto-assignment.
      rtpParam.nRcvPayloadType = pt;//
      rtpParam.nRcvPayloadType2 = ptDtmf;//
      rtpParam.nRcvPayloadSize = 1500;  
      
	  rtpParam.nSndPayloadType = pt;//
      rtpParam.nSndPayloadType2 = ptDtmf;//
      rtpParam.nSndPayloadSize = 1500;

      rtpParam.nTimeScale = _timescale;
      rtpParam.nMaxBufferCount = 0; // Of internal jitter buffer
      rtpParam.ucRmtNatCount = 0;

	  
      strcpy(rtpParam.remote.szAddr, ip.c_str());
      rtpParam.remote.ver = 1; // ipv4
      rtpParam.remote.nRtpPort = port;
      rtpParam.remote.bRtpOn = true;
      rtpParam.remote.bRtcpOn = bRtcp;
      rtpParam.remote.usRtcpMode = 0; //0:read only  1: send SR/RR 


      unsigned int rc = _rtpComm.StartSession(&rtpParam);

      if (rc != 0) 
      {
         // error log
         return false;
      }

      AMT::DTMFGEN_STS  dtmfGenSts;
	  dtmfGenSts.bEnable = true;
	  dtmfGenSts.ucPayloadType = ptDtmf;
	  dtmfGenSts.usSeq = 0;
	  dtmfGenSts.nRtpPort = 0;
	  dtmfGenSts.uTimestamp = 0;
	  dtmfGenSts.uTimeScale = _timescale;
	  dtmfGenSts.lFirstStartTimeMS = 0;

	  _prtpDtmf->Init();
	  _prtpDtmf->SetParams(AMT_DTMFGEN_PARM_ALL, &dtmfGenSts);
	  //_prtpDtmf->GenInit(); ...

      return true;
   }

   bool final() 
   {
	  _rtpComm.CloseSocket();
	  return true;
   }
   
   bool setRmt(const std::string& ip, const unsigned int& port,
      const std::string& cname, const unsigned int& ssrc,
      const unsigned char& pt, const unsigned char& ptDtmf, bool bRtcp)
   {
      AMT::CRtpComm::RTP_PARAM  rtpParam;
      memset(&rtpParam, 0, sizeof(AMT::CRtpComm::RTP_PARAM));

      strcpy(rtpParam.cname, cname.c_str());
      rtpParam.nSSRC = ssrc;
      rtpParam.nRcvPayloadType = pt;//
      rtpParam.nRcvPayloadType2 = ptDtmf;//
      rtpParam.nSndPayloadType = pt;//
      rtpParam.nSndPayloadType2 = ptDtmf;//

      //rtpParam.nInitSequence = 0;
      //rtpParam.nDelayTime = 0; // millisecond.
      rtpParam.nSndPayloadSize = 1500;
      //rtpParam.nMaxBufferCount = 0; // Of internal jitter buffer
      rtpParam.nRcvPayloadSize = 1500;

      rtpParam.remote.bRtpOn = true;
      rtpParam.remote.bRtcpOn = bRtcp;
      rtpParam.remote.usRtcpMode = 0; 
      rtpParam.ucRmtNatCount = 0;

      unsigned int rc = _rtpComm.ModifySession(&rtpParam);

      if (rc != 0)
      {
         // error log
         return false;
      }
      return true;
   }

   bool recv(TMBuffer& buf)
   {
	  buf.clear();
  
	  AMT::RTPHEADER hdr;
      AMT::CRtpComm::PACKET_TYPE type = _rtpComm.DirectRecv(0, 1);
	  if(type != AMT::CRtpComm::PT_RTP) {
		 return false; 
	  }
      //int rc = _rtpComm.GetPacket(buf.ptr(), buf.size(), (int*)&(buf.len()));

      int len, marker;
      //int rc = _rtpComm.GetPayload(buf.ptr(), buf.size(), &len, &marker);
      int rc = _rtpComm.GetPayload_Hdr(buf.ptr(), buf.size(), &len, &hdr);
      if (rc != 0) return false;

	  hdr.timestamp = htonl(hdr.timestamp);
	  hdr.seqnumber = htonl(hdr.seqnumber);
	  hdr.ssrc = htonl(hdr.ssrc);

	  buf.len() = len;
	  //conv timestamp -> milli-second 8000hz : rtptimestamp/8, 16000Hz : rtptimestamp/16
	  //buf.timestamp() = (_codec == TCODEC_AMRWB)?hdr.timestamp>>4:hdr.timestamp>>3;

	  buf.timestamp() = (_codec == TCODEC_AMRWB)?hdr.timestamp>>1:hdr.timestamp;
	  //std::cout << "## RECV : time = " << buf.timestamp() << "/" << _rtpComm.GetTimestamp() << std::endl;

	  buf.sequence()  = hdr.seqnumber;
	  buf.pt()   = hdr.payloadtype;
	  buf.ssrc() = hdr.ssrc;
	  buf.marker() = hdr.marker;
	  buf.skip() = false;

	  // DTMF LOG CHECK!
	  //std::cout << formatStr("TRtpComm::recv()- %s", buf.str().c_str()) << std::endl;
      //LOG(LINF, "TRtpComm::recv()-%s", buf.str().c_str());

	  if(hdr.payloadtype == _ptDtmf)
	  {
		 AMT::RtpPayloadRfc2833 *pPayload = (AMT::RtpPayloadRfc2833 *)buf.ptr();	
		 bool bEnd = pPayload->edge==1?true:false;
		 unsigned char ucDigit = pPayload->event;
		 unsigned char usDuration = ntohs(pPayload->duration);

		 //std::cout << formatStr("TRtpComm::recv()-dtmf(pt=%d, edge=%d, digit=%d, dur=%d", 
		 //					      hdr.payloadtype, pPayload->edge, pPayload->event, usDuration) << std::endl;

		 //LOG(LINF, "TRtpComm::recv()-dtmf(pt=%d, edge=%d, digit=%d, dur=%d", 
		 //				 hdr.payloadtype, pPayload->edge, pPayload->event, usDuration);
		 if(_lastTimestampDtmf != buf.timestamp() 
			||   _lastDurationDtmf != usDuration
			||   _lastDigitDtmf != ucDigit)
		 {
			if(bEnd)
			{
			   _lastTimestampDtmf = buf.timestamp();
			   _lastDurationDtmf = usDuration;
			   _lastDigitDtmf = ucDigit;
		
			   buf._dtmf = true;
			   buf._dtmfCode = ucDigit;
			   buf.len() = 0;
			   buf.skip() = true;
			   //LOG(LAPI, "TRtpComm::recv()-dtmf_detect(pt=%d, edge=%d, digit=%d, dur=%d", 
			   //			hdr.payloadtype, pPayload->edge, pPayload->event, usDuration);
			}
			else
			{
			   _lastTimestampDtmf = 0;//this code designed for catch-dtmfevent that incomming with same timestamp
			}
		 }
		 buf.len() = 0;
		 buf.skip() = true;
	  }
	  return true;
   }

   bool send(const TMBuffer & buf)
   {
	  _timeSnd += _ptime;
	  _seqSnd++;

	  if(buf.skip()) return true; // for sid

      int rc = _rtpComm.PutPayload(buf.ptr(), buf.len(), _timeSnd, 0);
      //int rc = _rtpComm.PutPayload(buf.ptr(), buf.len(), _timeSnd, buf.marker());
      //int rc = _rtpComm.PutPayloadWithLastTimestamp(buf.ptr(), buf.len(), buf.marker(), buf.pt());
	  if(rc > 0) return true;

	  return false;
   }
   
   bool sendDtmf(int digit, int duration, int volume)
   {
	  return _prtpDtmf->GenDTMF(0, digit, duration, volume);
   }


};


class TRtpSender : public TMProcessor
{
private:
   TRtpComm * _prtpComm;
   //AMT::CRtpDtmfSender * _dtmfGen;

public:
   TRtpSender()
	  : _prtpComm(NULL)
    {

    }

    ~TRtpSender()
    {

    }

    bool init(TRtpComm * prtpComm)
    {
       _prtpComm = prtpComm;
	   //_dtmfGen = AMT::CRtpDtmfSender::GetInstance();
	   return true;
    }

   bool final()
   {
	  if(!_prtpComm) return false;
	  _prtpComm = NULL;
	  return true;
   }

   bool sendDtmf(int digit, int duration, int volume)
   {
	  return _prtpComm->sendDtmf(digit, duration, volume);
   }

   bool proc(const TMBuffer& in, TMBuffer& out)
   {
	  if(!_prtpComm) return false;

	  return _prtpComm->send(in);
   }
};

class TRtpReceiver : public TMProcessor
{
private:
   TRtpComm * _prtpComm;

public:
   TRtpReceiver()
	  : _prtpComm(NULL)
   {

   }

   ~TRtpReceiver()
   {

   }

   bool init(TRtpComm * prtpComm)
   {
      _prtpComm = prtpComm;
	  return true;
   }

   bool final()
   {
	  if(!_prtpComm) return false;
	  _prtpComm = NULL;
	  return true;
   }

   bool proc(const TMBuffer& in, TMBuffer& out)
   {
	  if(!_prtpComm) return false;
      return _prtpComm->recv(out);
   }
  
};


#endif // __TRtpProcessor__
