#ifndef __PAVFormat__
#define __PAVFormat__


#include "libavcodec/avcodec.h"

#include "amtrtp.h"
#include "amtrtpdtmf.h"
#include "amtrtpcomm.h"


#include "havrtputil.h"

#include "PAVBase.h"
#include "PMPBase.h"

class PAVFormat 
{
public:
   enum MODE { RX=1, TX=2 };
   enum MAX  { MAX_STREAMS = 4 };

protected:
   AVFormatContext * _ctx;
   AVInputFormat *   _fmt;
   //AVOutputFormat *   _fmt;
 
   PAVParam		  _paramA;
   PAVParam		  _paramV;
   PAVParam		  _paramNone;
   
   MODE _mode;
   std::string	  _fname;
   bool	 _bwrite_hdr;

public: 

   PAVFormat() :_ctx(NULL){};
   virtual ~PAVFormat() { final(); };


   virtual bool init(const std::string & fname, const std::string & fmtname, MODE mode)
   {
	  //if(_ctx || _fmt)  return false;
	  if(_ctx)  return false;

	  _paramA.init();
	  _paramV.init();
	  _paramNone.init();

	  if(mode == TX) 
	  {
		 avformat_alloc_output_context2(&_ctx, NULL, fmtname.c_str(), fname.c_str());
		 if(!_ctx)
		 {
			// error log
			fprintf(stderr, "# failed to avformat_alloc_output_context2(file=%s, format=%s)!\n", fname.c_str(), fmtname.c_str()); 
			return false;
		 }
	  } 
	  else if(mode == RX)
	  {
		 int ret = avformat_open_input(&_ctx, fname.c_str(), NULL, NULL) ;
		 if(ret <0) 
		 //if(!_ctx)
		 {
			// error log
			fprintf(stderr, "# failed to avformat_open_input(file=%s, format=%s)!\n", fname.c_str(), fmtname.c_str()); 
			return false;
		 }

		 ret = avformat_find_stream_info(_ctx, NULL);
		 if(ret<0 || _ctx->nb_streams <= 0) 
		 {
			fprintf(stderr, "# failed to avformat_find_stream_info(file=%s, format=%s)!\n", fname.c_str(), fmtname.c_str()); 
			avformat_free_context(_ctx);
			_ctx = NULL;
			return false;
		 }

		 for(int stream_index=0; stream_index<_ctx->nb_streams && stream_index<MAX_STREAMS; stream_index++)
		 {
			AVCodecContext * codec_ctx = _ctx->streams[stream_index]->codec;
			if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
			{
			   PAVParam & param = _paramA;
			   param.init(codec_ctx->codec_id);
			   param.bitrate = codec_ctx->bit_rate;
			   param.width = codec_ctx->width;
			   param.height = codec_ctx->height;
			   param.samplerate = codec_ctx->sample_rate;
			   param.channels = codec_ctx->channels;
			   param.stream_index = stream_index;

			}
			else if(codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
			{
			   PAVParam & param = _paramV;
			   param.init(codec_ctx->codec_id);
			   param.bitrate = codec_ctx->bit_rate;
			   param.width = codec_ctx->width;
			   param.height = codec_ctx->height;
			   if(codec_ctx->framerate.den > 0) 
			   {
				  param.fps = codec_ctx->framerate.num/codec_ctx->framerate.den;
			   } else if(codec_ctx->time_base.num > 0)
			   {
				  param.fps = codec_ctx->time_base.den/codec_ctx->time_base.num;
			   } else 
			   {
				  param.fps = 15;
			   }

			   param.samplerate = codec_ctx->sample_rate;
			   param.channels = codec_ctx->channels;
			   param.stream_index = stream_index;
			}
		 }
	  } 
	  else 
	  {
		 return false;
	  }

	  _fname = fname;
	  _mode = mode;

	  _bwrite_hdr = false;
	  return true; 
   } 

   virtual PAVParam & getstream(PAVTYPE type)  // param with stream_index
   {
	  if(type == PAVTYPE_AUDIO && _paramA.type == PAVTYPE_AUDIO)
	  {
		 return _paramA;
	  } 
	  else if(type == PAVTYPE_VIDEO && _paramV.type == PAVTYPE_VIDEO)
	  {
		 return _paramV;
	  }
   
	  return _paramNone;
   }

   virtual bool  addstream(PAVParam & param) 
   {
	  if(!_ctx || _mode != TX )  return false;

	  fprintf(stderr, "# PAVFormat::addstream (%s)!\n", param.str().c_str());
	  //AVCodec * codec = avcodec_find_encoder_by_name(param.avencoder_name().c_str());
	  AVCodec * codec = avcodec_find_encoder(param.avcodec()); 
	  if(!codec)
	  {
		 fprintf(stderr, "# failed to avcodec_find_encoder(%s)!\n", param.str().c_str());
		 return false;
	  }

	  AVStream * stream = avformat_new_stream(_ctx, codec);
	  if(!stream) 
	  {
		 fprintf(stderr, "# failed to avformat_new_stream(%s)!\n", param.str().c_str());
		 return false;
	  }

	  AVCodecContext * codec_ctx = stream->codec;
	  stream->start_time = 0;

	  codec_ctx->bit_rate = param.bitrate;
	  codec_ctx->width = param.width;
	  codec_ctx->height = param.height;
	  
	  codec_ctx->sample_rate = param.samplerate;
	  codec_ctx->channels = param.channels;


	  if(param.avtype() == AVMEDIA_TYPE_VIDEO) 
	  {
		 stream->time_base = (AVRational){1, param.fps}; 
		 //codec_ctx->sample_aspect_ratio = codec_ctx->sample_aspect_ratio;
		 codec_ctx->pix_fmt = avcodec_default_get_format(codec_ctx, codec->pix_fmts); // AV_PIX_FMT_YUV420P
		 codec_ctx->time_base = (AVRational){1, param.fps}; 
		 //codec_ctx->framerate = (AVRational){param.fps, 1}; 

	  } else if(param.avtype() == AVMEDIA_TYPE_AUDIO)
	  {
		 stream->time_base = (AVRational){1, param.samplerate}; 
		 codec_ctx->channel_layout = av_get_default_channel_layout(param.channels);
		 codec_ctx->sample_fmt = codec->sample_fmts[0];
		 codec_ctx->time_base = (AVRational){1, param.samplerate};
		 codec_ctx->extradata_size = 0;
		 codec_ctx->extradata = NULL;
	  }

	  if(_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	  {
		 codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	  }

	  int ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
	  if(ret<0) 
	  {
		 fprintf(stderr, "# failed to avcodec_parameters_from_context(%s)!\n", param.str().c_str());
		 return false;
	  }

	  if(param.avtype() == AVMEDIA_TYPE_VIDEO) 
	  {
		 param.stream_index = stream->index;
		 _paramV = param;

	  }
	  else if (param.avtype() == AVMEDIA_TYPE_AUDIO)
	  {
		 param.stream_index = stream->index;
		 _paramA = param;
	  } 
	  else 
	  {
		 return false;
	  }
	  return true;
   }
   
   virtual void final()
   {
	  if(!_ctx) return;

	  if(_mode == TX) 
	  {
		 //flush
		 if(_bwrite_hdr)
			av_write_trailer(_ctx);

		 for(int i=0; i<_ctx->nb_streams; i++)
		 {
			//if(_ctx->streams[i]->codec->extradata !=NULL)
			//{
			//   av_freep(&_ctx->streams[i]->codec->extradata);
			//}
			avcodec_close(_ctx->streams[i]->codec); // ???

			// free stream ???
			// av_freep(&_ctx->streams[i]);
		 }
		 
		 if(!(_ctx->oformat->flags & AVFMT_NOFILE))
		 {
			avio_closep(&_ctx->pb);
		 }
		 avformat_free_context(_ctx);
	  } 
	  else if(_mode == RX)
	  {
		 
		 for(int i=0; i<_ctx->nb_streams; i++)
		 {
			avcodec_close(_ctx->streams[i]->codec); // ???
		 }
		 avformat_close_input(&_ctx);
	  }
	  _ctx=NULL;
   }
   
   virtual bool rewind()
   {
	  if(!_ctx || _mode != RX) return false;

	  int ret = avformat_seek_file(_ctx, -1, 0, 0, 0, 0);
	  if(ret < 0) return false;
	  return true;
   }

   virtual bool put(PAVBuffer & in)
   {
	  if(!_ctx || _mode != TX) return false;

	  if(!_bwrite_hdr)
	  {
		 av_dump_format(_ctx, 0, _fname.c_str(), 1);
		 if(!(_ctx->oformat->flags & AVFMT_NOFILE))
		 {
			int ret = avio_open(&_ctx->pb, _fname.c_str(), AVIO_FLAG_WRITE);
			if(ret<0) 
			{
			   fprintf(stderr, "# failed to avio_open(file=%s)!\n", _fname.c_str());
			   return false;
			}
		 }
		 int ret = avformat_write_header(_ctx, NULL);
		 if(ret<0) 
		 {
			fprintf(stderr, "# failed to avformat_write_header(file=%s)!\n", _fname.c_str());
			return false;
		 }
		 _bwrite_hdr = true;
	  }

	  //fprintf(stdout, "# write packet(%s)!\n", in.str().c_str());

	  if(in._type == PAVTYPE_AUDIO) 
		 in._packet->stream_index = _paramA.stream_index;
	  else if(in._type == PAVTYPE_VIDEO) 
		 in._packet->stream_index = _paramV.stream_index;

	  int ret = av_interleaved_write_frame(_ctx, in._packet);
	  if(ret < 0) 
	  {
		 fprintf(stderr, "# failed to av_interleaved_write_frame(ret=%d, file=%s)!\n", ret, _fname.c_str());
		 return false;
	  }
	  return true;
   }

   virtual bool get(PAVBuffer & out)
   {
	  if(!_ctx || _mode != RX) return false;

	  AVPacket pkt;
	  //int ret = av_read_frame(_ctx, out._packet);
	  int ret = av_read_frame(_ctx, &pkt);
	  if(ret == AVERROR_EOF) 
	  {
		 out.set(&pkt, PAVTYPE_EOS);
		 //fprintf(stderr, "# End of frame(file=%s)!\n", _fname.c_str());
		 return true;
	  }

	  if(pkt.stream_index == _paramA.stream_index) 
	  {
		 out.set(&pkt, PAVTYPE_AUDIO);
	  } 
	  else if(pkt.stream_index == _paramV.stream_index)
	  {
		 out.set(&pkt, PAVTYPE_VIDEO);
	  } 
	  else 
	  {
		 out.set(&pkt, PAVTYPE_NONE);
	  }
	  return true;
   }
   
};

class PAVSyncFormat : public PAVFormat
{
public:
   enum { MAX_BUFFERS = 10 };
private:
   struct timeval _tvBaseTime;
   double _timeIntA;
   double _timeIntV;

   bool _bpacket;
   PAVBuffer _buf;


public:
   PAVSyncFormat() : PAVFormat() {}
   ~PAVSyncFormat() { final(); }


   bool init(const std::string & fname, const std::string & fmtname, MODE mode)
   {
	  _bpacket = false;
	  _timeIntA = 1.0;
	  _timeIntV = 1.0;
	  _buf.init();

	  gettimeofday(&_tvBaseTime, NULL);
	  bool bres =  PAVFormat::init(fname, fmtname, mode);

	  if(!bres) 
	  {
		 printf("# failed to PAVFormat::init()!(read only)\n");
		 return false;
	  }

	  if(_paramA.codec != PAVCODEC_NONE)  
	  {
		 _timeIntA = (double)1000/(double)_paramA.samplerate; // 20msec
	  } 
	  if(_paramV.codec != PAVCODEC_NONE)  // video only
	  {
		 //_timeIntV = (double)1000/_paramV.fps;
		 _timeIntV = (double)1000/_paramV.timescale;;
	  }
	  return true;
   }
   void final()
   {
	  PAVFormat::final();
	  _buf.final();
   }
   bool put(PAVBuffer & in)
   {
	  //printf("# failed to PAVSyncFormat::put()!(read only)\n");
#if 0

	  AVPacket * pkt = in.packet();
	  if(in._type == PAVTYPE_AUDIO) 
	  {
		 pkt.pts = 
	  }
	  else if(in._type == PAVTYPE_VIDEO) 
	  {
		 pkt.pts = 
	  }

	  return PAVFormat::put(in);
#else
	  return PAVFormat::put(in);
#endif
   }

   bool get(PAVBuffer & out)
   {
	  bool bres;
	  
	  struct timeval tvCurTime; 
	  gettimeofday(&tvCurTime, NULL);
	  unsigned int nDiffTimeMs = PDIFFTIME(tvCurTime, _tvBaseTime);

	  //_bpacket = true;
	  // printf("!!!!!!!!!!!!!\n");
	  if(!_bpacket) 
	  {
		 bool bres = PAVFormat::get(_buf);
	//	 printf("===============\n");
		 if(!bres)  return false;

	//	 printf("***************\n");
		 _bpacket = true;
	  }

	  int dts = 0;
	  AVPacket * pkt = _buf._packet;
	  if(_buf._type == PAVTYPE_AUDIO && _paramA.codec != PAVCODEC_NONE) // aduio packet 
	  {
		 dts = nDiffTimeMs/_timeIntA; 
		 if(dts>pkt->dts) 
		 {
#if 0
			CTime_ ctime;
			printf("%s SDMUX Audio diff=%u, interval=%f, dts=%d, pkt[dts=%d, pts=%d, dur=%d, pos=%d, size=%d]\n", 
					 (LPCSTR)ctime.Format("%T"), nDiffTimeMs, _timeIntA, dts, 
					 pkt->dts, pkt->pts, pkt->duration, pkt->pos, pkt->size);
#endif
			_bpacket = false;
			return out.set(pkt, PAVTYPE_AUDIO);
		 }
	  }

	  else if(_buf._type == PAVTYPE_VIDEO && _paramV.codec != PAVCODEC_NONE) // video packet
	  {
		 dts = nDiffTimeMs/_timeIntV;
		 if(dts>pkt->dts) 
		 {
#if 0
			CTime_ ctime;
			printf("%s SDMUX Video diff=%u, interval=%f, dts=%d, pkt[dts=%d, pts=%d, dur=%d, pos=%d, size=%d]\n", 
					 (LPCSTR)ctime.Format("%T"), nDiffTimeMs, _timeIntV, dts, 
					 pkt->dts, pkt->pts, pkt->duration, pkt->pos, pkt->size);
#endif
			_bpacket = false;
			return out.set(pkt, PAVTYPE_VIDEO);
		 }
	  }
	  else if(_buf._type == PAVTYPE_EOS) 
	  {
		 _bpacket = false;
		 return out.set(pkt, PAVTYPE_EOS);
	  }

	  return false; 
   }

   bool rewind()
   {
	  bool bres = PAVFormat::rewind();
	  if(bres) 
	  {
		 gettimeofday(&_tvBaseTime, NULL);
	  } 
	  return bres;
   }
};

class PAVRtpComm
{
private: 
   PAVParam	    _param;

   unsigned int _ptime;;
   unsigned int _timescale;
   unsigned int _timeRcv;
   unsigned int _timeSnd;
   unsigned int _seqRcv; 
   unsigned int _seqSnd;


   // for dtmf rfc2833
   unsigned int _lastTimestampDtmf;
   unsigned char _lastDurationDtmf;
   unsigned char _lastDigitDtmf;

   AMT::CRtpComm   _rtpComm;
   AMT::CRtpDtmf * _prtpDtmf;

   bool		   _baudio;
   AMT::CHAVStreamConv * _convRx;
   AMT::CHAVStreamConv * _convTx;

   unsigned char _pktdata[1600];
   unsigned char _framedata[4096*20];

public:
   PAVRtpComm() {  _prtpDtmf = new AMT::CRtpDtmf(&_rtpComm); }
   ~PAVRtpComm() {  delete _prtpDtmf; }

   bool init(const std::string & ipLoc, const unsigned int& portLoc, 
			 const bool bRtcp=false)
   {

	  if(!_init(ipLoc, portLoc, bRtcp)) return false;

	  return true;
   }
   
   bool setRmt(PAVParam param) 
   {
	  //fprintf(stderr, "# PAVRtpComm::setRmt (%s)!\n", param.str().c_str());
	  _convRx = AMT::CHAVStreamConv::CreateNew(param.codec, AMT::CHAVStreamConv::ST_RTP, AMT::CHAVStreamConv::ST_BYTE);
	  _convTx = AMT::CHAVStreamConv::CreateNew(param.codec, AMT::CHAVStreamConv::ST_BYTE, AMT::CHAVStreamConv::ST_RTP);

	  switch(param.codec)
	  {
	  case PAVCODEC_PCMA:
	  case PAVCODEC_PCMU:
		 _baudio = true;
		 _timescale = 8000;
		 _ptime = 160; // 20msec
		 _convRx->Open(160);
		 _convTx->Open(320);
		 break;
	  case PAVCODEC_AMR:
		 _baudio = true;
		 _timescale = 8000;
		 _ptime = 160;
		 _convRx->Open(128, AMT::CHAVStreamConv::PM_AMR_OCTET);
		 _convTx->Open(128, AMT::CHAVStreamConv::PM_AMR_OCTET);
		 
	  case PAVCODEC_AMRWB:
		 _baudio = true;
		 _timescale = 16000;
		 _ptime = 320;
		 _convRx->Open(256, AMT::CHAVStreamConv::PM_AMR_OCTET);
		 _convTx->Open(256, AMT::CHAVStreamConv::PM_AMR_OCTET);
		 break;
	  case PAVCODEC_H264:
		 _baudio = false;
		 _timescale = 90000;
		 _ptime = 6000;
		 _convRx->Open(1024*1024); 
		 _convTx->Open(1400);
		 if(param.extsize > 0) 
		 {
			_convTx->SetNalLengthSize(param.extra);
		 }
		 break;
	  default: 
		 printf("#FFMERR : failed to open stream conv(codec=%d)!\n", param.codec);
		 return false;
	  }

	  bool bres =_start(param.ip, param.port, param.cname, param.ssrc, param.pt1, param.pt2, false);
	  if(!bres)
	  {
		 return false;
	  }
	  _param = param;
	  return true;
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
       addr.nRtpBufLen = 16384*10;             // RTP Sender/Recever Socket buffer size.
       addr.nRtcpBufLen = 0;             // RTCP Sender/Recever Socket buffer size. //  * 0: default=16384, else: min=256, max=262142 bytes


       unsigned int rc = _rtpComm.OpenSocket(&addr, NULL);
       if(rc!=0) return false;

       _rtpComm.SetSendTimestampMode(false); // why?
	   _timeRcv = 0;
	   _timeSnd = 0;
	   _seqRcv = 0;
	   _seqSnd = 0;
       return true;
   }
  
   // set remote param. & start 
   bool _start(const std::string & ip, unsigned int port, 
               const std::string & cname, unsigned int ssrc, 
               unsigned char pt, unsigned char ptDtmf, bool bRtcp)
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
      rtpParam.ucRmtNatCount = 50;

	  
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

	  _convRx->Close();
	  _convTx->Close();

#if 0
	  avcodec_free_context(&_ctxcodec);
	  av_parser_close(_parser);
#endif
	  return true;
   }
   
   bool resetRmt(const std::string& ip, const unsigned int& port,
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
      rtpParam.ucRmtNatCount = 50;

      unsigned int rc = _rtpComm.ModifySession(&rtpParam);

      if (rc != 0)
      {
         // error log
         return false;
      }
      return true;
   }


   bool _get(PAVBuffer& out)
   {
	  int len = 0;
	  int isend = 0;
	  
	  bool bres = _convRx->TryGet(_framedata, sizeof(_framedata), &len, &isend);
	  if(bres && isend && len>0) 
	  {
		 _seqRcv++;
		 _timeRcv += _ptime;

		 bool bres = out.set_packet(_seqRcv, _timeRcv, _ptime, _framedata, len);	
		 return bres;
	  }
	  return false;;
   }
   
   bool get(PAVBuffer& out)
   {
	  AMT::RTPHEADER hdr;
      AMT::CRtpComm::PACKET_TYPE type = _rtpComm.DirectRecv(0, 1);
	  if(type == AMT::CRtpComm::PT_RTP) 
	  {
		 int len, marker;
		 int rc = _rtpComm.GetPayload_Hdr(_pktdata, sizeof(_pktdata), &len, &hdr);
		 if (rc == 0) 
		 {

			hdr.timestamp = htonl(hdr.timestamp);
			hdr.seqnumber = htons(hdr.seqnumber);
			hdr.ssrc = htonl(hdr.ssrc);

			//LOG(LINF, "PAVRtpComm::get()-video packet (pt=%d, seq=%d, timestamp=%u, marker=%d, len=%d, data[0]=0x%02X\n", 
		    //		  hdr.payloadtype, hdr.seqnumber, hdr.timestamp, hdr.marker, len, _pktdata[0]);


			if(_baudio && hdr.payloadtype == _param.pt2)
			{
			   AMT::RtpPayloadRfc2833 *pPayload = (AMT::RtpPayloadRfc2833 *)_pktdata; 
			   bool bEnd = pPayload->edge==1?true:false;
			   unsigned char ucDigit = pPayload->event;
			   unsigned char usDuration = ntohs(pPayload->duration);

			   //std::cout << formatStr("TRtpComm::recv()-dtmf(pt=%d, edge=%d, digit=%d, dur=%d", 
			   //					      hdr.payloadtype, pPayload->edge, pPayload->event, usDuration) << std::endl;

			   //LOG(LINF, "TRtpComm::recv()-dtmf(pt=%d, edge=%d, digit=%d, dur=%d", 
			   //				 hdr.payloadtype, pPayload->edge, pPayload->event, usDuration);
			   if(_lastTimestampDtmf != hdr.timestamp 
					 ||   _lastDurationDtmf != usDuration
					 ||   _lastDigitDtmf != ucDigit)
			   {
				  if(bEnd)
				  {
					 _lastTimestampDtmf = hdr.timestamp;
					 _lastDurationDtmf = usDuration;
					 _lastDigitDtmf = ucDigit;

					 out._dtmf = true;
					 out._dtmfCode = ucDigit;
					 //LOG(LAPI, "TRtpComm::recv()-dtmf_detect(pt=%d, edge=%d, digit=%d, dur=%d", 
					 //			hdr.payloadtype, pPayload->edge, pPayload->event, usDuration);
				  }
				  else
				  {
					 _lastTimestampDtmf = 0;//this code designed for catch-dtmfevent that incomming with same timestamp
				  }
			   }
			   return true;
			}
			out._dtmf = false;

			bool bres = false;

			if(_baudio) 
			{
			   _convRx->Put(true, _pktdata, len);
			} 
			else  //video
			{
			   _convRx->Put(hdr.marker==1, _pktdata, len);
			}
		 }
	  }
	  return _get(out);
   }

   bool put(const PAVBuffer & in)
   {
	  AVPacket * pkt = in.packet();

	  _timeSnd += _ptime;
	  if(in._dtmf) 
	  {
		 sendDtmf(in._dtmfCode, 50, 100);
		 return true;
	  } 

	  _convTx->Put(true, pkt->data, pkt->size);

	  while(1)
	  {
		 int len = 0;
		 int isend = 0;
		 int rc = -1;
		 bool bres = _convTx->TryGet(_pktdata, sizeof(_pktdata), &len, &isend);

		 if(!bres)  break;

		 //_seqSnd++;
		 if(_baudio)
		 {
			rc = _rtpComm.PutPayload(_pktdata, len, _timeSnd, 0);
		 }
		 else 
		 {
	  #if 0
			if((_pktdata[0]&0x1F) == 6) 
			{
			   printf("# Skip SEI \n");
			   continue;
			} 
			printf("# non SEI \n");
	  #endif 
			rc = _rtpComm.PutPayload(_pktdata, len, _timeSnd, isend);
		 }
	  }

	  return true;
   }
   
   bool sendDtmf(int digit, int duration, int volume)
   {
	  return _prtpDtmf->GenDTMF(0, digit, duration, volume);
   }

};


#endif // __PAVFormat__
