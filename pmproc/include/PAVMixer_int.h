#ifndef __PVideoCodec__
#define __PVideoCodec__


#include "libavutil/avcodec.h"
#include "libavutil/avutil.h"

   /******************************************************************************************
	 +------------------+   +------------------+   +--------+---------+    +--------+---------+
	 | 1                |   | 1                |   |        |    2    |    |   1    |    2    |
	 |                  |   |                  |   +--------|         |    |        |         |
	 |                  |   |         +------+ |   |   1    +---------+    +--------+---------+
	 |                  |   |         |  2   | |   +--------+    3    |    |   3    |    4    |
	 |                  |   |         +------+ |   |        |         |    |        |         |
	 +------------------+   +------------------+   +--------+---------+    +--------+---------+

	 +--------+---------+   +------------+-----+   +-----+-----+------+    +-----+-----+------+
	 |   1    |    2    |   |            |  2  |   |  1  |  3  |  6   |    |  1  |  3  |   6  |
	 |        |         |   |     1      +-----+   +     +-----+      +    +     +-----+------+
	 +-----+------+-----+   |            |  3  |   |-----|  4  |------|    |-----|  4  |   7  |
	 |   3 |  4   |  5  |   +-----+------+-----+   +     +-----+      +    +     +-----+------+
	 |     |      |     |   |   4 |  5   |  6  |   |  2  |  5  |  7   |    |  2  |  5  |   8  |
	 +-----+------+-----+   +-----+------+-----+   +-----+-----+------+    +-----+-----+------+

	 ... (MAX:16)
	******************************************************************************************/

#define FFM_RESIZE_FILTER 1

#ifdef FFM_RESIZE_FILTER
   extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
	  //#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "ffmpeg_config.h"
#include "libavutil/avutil.h"
#include "libavutil/dict.h"
#include "libavutil/eval.h"
#include "libavutil/fifo.h"
#include "libavutil/hwcontext.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"
#include "libavutil/threadmessage.h"
#include "libswresample/swresample.h"
   }
#endif




typedef struct {
   int x; // percent
   int y; // percent
   int width; // percent
   int height; // percent
} MixPos;

static MixPos MixPosInfo1[] = {
   {  0,  0,100,100}
};

static MixPos MixPosInfo2[] = {
   {  0,  0,100,100},
   {  55, 55, 40, 40}
};

static MixPos MixPosInfo3[] = {
   {  0, 25, 50, 50}, { 50,  0, 50, 50},
   { 50, 50, 50, 50}
};

static MixPos MixPosInfo4[] = {
   {  0,  0, 50, 50}, { 50,  0, 50, 50},
   {  0, 50, 50, 50}, { 50, 50, 50, 50}
};

static MixPos MixPosInfo5[] = {
   {  0,  0, 50, 50}, { 50,  0, 50, 50},
   {  0, 50, 33, 50}, { 33, 50, 34, 50}, { 67, 50, 33, 50}
};

static MixPos MixPosInfo6[] = {
   {  0,  0, 66, 66},           { 66,  0, 34, 33},
   { 66, 33, 34, 33},
   {  0, 66, 33, 34}, { 33, 66, 33, 34}, { 66, 66, 34, 34}
};

static MixPos MixPosInfo7[] = {
   {  0,  0, 33, 50},
   {  0, 50, 33, 50},                       /* ▒▒ */
   { 67,  0, 33, 50},
   /* ▒▒ */      { 67, 50, 33, 50},
   { 33,  0, 34, 33},
   { 33, 33, 34, 34},
   { 33, 67, 34, 33}
};




static MixPos MixPosInfo8[] = {
   {  0,  0, 34, 50},
   {  0, 50, 34, 50},               /* ▒▒ */
   { 34,  0, 33, 34}, { 67,  0, 33, 34},
   { 34, 34, 33, 33}, { 67, 34, 33, 33},
   { 34, 67, 33, 33}, { 67, 67, 33, 33},
};

static MixPos MixPosInfo9[] = {
   {  0,  0, 34, 34}, { 34,  0, 33, 34}, { 67,  0, 33, 34},
   {  0, 34, 34, 33}, { 34, 34, 33, 33}, { 67, 34, 33, 33},
   {  0, 67, 34, 33}, { 34, 67, 33, 33}, { 67, 67, 33, 33}
};


