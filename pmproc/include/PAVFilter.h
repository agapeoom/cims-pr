#ifndef __PAVFilter__
#define __PAVFilter__

#include "PAVCodec.h"
#include "PAVBuffer.h"

class PAVResizeFilter
{
private:
   struct SwsContext *_ctx;

   int _iwidth;
   int _iheight;
   int _owidth;
   int _oheight;

   PAVBuffer _tbuf;

public:
   PAVResizeFilter() : _ctx(NULL) {};
   ~PAVResizeFilter() {};
   
   bool init(int iwidth, int iheight, int owidth, int oheight)
   {
	  _ctx = sws_getContext(iwidth, iheight, AV_PIX_FMT_YUV420P, owidth, oheight,  AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	  if(!_ctx) 
	  {
		 LOG(LERR, "# failed to sws_getContext(width:%d->%d, height:%d->%d) \n", iwidth, owidth, iheight, oheight);
		 return false;
	  }

	  _tbuf.init();

	  _init_frame(_tbuf, _owidth, _oheight);
	  
	  _iwidth = iwidth;
	  _iheight = iheight;
	  _owidth = owidth;
	  _oheight = oheight;
	  return true;
   }

   void final()
   {

   }

   bool _init_frame(PAVBuffer & in, int width, int height)
   {
	  AVFrame * frame = in.frame();

	  av_frame_unref(frame);

	  frame->format = AV_PIX_FMT_YUV420P;
	  frame->width  = width;
	  frame->height = height;
	  int ret = av_image_alloc(frame->data, frame->linesize, frame->width, frame->height, frame->format, 32);
	  if (ret < 0) {
		 fprintf(stderr, "failed to av_image_alloc()!\n");
		 return false;
	  }

	  memset(frame->data[0], 16, frame->linesize[0]*height);
	  memset(frame->data[1], 128, frame->linesize[1]*(height>>1));
	  memset(frame->data[2], 129, frame->linesize[2]*(height>>1));
	  return true;
   }


   bool put(PAVBuffer & in)
   {
	  AVFrame * src = in.frame();
	  AVFrame * dst = _tbuf.frame();

	  dst->pts = src->pts;
	  int ret = sws_scale(_ctx, src->data, src->linesize, 0, _iheight, dst->data, dst->linesize);
	  if(ret < 0) 
	  {
		 LOG(LERR, "# failed to sws_scale(width:%d->%d, height:%d->%d) \n", _iwidth, _owidth, _iheight, _oheight);
		 return false;
	  }
   }

   bool get(PAVBuffer & out) 
   {
	  AVFrame * src = _tbuf.frame();
	  AVFrame * dst = out.frame();

	  av_frame_ref(dst, src);
	  dst->pts = src->pts;

	  return true;
   }

};

class PAResampler
{
private:
   SwrContext     * _ctxswr;
   unsigned short    _data[2048];
   unsigned int	     _samples;
   unsigned int	     _osamples;
   unsigned int		 _ihz;
   unsigned int		 _ohz;
   unsigned int		 _ich;
   unsigned int		 _och;
   AVSampleFormat	 _ifmt;
   AVSampleFormat	 _ofmt;

   unsigned int		 _pts;

public:
   PAResampler() : _ctxswr(NULL) {};
   ~PAResampler() {};

   bool init(int ihz, int ich, int ohz, int och)
   {
	  _ihz = ihz;
	  _ohz = ohz;
	  _ich = ich;
	  _och = och;

	  _ifmt = AV_SAMPLE_FMT_S16;
	  _ofmt = AV_SAMPLE_FMT_S16;
	   
	  _samples = 0;
	  _pts = 0; 
	  _osamples = _ohz / 50; // if 1channel 

	  _ctxswr = swr_alloc_set_opts(NULL, av_get_default_channel_layout(_och), _ofmt, _ohz,
	                              av_get_default_channel_layout(_ich), _ofmt, _ihz, 0, NULL);
	  
	  int ret = swr_init(_ctxswr);
	  if(ret < 0)
	  {
		 printf("# %s(%d):%s : failed to alloc & init swr context!\n", __FILE__, __LINE__, __FUNCTION__);
		 return false;
	  }

	  return true;
   }

   bool put(PAVBuffer & in)
   {
	  AVFrame * frame = in.frame();
	  int ihz = frame->sample_rate;
	  int samples = frame->nb_samples;
     
	  if(ihz != _ihz) 
	  {
		 printf("# %s(%d):%s : invalid sample rate(%d/%d)!\n", __FILE__, __LINE__, __FUNCTION__, ihz, _ihz);
		 return false;
	  }
	  unsigned short * ptr = &_data[_samples];

	  int osamples = av_rescale_rnd(swr_get_delay(_ctxswr, _ihz) + samples, _ohz, _ihz, AV_ROUND_UP);
	  //int out_samples = swr_convert(_ctxswr, (unsigned char **)&ptr, _osamples, (const unsigned char **)frame->data, samples);
	  int out_samples = swr_convert(_ctxswr, (unsigned char **)&ptr, osamples, (const unsigned char **)frame->data, samples);

	  if(out_samples < 0)  
	  {
		 printf("# %s(%d):%s : faled to convert -- invalid samples(out=%d/in=%d)!\n", __FILE__, __LINE__, __FUNCTION__, out_samples, samples);
		 return false;
	  }
#if 0
	  if(out_samples != _osamples)  
	  {
		 printf("# %s(%d):%s : not equal samples(out=%d/in=%d)!\n", __FILE__, __LINE__, __FUNCTION__, out_samples, samples);
		 return false;
	  }
#endif 
	  _samples += out_samples;
	  return true;
   }

   bool get(PAVBuffer & out)
   {
	  if(_samples>0) 
	  {
		 bool bres = out.set_frame_audio(_ohz, _och, (unsigned char *)_data, _samples, _pts);
		 if(!bres)  
		 {
			printf("# %s(%d):%s : faile to set audio frame!(_samples=%d)\n", __FILE__, __LINE__, __FUNCTION__, _samples);
			return false;
		 }
		 _pts += _samples;
		 _samples = 0;
		 return true;
	  }
	  return false;
   }
};

class PAMixFilter 
{
private:
   unsigned int _group;

public:
   PAMixFilter();
   ~PAMixFilter();

   //bool init(unsigned int gourp, int samplerate=16000, int channels=1);
   bool init(unsigned int gourp);
   void final();
   
   bool put(PAVBuffer & in);
   bool get(PAVBuffer & out);
};

class PVMixFilter 
{
private:
   unsigned int _group;

public:
   PVMixFilter();
   ~PVMixFilter();

   bool init(unsigned int gourp, int priority, const PAVParam & param);
   void final();
   
   bool put(PAVBuffer & in);
   bool get(PAVBuffer & out);
};

#endif 
