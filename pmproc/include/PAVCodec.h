#ifndef __PAVCodec__
#define __PAVCodec__


#include "PAVBase.h"
#include "PMPBase.h"

#include "PAVBuffer.h"

class PAVDecoder // for using ffmpeg 
{
protected:
   PAVParam			 _param;
   //AVStream       _stream;
   AVCodecContext * _ctx; //
   //SwrContext     * _ctxswr;


public:
   PAVDecoder() : _ctx(NULL) { }
   virtual ~PAVDecoder() { final(); }

   virtual bool init(PAVParam & param)
   {
	  AVCodec * pcodec = NULL;
	  //pcodec = avcodec_find_decoder_by_name(param.avdecoder_name().c_str()); 
	  pcodec = avcodec_find_decoder(param.avcodec());
	  if(!pcodec) 
	  {
		 //error log
		 return false;
	  }

	  _ctx = avcodec_alloc_context3(pcodec);
	  if(!_ctx) 
	  {
		 //error log
		 return false;
	  }


#if 0
	  memset(&_stream, 0, sizeof(AVStream));
	  int ret = avcodec_parameters_to_context(_ctx, _stream.codecpar); 
	  if(ret < 0) 
	  {
		 //error log 
		 avcodec_free_context(&_ctx);
		 return false;
	  }
#endif 

	  if (_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
	  {
		 //_ctx->width = param.width;
		 //_ctx->height = param.height;
		 _ctx->time_base = (AVRational){ 1, param.fps };
		 _ctx->framerate = (AVRational){ param.fps, 1 };
	  }
	  else if(_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
      {
		 if(param.codec == PAVCODEC_PCMA || param.codec == PAVCODEC_PCMU || param.codec == PAVCODEC_AMR) 
		 {
			_ctx->sample_rate = 8000;
			_ctx->channels = 1;
		 } 
		 else if(param.codec == PAVCODEC_AMRWB) 
		 {
			_ctx->sample_rate = 16000;
			_ctx->channels = 1;
		 }
#if 0
		 //resampling
		 _ctxswr = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_S16, 16000, 
							            AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_S16, _ctx->sample_rate, 0, NULL);

		 int ret = swr_init(_ctxswr);
		 if(ret < 0)
		 {
			printf("#FFM->failed to alloc & init swr context!\n");
			return false;
		 }
#endif
	  }

	  /* Open decoder */
	  int ret = avcodec_open2(_ctx, pcodec, NULL);
	  if (ret < 0) {
		 //error log 
		 avcodec_free_context(&_ctx);
		 //av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
		 return false;
	  }


	  _param = param;

	  return true;
   }

   virtual void final()
   {
	  if(_ctx) 
	  {
		 avcodec_free_context(&_ctx);
		 _ctx = NULL;
	  }
   }

   virtual bool put(PAVBuffer & in)
   {
	  AVPacket * pkt = in.packet();
	  if(!_ctx) {
		 //error log 
		 return false;
	  }
	  int ret = avcodec_send_packet(_ctx, pkt);
	  if(ret < 0) 
	  {
		 //error log 
		 return false;
	  }
	  return true;
   }

   virtual bool put()
   {
	  if(!_ctx) {
		 //error log 
		 return false;
	  }
	  int ret = avcodec_send_packet(_ctx, NULL);
	  if(ret < 0) 
	  {
		 //error log 
		 return false;
	  }
	  return true;
   }

   virtual bool get(PAVBuffer & out)
   {
	  if(!_ctx) {
		 //error log 
		 return false;
	  }


	  AVFrame * frame = out.frame();

	  int ret = avcodec_receive_frame(_ctx, frame);
	  if(ret == 0) 
	  {
		 frame->pts = av_frame_get_best_effort_timestamp(frame);

		 out._type = _param.type;
		 return true;
	  }
	  else if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
	  {
		 //done  
	  } 
	  else 
	  {
		 // error log 
	  }
	  return false; //error or EOF
   }
};

class PAVEncoder // for using ffmpeg 
{
protected:
   PAVParam			 _param;
   AVStream			 _stream;
   //AVCodec        * _codec;
   AVCodecContext * _ctx; //


   int			    _got_packet;
   int (*pfcoder)(AVCodecContext *, AVFrame *, int *, const AVPacket *);

   unsigned int _lpts; 

public:
   PAVEncoder() : _ctx(NULL) { }
   virtual ~PAVEncoder() { final(); }