class CVideoMixFilter : public UMC::BaseCodec
{
   DYNAMIC_CAST_DECL(CVideoMixFilter, BaseCodec)
   public:
	  enum {MAX_POSITIONS = 16};
   public:
	  CVideoMixFilter();
	  ~CVideoMixFilter();

	  UMC::Status UpdateFrame(int nChannelId, int nPosition, UMC::MediaData *in); // nPosition=0,1,2,3

	  UMC::Status Init(UMC::BaseCodecParams *info);
	  // ChannelIDs[] : ChannelIDs to be located at each position.
	  UMC::Status RelocateChannels(bool bOn, int *pChannelIDs, int nChanCount);
	  UMC::Status GetFrame(UMC::MediaData *in, UMC::MediaData *out);
	  UMC::Status GetInfo(UMC::BaseCodecParams *) { return UMC::UMC_OK; };
	  UMC::Status Close(void) { return UMC::UMC_OK; };
	  UMC::Status Reset(void) { return UMC::UMC_OK; };

   protected:
	  UMC::Status CopyImage(UMC::VideoData *in, UMC::VideoData *out, int posX, int posY);

   protected:
	  typedef struct {
		 UMC::VideoData* vi;
		 int id;
		 vm_tick tm;
	  } TFrameInfo;

#ifdef FFM_RESIZE_FILTER
	  typedef struct FFMFilter{
		 int bInit;
		 int nWidth;
		 int nHeight;
		 int nScale_w;
		 int nScale_h;
		 AVFrame* m_pInFrame;
		 AVFrame* m_pOutFrame;
		 AVFilterGraph * m_pFilterGraph;
		 AVFilterContext* m_pFilterContext;
		 AVFilterContext* m_pFilterContext_buffersrc;
		 AVFilterContext* m_pFilterContext_buffersink;
		 AVFilter* m_pFiler_buffersrc;
		 AVFilter* m_pFilter_buffersink;
		 unsigned int m_nPts;
	  };

	  FFMFilter* m_pFFMFilter;

#else
	  UMC::VideoProcessing      *m_pFilter;
	  UMC::VideoProcessingParams m_FilterParams;
#endif

	  UMC::VideoData m_DataRGB;
	  unsigned char *m_pBufferRGB;
	  int            m_nSizeRGB;

	  static vm_tick  m_nTickTolerance;
	  TFrameInfo m_SrcInfos[MAX_POSITIONS];
	  TFrameInfo m_DispInfos[MAX_POSITIONS];

	  class ClipCache {
		 protected:
			typedef struct {
			   int id;
			   int width;
			   int height;
			   UMC::ColorFormat color;
			   vm_tick tm;
			   unsigned char* data;
			} TClipInfo;
			std::list <TClipInfo*> m_List;

		 public:
			ClipCache();
			~ClipCache();
			void Clear();
			unsigned char* Add(int id, int width, int height, UMC::ColorFormat color, int nDataSize);
			unsigned char* Get(int id, int width, int height, UMC::ColorFormat color);
			void Delete(int id);
	  };

	  ClipCache m_ClipCache;
	  unsigned int m_nChanCount;

};


/////////////////////////////////////////////////////////////////////
CVideoMixFilter::ClipCache::ClipCache()
{
}

CVideoMixFilter::ClipCache::~ClipCache()
{
   Clear();
}

void CVideoMixFilter::ClipCache::Clear()
{

   TClipInfo *pClip = NULL;
   std::list <TClipInfo*>::iterator it;
   for(it = m_List.begin(); it != m_List.end(); it++) {
	  pClip = (TClipInfo*)*it;
	  delete[] pClip->data;
	  delete pClip;
   }
   m_List.clear();

}


VideoMixFilter::ClipCache::Clear()
{

   TClipInfo *pClip = NULL;
   std::list <TClipInfo*>::iterator it;
   for(it = m_List.begin(); it != m_List.end(); it++) {
	  pClip = (TClipInfo*)*it;
	  delete[] pClip->data;
	  delete pClip;
   }
   m_List.clear();

}

unsigned char* CVideoMixFilter::ClipCache::Add(int id, int width, int height, UMC::ColorFormat color, int nDataSize)
{
   TClipInfo *pClip = new TClipInfo;
   pClip->id     = id;
   pClip->width  = width;
   pClip->height = height;
   pClip->color  = color;
   pClip->tm     = vm_time_get_tick();
   pClip->data   = new unsigned char[nDataSize];
   m_List.insert(m_List.end(), pClip);

   return pClip->data;
}

