#ifndef __PAVBase__
#define __PAVBase__

extern "C" {

#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
//#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avfilter.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}


#include "PMPBase.h"

#define	 PAV_VIDEO_WIDTH   (1280)
#define	 PAV_VIDEO_HEIGHT  (720)
#define	 PAV_VIDEO_FPS	   10

inline const char *  pav_decoder_name(PAVCODEC codec) 
{
	  switch(codec) 
	  {
	  case PAVCODEC_PCMA:
		 return "pcm_alaw";
		 break;
	  case PAVCODEC_PCMU:
		 return "pcm_mulaw";
		 break;
	  case PAVCODEC_AMR:
		 return "libopencore_amrnb";
		 //return "amrnb";
		 break;
	  case PAVCODEC_AMRWB:
		 return "libopencore_amrnb";
		 break;
	  case PAVCODEC_H264:
		 //return "libx264";
		 return "h264";
		 break;
	  case PAVCODEC_H265:
	  default: 
		 return NULL; 
		 break;
	  }
	  return NULL;
}

class PAVParam
{
public:
   enum { MAX_VC_EXTRA = 4096 } ;
   PAVCODEC codec;  
   PAVTYPE	type;   // AUDIO or VIDEO
   int bitrate; //audio(voice) & video

   // for audio
   int samplerate; //audio(voice) 
   int channels; //audio(voice) 
   int mode;    //for amr, amr-wb, evs 

   // for video
   int width;   // video only
   int height;  // video only
   int fps;     // video only

   // for rtp
   char			  ip[32];  // rtp ip 
   unsigned short port;  // rtp ip 
   unsigned char  pt1;
   unsigned char  pt2;
   unsigned int	  ssrc;
   char			  cname[32];  // rtp ip 
   unsigned int   timescale;
   //for extra
   char extra[MAX_VC_EXTRA];
   int extsize;

   //for file , encoder, decoder, rtp, ...
   int stream_index; 
   
   PAVParam() {}
   ~PAVParam() {};

   bool init(PAVCODEC codecid = PAVCODEC_NONE) 
   {
	  final();
	  if(codecid == PAVCODEC_NONE) return false;


	  switch(codecid) 
	  {
	  case PAVCODEC_PCMA:	  
	  case PAVCODEC_PCMU:	  
		 type	    = PAVTYPE_AUDIO;
		 bitrate = 64000;
		 samplerate = 8000;
		 timescale  = samplerate;
		 channels=1;
		 break;
	  case PAVCODEC_AMR:	  
		 type	    = PAVTYPE_AUDIO;
		 bitrate = 12800;
		 samplerate = 8000;
		 timescale  = samplerate;
		 channels=1;
		 break;
	  case PAVCODEC_AMRWB:	  
		 type	    = PAVTYPE_AUDIO;
		 bitrate = 24400;
		 samplerate = 16000;
		 timescale  = samplerate;
		 channels=1;
		 break;
	  case PAVCODEC_H264:	  
		 type	    = PAVTYPE_VIDEO;
		 bitrate = 512000;
		 width = PAV_VIDEO_WIDTH;
		 height = PAV_VIDEO_HEIGHT;
		 fps = 15;
		 timescale  = 90000;
		 break;
	  case PAVCODEC_H265:	  
		 type	    = PAVTYPE_VIDEO;
		 bitrate = 1024000;
		 width = PAV_VIDEO_WIDTH;
		 height = PAV_VIDEO_HEIGHT;
		 fps = 30;
		 timescale  = 90000;
		 break;
	  default:
		 return false;
		 break;
	  }
	  codec = codecid;
	  return true;
   }

   std::string  str()
   {
	  return formatStr("Param:[codec:%d, type:%d, bitrate:%d, width:%d, height:%d, fps:%d, channels:%d, "
					   "ip=%s, port=%d, pt1=%d, pt2=%d]", 
						codec, type, bitrate, width, height, fps, channels, 
						ip, port, pt1, pt2);

   }

   bool init(int codecid) 
   {
	  return init(pavcodec(codecid));
   }

   void final() 
   {
	  codec			 = PAVCODEC_NONE;   //CODEC ID
	  type			 = PAVTYPE_NONE;   //Media TYPE 
	  bitrate		 = 0; //audio(voice) & video
	  
	  // for audio
	  samplerate	 = 0; //audio(voice) & video
	  channels		 = 0; //audio(voice) & video
	  mode			 = 0;    //for amr, amr-wb, evs 

	  // for video
	  width			 = 0;   // video only
	  height		 = 0;  // video only
	  fps			 = 0;     // video only

	  // for rtp
	  ip[0]			 = 0;  // rtp ip 
	  port			 = 0;  // rtp ip 
	  pt1			 = 0;
	  pt2			 = 0;
	  ssrc			 = 0;
	  timescale		 = 0;
	  cname[0]		 = 0;  // rtp ip 
	  //for extra
	  extra[0]       = 0;
	  extsize		 = 0;

	  //for file , encoder, decoder, rtp, ...
	  stream_index	 = 0; 
   }
   
   PAVCODEC pavcodec(int codecid) 
   {
	  switch(codecid) 
	  {
		 case AV_CODEC_ID_PCM_ALAW:
			return PAVCODEC_PCMA;
		 break;
		 case AV_CODEC_ID_PCM_MULAW:
			return  PAVCODEC_PCMU;
		 break;
		 case AV_CODEC_ID_AMR_NB:
			return  PAVCODEC_AMR;
		 break;
		 case AV_CODEC_ID_AMR_WB:
			return  PAVCODEC_AMRWB;
		 break;
		 case AV_CODEC_ID_H264:
			return  PAVCODEC_H264;
		 break;
		 case AV_CODEC_ID_H265:
			return  PAVCODEC_H265;
		 break;
		 default: 
			return  PAVCODEC_NONE;
		 break;
	  }
	  return  PAVCODEC_NONE;
   }
   
   AVCodecID avcodec() 
   {
	  switch(codec) 
	  {
		 case PAVCODEC_PCMA:
			return AV_CODEC_ID_PCM_ALAW;
			break;
		 case PAVCODEC_PCMU:
			return AV_CODEC_ID_PCM_MULAW;
			break;
		 case PAVCODEC_AMR:
			return AV_CODEC_ID_AMR_NB;
			break;
		 case PAVCODEC_AMRWB:
			return AV_CODEC_ID_AMR_WB;
			break;
		 case PAVCODEC_H264:
			return AV_CODEC_ID_H264;
			break;
		 case PAVCODEC_H265:
			return AV_CODEC_ID_H265;
			break;
		 default: 
			return AV_CODEC_ID_NONE;
			break;
	  }
	  return AV_CODEC_ID_NONE;
   }

   int avtype()
   {
	  switch(type) 
	  {
	  case PAVTYPE_AUDIO:
		 return AVMEDIA_TYPE_AUDIO;
		 break;
	  case PAVTYPE_VIDEO:
		 return AVMEDIA_TYPE_VIDEO;
		 break;
	  default: 
		 return AVMEDIA_TYPE_UNKNOWN;
		 break;
	  }
	  return AVMEDIA_TYPE_UNKNOWN;
   }

   std::string avencoder_name() 
   {
	  switch(codec) 
	  {
		 case PAVCODEC_PCMA:
			return "pcm_alaw";
			break;
		 case PAVCODEC_PCMU:
			return "pcm_mulaw";
			break;
		 case PAVCODEC_AMR:
			return "libopencore_amrnb";
			//return "amrnb";
			break;
		 case PAVCODEC_AMRWB:
			return "libvo_amrwbenc";
			break;
		 case PAVCODEC_H264:
			return "libx264";
			break;
		 case PAVCODEC_H265:
		 default: 
			return "";
			break;
	  }
	  return "";
   }

   std::string  avdecoder_name() 
   {
	  switch(codec) 
	  {
		 case PAVCODEC_PCMA:
			return "pcm_alaw";
			break;
		 case PAVCODEC_PCMU:
			return "pcm_mulaw";
			break;
		 case PAVCODEC_AMR:
			return "libopencore_amrnb";
			//return "amrnb";
			break;
		 case PAVCODEC_AMRWB:
			return "libopencore_amrnb";
			break;
		 case PAVCODEC_H264:
			//return "libx264";
			return "h264";
			break;
		 case PAVCODEC_H265:
		 default: 
			return ""; 
			break;
	  }
	  return "";
   }

};

#endif // __PAVBase__