   virtual bool init(PAVParam & param)
   {
	  AVCodec * pcodec = NULL;
	  //pcodec = avcodec_find_encoder_by_name(param.avencoder_name().c_str()); 
	  pcodec = avcodec_find_encoder(param.avcodec());
	  if(!pcodec) 
	  {
		 //error log
		 return false;
	  }

	  _ctx = avcodec_alloc_context3(pcodec);
	  if(!_ctx) 
	  {
		 //error log
		 return false;
	  }

	  //set default codec parameter 
#if 0
	  memset(&_stream, 0, sizeof(AVStream));
	  int ret = avcodec_parameters_to_context(_ctx, _stream.codecpar); 
	  if(ret < 0) 
	  {
		 //error log 
		 avcodec_free_context(&_ctx);
		 return false;
	  }
#endif

	  _ctx->bit_rate = param.bitrate;
	  if(_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
	  {
		 _ctx->width = param.width;
		 _ctx->height = param.height;
		 _ctx->time_base.den = param.fps;
		 _ctx->time_base.num = 1;
		 _ctx->pix_fmt = AV_PIX_FMT_YUV420P;
		 _ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		 
		 _ctx->framerate.num = param.fps;
		 _ctx->framerate.den = 1;
		 _ctx->gop_size = 1;

		 if(param.codec == PAVCODEC_H264) 
		 {
			av_opt_set(_ctx->priv_data, "profile", "baseline", 0);
			av_opt_set(_ctx->priv_data, "level", "3.2", 0);
			av_opt_set(_ctx->priv_data, "preset", "superfast", 0);
			av_opt_set(_ctx->priv_data, "tune", "zerolatency", 0);

			char szParams[256];
			memset(szParams, 0, sizeof(szParams));
			int nVBV = param.bitrate/1024;
			int nthread = 5;
			if(nthread == 0) 
			{
			   //threads  0
			   snprintf(szParams, sizeof(szParams), "repeat-headers=1:keyint=%d:min-keyint=%d:open-gop=0:vbv-maxrate=%d:vbv-bufsize=%d",
					 param.fps, param.fps, nVBV, nVBV);
			}
			else 
			{
			   //threads 5
			   snprintf(szParams, sizeof(szParams), "repeat-headers=1:keyint=%d:min-keyint=%d:open-gop=0:vbv-maxrate=%d:vbv-bufsize=%d:threads=%d",
					 param.fps, param.fps, nVBV, nVBV, nthread);
			   //snprintf(szParams, sizeof(szParams), "repeat-headers=1:keyint=10:min-keyint=10:vbv-maxrate=300:vbv-bufsize=600:threads=1");

			}	

			av_opt_set(_ctx->priv_data, "x264-params", szParams, 0);
		 }
		 //_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
	  } 
	  else if(_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
	  {
		 if(param.codec == PAVCODEC_PCMA || param.codec == PAVCODEC_PCMU || param.codec == PAVCODEC_AMR) 
		 {
			_ctx->sample_rate = 8000;
			_ctx->channels = 1;
			_ctx->sample_fmt = pcodec->sample_fmts[0];
		 } 
		 else if(param.codec == PAVCODEC_AMRWB) 
		 {
			_ctx->sample_rate = 16000;
			_ctx->channels = 1;
			_ctx->sample_fmt = pcodec->sample_fmts[0];
		 }
	  }

	  /* Open decoder */
	  int ret = avcodec_open2(_ctx, pcodec, NULL);
	  if (ret < 0) {
		 //error log 
		 avcodec_free_context(&_ctx);
		 printf("FFM->Failed to open encoder!(ret=%d)\n", ret);
		 return false;
	  }

	  param.extsize = _ctx->extradata_size;
	  if(_ctx->extradata_size > 0 ) 
	  {
		 memcpy(param.extra, _ctx->extradata, _ctx->extradata_size);
	  }
	  _param = param;
	  _lpts = 0;
	  return true;
   }

   virtual void final()
   {
	  if(_ctx) 
	  {
		 avcodec_free_context(&_ctx);
		 _ctx == NULL;
	  }
   }

   virtual bool put(PAVBuffer & in)
   {
	  if(!_ctx) {
		 //error log 
		 return false;
	  }

	  AVFrame * frame = in.frame();

	  
#if 0
	  if(_ctx->codec_id == AV_CODEC_ID_H264)
	  { 
		 FILE *fp = fopen("before.yuv", "ab+");
		 if (fp != NULL)
		 {
			fwrite(frame->data[0], 1, frame->linesize[0]*480, fp);
			fwrite(frame->data[1], 1, frame->linesize[1]*480/2, fp);
			fwrite(frame->data[2], 1, frame->linesize[2]*480/2, fp);
			fclose(fp);
		 }
	  }
#endif 
	  int ret = avcodec_send_frame(_ctx, frame);
	  if(ret < 0) 
	  {
		 //error log 
		 printf("# Encoder->put() : failed to avcodec_send_frame!\n");
		 return false;
	  }
	  return true;
   }

   virtual bool put()
   {
	  if(!_ctx) {
		 //error log 
		 return false;
	  }

	  int ret = avcodec_send_frame(_ctx, NULL);
	  if(ret < 0) 
	  {
		 //error log 
		 printf("# Encoder->put() : failed to avcodec_send_frame!\n");
		 return false;
	  }
	  return true;
   }

   virtual bool get(PAVBuffer & out)
   {
	  if(!_ctx) {
		 //error log 
		 return false;
	  }

	  AVPacket * pkt = out.packet();
	  int ret = avcodec_receive_packet(_ctx, pkt);
	  if(ret == 0) 
	  {
		 out._type = _param.type;
#if 0
		 {
			FILE *fp = fopen("after.h264", "ab+");
			if (fp != NULL)
			{
			   fwrite(pkt->data, 1, pkt->size, fp);
			   fclose(fp);
			}
		 }
#endif
		 return true;
	  }
	  else if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
	  {
		 //done 
	  } 
	  else 
	  {
		 // error log 
	  }
	  return false;
   }
};

#endif //__PAVCodec__