unsigned char* CVideoMixFilter::ClipCache::Get(int id, int width, int height, UMC::ColorFormat color)
{
   TClipInfo *pClip = NULL;
   std::list <TClipInfo*>::iterator it;
   for(it = m_List.begin(); it != m_List.end(); it++) {
	  pClip = (TClipInfo*)*it;
	  if((pClip->id     == id    ) &&
			(pClip->width  == width ) &&
			(pClip->height == height) &&
			(pClip->color  == color )) {
		 //printf("GET:%d,%d,%d,%d-FOUND\n", id, width, height, color);
		 return pClip->data;
	  }
   }
   return NULL;
}

void CVideoMixFilter::ClipCache::Delete(int id)
{
   TClipInfo *pClip = NULL;
   std::list <TClipInfo*>::iterator it;
   bool bFound;
   do {
	  bFound = false;
	  for(it = m_List.begin(); it != m_List.end(); it++) {
		 pClip = (TClipInfo*)*it;
		 if(pClip && pClip->id == id) {
			bFound = true;
			delete[] pClip->data;
			pClip->data = NULL;
			delete pClip;
			pClip = NULL;
			//m_List.erase(it); //fixed by redrock 2012-01-09
			it = m_List.erase(it)--;
			//break; // list-chain changed, restart searching.
		 }
	  }
   } while(bFound);
}


vm_tick CVideoMixFilter::m_nTickTolerance = vm_time_get_frequency() / 30;


CVideoMixFilter::CVideoMixFilter() {
#ifdef FFM_RESIZE_FILTER
   av_register_all();
   avfilter_register_all();
   m_pFFMFilter = new FFMFilter[MAX_POSITIONS];
   for(int i=0;i<MAX_POSITIONS;i++)
   {
	  memset(&m_pFFMFilter[i],0x00,sizeof(FFMFilter));
   }
#else
   m_FilterParams.InterpolationMethod   = IPPI_INTER_LANCZOS;
   m_FilterParams.m_DeinterlacingMethod = NO_DEINTERLACING;
   m_FilterParams.numThreads = 1;
   m_pFilter = new UMC::VideoProcessing;
   m_pFilter->Init(&m_FilterParams);
#endif
}

CVideoMixFilter::~CVideoMixFilter() {
#ifdef FFM_RESIZE_FILTER
   for(int i=0;i<MAX_POSITIONS;i++)
   {
	  avfilter_graph_free(&m_pFFMFilter[i].m_pFilterGraph);
	  av_frame_free(&m_pFFMFilter[i].m_pInFrame);
	  av_frame_free(&m_pFFMFilter[i].m_pOutFrame);
   }
   delete [] m_pFFMFilter;
#else
   if(m_pFilter) delete m_pFilter;
#endif
}

UMC::Status CVideoMixFilter::Init(BaseCodecParams *info)
{
   //fprintf(stderr,"%s %s %d\n",__FILE__,__func__,__LINE__);
   int i;
   for(i=0;i<MAX_POSITIONS;i++) {
	  m_SrcInfos[i].vi = NULL; // top-left, top-right, bottom-left, bottom-right.
	  m_SrcInfos[i].id = -1;
   }
   for(i=0;i<MAX_POSITIONS;i++) {
	  m_DispInfos[i].vi = NULL;
	  m_DispInfos[i].id = -1;
   }
   m_ClipCache.Clear();
   m_nChanCount = 0;
   fprintf(stderr,"%s %s %d\n",__FILE__,__func__,__LINE__);
#ifdef FFM_RESIZE_FILTER
   for(int i=0;i<MAX_POSITIONS;i++)
   {
	  m_pFFMFilter[i].m_pInFrame = av_frame_alloc();
	  m_pFFMFilter[i].m_pOutFrame = av_frame_alloc();
	  if(!m_pFFMFilter[i].m_pInFrame || !m_pFFMFilter[i].m_pOutFrame)
	  {
		 fprintf(stderr,"av_frame_alloc failed\n");
		 return UMC::UMC_ERR_FAILED;
	  }
	  memset(m_pFFMFilter[i].m_pInFrame,0x00,sizeof(AVFrame));
	  memset(m_pFFMFilter[i].m_pInFrame,0x00,sizeof(AVFrame));
	  m_pFFMFilter[i].m_nPts=1;
   }
#endif
   return UMC::UMC_OK;
}


// nPosition=0,1,2,...
UMC::Status CVideoMixFilter::UpdateFrame(int nChannelId, int nPosition, MediaData *in)
{
   //fprintf(stderr,"%s %s %d m_nChanCount:%d ch:%d pos:%d\n",__FILE__,__func__,__LINE__,m_nChanCount,nChannelId,nPosition);
   if(nPosition < 0 || nPosition >= MAX_POSITIONS) {
	  return UMC::UMC_ERR_FAILED;
   }
   m_SrcInfos[nPosition].vi = DynamicCast<UMC::VideoData>(in);
   m_SrcInfos[nPosition].tm = vm_time_get_tick();
   if(in != NULL) {
	  m_SrcInfos[nPosition].id = nChannelId;
#ifdef FFM_RESIZE_FILTER
	  if(m_pFFMFilter[nPosition].bInit == 0)
	  {
		 int ret;
		 char filter_descr[128];
		 MixPos *pMixPos = NULL;
		 switch(m_nChanCount) {
			case 0: pMixPos = MixPosInfo1; break;
			case 1: pMixPos = MixPosInfo1; break;
			case 2: pMixPos = MixPosInfo2; break;
			case 3: pMixPos = MixPosInfo3; break;
			case 4: pMixPos = MixPosInfo4; break;
			case 5: pMixPos = MixPosInfo5; break;
			case 6: pMixPos = MixPosInfo6; break;
			case 7: pMixPos = MixPosInfo7; break;
			case 8: pMixPos = MixPosInfo8; break;
			case 9: pMixPos = MixPosInfo9; break;
			default:pMixPos = MixPosInfo9; break;
		 }
		 m_pFFMFilter[nPosition].nScale_w = m_SrcInfos[nPosition].vi->GetWidth() * pMixPos[nPosition].width / 100;
		 m_pFFMFilter[nPosition].nScale_h = m_SrcInfos[nPosition].vi->GetHeight() * pMixPos[nPosition].height / 100;
		 snprintf(filter_descr,128,"scale=%d:%d",m_pFFMFilter[nPosition].nScale_w,m_pFFMFilter[nPosition].nScale_h);
		 //fprintf(stderr,"filter_descr = %s\n",filter_descr);
		 enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
		 AVFilter* pFilter_buffersrc = avfilter_get_by_name("buffer");
		 AVFilter* pFilter_buffersink = avfilter_get_by_name("buffersink");
		 AVFilterInOut * pFilter_out = avfilter_inout_alloc();
		 AVFilterInOut * pFilter_in = avfilter_inout_alloc();
		 m_pFFMFilter[nPosition].m_pFilterGraph = avfilter_graph_alloc();
		 if(!pFilter_buffersrc || !pFilter_buffersink || !pFilter_out || !pFilter_in || !m_pFFMFilter[nPosition].m_pFilterGraph)
		 {
			//fprintf(stderr,"filter memory alloc failed %p, %p, %p, %p, %p\n",m_pFilter_buffersrc,m_pFilter_buffersink,m_pFilter_out,m_pFilter_in,m_pFilterGraph);
			return UMC::UMC_ERR_FAILED;
		 }


		 char args[512];
		 snprintf(args,sizeof(args),"video_size=%dx%d:pix_fmt=0:time_base=1/20000000:pixel_aspect=0/1:sws_param=flags=2:frame_rate=353/12",m_SrcInfos[nPosition].vi->GetWidth(),m_SrcInfos[nPosition].vi->GetHeight());
		 //snprintf(args,sizeof(args),"video_size=%dx%d:pix_fmt=%d",288,352,AV_PIX_FMT_YUV420P);
		 if(avfilter_graph_create_filter(&m_pFFMFilter[nPosition].m_pFilterContext_buffersrc,pFilter_buffersrc,"in",args,NULL,
				  m_pFFMFilter[nPosition].m_pFilterGraph) < 0)
		 {
			fprintf(stderr,"avfilter_graph_create_filter failed\n");
			return UMC::UMC_ERR_FAILED;
		 }

		 if(avfilter_graph_create_filter(&m_pFFMFilter[nPosition].m_pFilterContext_buffersink,pFilter_buffersink,"out",NULL,NULL,
				  m_pFFMFilter[nPosition].m_pFilterGraph) < 0)
		 {
			fprintf(stderr,"avfilter_graph_create_filter failed\n");
			return UMC::UMC_ERR_FAILED;
		 }
		 if(av_opt_set_int_list(m_pFFMFilter[nPosition].m_pFilterContext_buffersink, "pix_fmts",pix_fmts, AV_PIX_FMT_YUV420P, AV_OPT_SEARCH_CHILDREN)<0)
		 {
			fprintf(stderr,"av_opt_set_int_list failed\n");
			return UMC::UMC_ERR_FAILED;
		 }

		 pFilter_out->name = av_strdup("in");
		 pFilter_out->filter_ctx = m_pFFMFilter[nPosition].m_pFilterContext_buffersrc;
		 pFilter_out->pad_idx = 0;
		 pFilter_out->next = NULL;

		 pFilter_in->name = av_strdup("out");
		 pFilter_in->filter_ctx = m_pFFMFilter[nPosition].m_pFilterContext_buffersink;
		 pFilter_in->pad_idx = 0;
		 pFilter_in->next = NULL;

		 if ((ret = avfilter_graph_parse_ptr(m_pFFMFilter[nPosition].m_pFilterGraph, filter_descr,
					 &pFilter_in, &pFilter_out, NULL)) < 0)
		 {
			fprintf(stderr,"avfilter_graph_parse_ptr failed\n");
			return UMC::UMC_ERR_FAILED;

		 }

		 if ((ret = avfilter_graph_config(m_pFFMFilter[nPosition].m_pFilterGraph, NULL)) < 0)
		 {
			fprintf(stderr,"avfilter_graph_config failed\n");
			return UMC::UMC_ERR_FAILED;

		 }
		 avfilter_inout_free(&pFilter_in);
		 avfilter_inout_free(&pFilter_out);
		 m_pFFMFilter[nPosition].bInit = 1;
	  }
#endif
   } else {
	  m_SrcInfos[nPosition].id = -1;
   }
   m_DispInfos[nPosition] = m_SrcInfos[nPosition];
   m_ClipCache.Delete(nChannelId);
   return UMC::UMC_OK;
}



UMC::Status CVideoMixFilter::RelocateChannels(bool bOn, int *pChannelIDs, int nChanCount)
{
   //fprintf(stderr,"%s %s %d on:%d ch:%d cnt:%d\n",__FILE__,__func__,__LINE__,bOn,pChannelIDs[0],nChanCount);
   int i,j;
   if(bOn) {
	  if(nChanCount > MAX_POSITIONS) {
		 nChanCount = MAX_POSITIONS;
	  }
	  for(i=0;i<MAX_POSITIONS;i++) {
		 m_DispInfos[i].vi = NULL;
		 if(i < nChanCount) {
			for(j=0;j<MAX_POSITIONS;j++) {
			   if(pChannelIDs[i] == m_SrcInfos[j].id) {
				  m_DispInfos[i] = m_SrcInfos[j];
				  break;
			   }
			}
		 }
	  }
	  m_nChanCount = nChanCount;
   } else {
	  for(i=0;i<MAX_POSITIONS;i++) {
		 m_DispInfos[i] = m_SrcInfos[i];
	  }
	  m_nChanCount = MAX_POSITIONS;
   }
   //fprintf(stderr,"%s %s %d ch:%d\n",__FILE__,__func__,__LINE__,m_nChanCount);
   return UMC::UMC_OK;
}

UMC::Status CVideoMixFilter::GetFrame(MediaData *in, MediaData *out)
{
   UMC::Status rc = UMC::UMC_OK;
   VideoData  ViTmp;
   VideoData* pViOut = DynamicCast<VideoData>(out);
   unsigned char* pBuf = NULL;
   int nWidth = pViOut->GetWidth();
   int nHeight = pViOut->GetHeight();
   int i, nWidthTo, nHeightTo;
   int nCount = 0;
   int nMemSize;

#if 0
   for(i=MAX_POSITIONS-1;i>=0;i--) {
	  if(m_DispInfos[i].vi != NULL) {
		 nCount = i+1;
		 break;
	  }
   }
#else
   nCount = m_nChanCount;
#endif

   MixPos *pMixPos = NULL;
   switch(nCount) {
	  case 0: pMixPos = MixPosInfo1; break;
	  case 1: pMixPos = MixPosInfo1; break;
	  case 2: pMixPos = MixPosInfo2; break;
	  case 3: pMixPos = MixPosInfo3; break;
	  case 4: pMixPos = MixPosInfo4; break;
	  case 5: pMixPos = MixPosInfo5; break;
	  case 6: pMixPos = MixPosInfo6; break;
	  case 7: pMixPos = MixPosInfo7; break;
	  case 8: pMixPos = MixPosInfo8; break;
	  case 9: pMixPos = MixPosInfo9; break;
	  default:pMixPos = MixPosInfo9; break;
   }

   UMC::ColorFormat nColorTo = pViOut->GetColorFormat();
#ifdef FFM_RESIZE_FILTER
   UMC::VideoData::PlaneInfo info;
   int ret,hi,wi;
#endif

   memset(pViOut->GetBufferPointer(), 0, pViOut->GetMappingSize());
   if(pMixPos != NULL) {
	  for(i=0;i<nCount;i++) {
		 if(m_DispInfos[i].vi != NULL) {
			nWidthTo = nWidth * pMixPos[i].width / 100;
			nHeightTo = nHeight * pMixPos[i].height / 100;
			pBuf = m_ClipCache.Get(m_DispInfos[i].id, nWidthTo, nHeightTo, nColorTo);
			ViTmp.Init(nWidthTo, nHeightTo, nColorTo);
			nMemSize = ViTmp.GetMappingSize();
			if(pBuf == NULL) {
			   pBuf = m_ClipCache.Add(m_DispInfos[i].id, nWidthTo, nHeightTo, nColorTo, nMemSize);
			   ViTmp.SetBufferPointer(pBuf, nMemSize);
#ifdef FFM_RESIZE_FILTER
			   ret = avpicture_fill((AVPicture*)m_pFFMFilter[i].m_pInFrame,(uint8_t*)m_DispInfos[i].vi->GetDataPointer(), AV_PIX_FMT_YUVJ420P,
					 m_DispInfos[i].vi->GetWidth(),m_DispInfos[i].vi->GetHeight());
			   //fprintf(stderr,"avpicture_fill ret=%d l:%d,%d,%d w:%d h:%d col:%d\n",ret,m_pInFrame->linesize[0],m_pInFrame->linesize[1],m_pInFrame->linesize[2],m_pInFrame->width,m_pInFrame->height,m_DispInfos[i].vi->GetColorFormat());

			   m_pFFMFilter[i].m_pInFrame->width = m_DispInfos[i].vi->GetWidth();
			   m_pFFMFilter[i].m_pInFrame->height = m_DispInfos[i].vi->GetHeight();
			   m_pFFMFilter[i].m_pInFrame->format = AV_PIX_FMT_YUV420P;
			   m_pFFMFilter[i].m_pInFrame->key_frame = 1;

			   m_pFFMFilter[i].m_pInFrame->pts = m_pFFMFilter[i].m_nPts++;
			   if(av_buffersrc_add_frame_flags(m_pFFMFilter[i].m_pFilterContext_buffersrc,m_pFFMFilter[i].m_pInFrame, AV_BUFFERSRC_FLAG_KEEP_REF)>=0)
			   {
				  //while(1)
				  {
					 ret = av_buffersink_get_frame(m_pFFMFilter[i].m_pFilterContext_buffersink,m_pFFMFilter[i].m_pOutFrame);
					 if(ret < 0)
					 {
						fprintf(stderr,"av_buffersink_get_frame failed ret=%d\n",ret);
						return UMC::UMC_ERR_FAILED;
					 }
				  }
				  //fprintf(stderr,"pitch [0] %d,%d [1] %d,%d [2] %d,%d viwh %d,%d\n",ViTmp.GetPlanePitch(0),m_pOutFrame->linesize[0],ViTmp.GetPlanePitch(1),m_pOutFrame->linesize[1],ViTmp.GetPlanePitch(2),m_pOutFrame->linesize[2],ViTmp.GetWidth(),ViTmp.GetHeight());
				  if((ViTmp.GetPlanePitch(0) == m_pFFMFilter[i].m_pOutFrame->linesize[0]) &&
						(ViTmp.GetPlanePitch(1) == m_pFFMFilter[i].m_pOutFrame->linesize[1]) &&
						(ViTmp.GetPlanePitch(2) == m_pFFMFilter[i].m_pOutFrame->linesize[2])) {
					 for(int n=0;n<3;n++) {
						ViTmp.GetPlaneInfo(&info, n);
						wi = ViTmp.GetWidth() * info.m_iSamples / info.m_iWidthDiv;
						hi = ViTmp.GetHeight() * info.m_iSamples / info.m_iHeightDiv;
						memcpy(ViTmp.GetPlanePointer(n), m_pFFMFilter[i].m_pOutFrame->data[n], wi*hi);
					 }
				  } else {
					 int n1, n2, hi, wi;
					 unsigned char *p1, *p2;

					 for(int n=0;n<3;n++) {
						ViTmp.GetPlaneInfo(&info, n);
						wi = ViTmp.GetWidth() * info.m_iSamples / info.m_iWidthDiv;
						hi = ViTmp.GetHeight() * info.m_iSamples / info.m_iHeightDiv;
						p1 = m_pFFMFilter[i].m_pOutFrame->data[n];
						n1 = m_pFFMFilter[i].m_pOutFrame->linesize[n];
						p2 = (unsigned char*)ViTmp.GetPlanePointer(n);
						n2 = ViTmp.GetPlanePitch(n);
						for(int i=0; i<hi; i++) {
						   memcpy(p2, p1, wi);
						   p1 += n1;
						   p2 += n2;
						}
					 }
				  }
			   }//if
			   else
			   {
				  fprintf(stderr,"av_buffersink_add_frame failed\n");
			   }
#else
			   m_pFilter->GetFrame(m_DispInfos[i].vi, &ViTmp);
#endif
			} else {
			   ViTmp.SetBufferPointer(pBuf, nMemSize);
			}
			rc = CopyImage(&ViTmp, pViOut, nWidth * pMixPos[i].x / 100, nHeight * pMixPos[i].y / 100);
		 }
		 if(rc != UMC::UMC_OK) break;
	  }
   }
   return rc;
}

UMC::Status CVideoMixFilter::CopyImage(VideoData *in, VideoData *out, int posX, int posY)
{
   int nPlanes = out->GetNumPlanes();
   int x0; // Horizontal position in pixel.
   int y0; // Vertical position in pixel.
   UMC::VideoData::PlaneInfo info1, info2;
   unsigned char *p1, *p2;
   int h1, h2, w1, w2;
   int n,y, nPitch1, nPitch2, nCpWidth, nCpHeight;

   if(posX < 0) return UMC_ERR_INVALID_PARAMS;
   if(posY < 0) return UMC_ERR_INVALID_PARAMS;
   if(posX >= out->GetWidth()) return UMC_ERR_INVALID_PARAMS;
   if(posY >= out->GetHeight()) return UMC_ERR_INVALID_PARAMS;

   for(n=0;n<nPlanes;n++) {
	  in->GetPlaneInfo(&info1, n);
	  w1 = in->GetWidth() * info1.m_iSamples / info1.m_iWidthDiv;
	  h1 = in->GetHeight() * info1.m_iSamples / info1.m_iHeightDiv;

	  out->GetPlaneInfo(&info2, n);
	  w2 = out->GetWidth() * info2.m_iSamples / info2.m_iWidthDiv;
	  h2 = out->GetHeight() * info2.m_iSamples / info2.m_iHeightDiv;
	  x0 = posX * info2.m_iSamples / info2.m_iWidthDiv;
	  y0 = posY * info2.m_iSamples / info2.m_iHeightDiv;

	  if(x0 + w1 > w2) {
		 nCpWidth = w2 - x0;
	  } else {
		 nCpWidth = w1;
	  }
	  if(y0 + h1 > h2) {
		 nCpHeight = h2 - y0;
	  } else {
		 nCpHeight = h1;
	  }

	  nPitch2 = out->GetPlanePitch(n);
	  p2 = (unsigned char *)out->GetPlanePointer(n) + nPitch2 * y0 + x0;

	  nPitch1 = in->GetPlanePitch(n);
	  p1 = (unsigned char *)in->GetPlanePointer(n);
	  for(y=0;y<nCpHeight;y++) {
		 memcpy(p2, p1, nCpWidth);
		 p1 += nPitch1;
		 p2 += nPitch2;
	  }
   }
   return UMC::UMC_OK;
}



