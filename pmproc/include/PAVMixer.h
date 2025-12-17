#ifndef __PAVMixer__
#define __PAVMixer__

#define NUM_MIX_DIV1 4
#define NUM_MIX_DIV2 4
#define  MAX_SES_PER_GROUP (NUM_MIX_DIV1 * NUM_MIX_DIV2)


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

// ffmpeg -i input.mp4 -i image.png -filter_complex "[0:v][1:v] overlay=25:25:enable='between(t,0,20)'" -pix_fmt yuv420p -c:a copy output.mp4
// ffmpeg -i input -i logo1 -i logo2 -filter_complex 'overlay=x=10:y=H-h-10,overlay=x=W-w-10:y=H-h-10' output 


struct PAVRect
{
   int x; // percent
   int y; // percent
   int w; // percent
   int h; // percent
};


class PVMixFilter
{
public:
   PAVRect _ratio;

   int _px; 
   int _py; 
   int _width;
   int _height; 
   
   AVFilterContext * _buffersrc_ctx = NULL;
   AVFilterContext * _buffersink_ctx = NULL;
   AVFilterGraph *   _filter_graph = avfilter_graph_alloc();

   void init(PAVRect &ratio, int width, int height)
   {
	  _ratio = ratio;

	  _px = _ratio.x * width / 100;
	  _py = _ratio.y * height / 100;

	  _width =  _ratio.w * width / 100;
	  _heigth = _ratio.h * height / 100;

	  int ret = 0;
	  char args[512];
	  enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
	  
	  _buffersrc_ctx = NULL;
	  _buffersink_ctx = NULL;
	  _filter_graph = avfilter_graph_alloc();


	  AVFilter *buffersrc = avfilter_get_by_name("buffer");;
	  AVFilter *overlay = avfilter_get_by_name("buffersink");
	  AVFilter *buffersink = avfilter_get_by_name("overlay");
	  AVFilterInOut *outputs = avfilter_inout_alloc();
	  AVFilterInOut *inputs  = avfilter_inout_alloc();

	  snprintf(args, sizeof(args),  "video_size=%dx%d:pix_fmt=0:time_base=1/20000000:"
									"pixel_aspect=0/1:sws_param=flags=2:frame_rate=353/12",
									_width, _height);
	  //snprintf(args,sizeof(args),"video_size=%dx%d:pix_fmt=%d",288,352,AV_PIX_FMT_YUV420P);
	  ret = avfilter_graph_create_filter(_buffersrc_ctx, buffersrc, "in", args, NULL, _filter_graph);
	  if(ret<0)
	  {
		 fprintf(stderr,"avfilter_graph_create_filter(_buffersrc_ctx) failed\n");
		 return false;
	  }

	  //snprintf(args,sizeof(args),"overlay=.....");
	  ret = avfilter_graph_create_filter(overlay_ctx, overlay, NULL, args, NULL, _filter_graph);
	  if(ret<0)
	  {
		 fprintf(stderr,"avfilter_graph_create_filter(_buffersrc_ctx) failed\n");
		 return false;
	  }


	  ret = avfilter_graph_create_filter(_buffersink_ctx, buffersink, "out", NULL, NULL, _filter_graph);
	  if(ret < 0)
	  {
		 fprintf(stderr,"avfilter_graph_create_filter(_buffersink_ctx) failed\n");
		 return false;
	  }

	  ret = av_opt_set_int_list(_buffersink_ctx, "pix_fmts",pix_fmts, AV_PIX_FMT_YUV420P, AV_OPT_SEARCH_CHILDREN);
	  //ret = av_opt_set_bin(_buffersink_ctx, "pix_fmts", (uint8_t*)&pix_fmts, sizeof(pix_fmts), AV_OPT_SEARCH_CHILDREN);
	  if(ret < 0)
	  {
		 fprintf(stderr,"av_opt_set_int_list failed\n");
		 return false;
	  }

	  outputs->name = av_strdup("in");
	  outputs->filter_ctx = _buffersrc_ctx;
	  outputs->pad_idx = 0;
	  outputs->next = NULL;

	  inputs->name = av_strdup("out");
	  inputs->filter_ctx = _buffersink_ctx;
	  inputs->pad_idx = 0;
	  inputs->next = NULL;
	  
	  snprintf(filter_descr,128,"scale=%d:%d", _width, _height); 
	  ret = avfilter_graph_parse_ptr(_filter_graph, filter_descr, &inputs, &outputs, NULL);
	  if (ret < 0)
	  {
		 fprintf(stderr,"avfilter_graph_parse_ptr failed\n");
		 return false;
	  }

	  ret = avfilter_graph_config(_filter_graph, NULL);
	  if (ret < 0)
	  {
		 fprintf(stderr,"avfilter_graph_config failed\n");
		 return false;
	  }
	  avfilter_inout_free(&inputs);
	  avfilter_inout_free(&outputs);

	  return true;
   }
   

   bool proc(PAVBuffer & in, PAVBuffer & out)
   {
	  out.pframe = av_frame_alloc();
   }
   
};

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
   {  0,  0, 66, 66},					  { 66,  0, 34, 33},
										  { 66, 33, 34, 33},
   {  0, 66, 33, 34}, { 33, 66, 33, 34},  { 66, 66, 34, 34}
};

static MixPos MixPosInfo7[] = {
   {  0,  0, 33, 50},
   {  0, 50, 33, 50},  
									{ 67,  0, 33, 50},
									{ 67, 50, 33, 50},
				  { 33,  0, 34, 33},
				  { 33, 33, 34, 34},
				  { 33, 67, 34, 33}
};

static MixPos MixPosInfo8[] = {
   {  0,  0, 34, 50},
   {  0, 50, 34, 50},              
						{ 34,  0, 33, 34}, { 67,  0, 33, 34},
						{ 34, 34, 33, 33}, { 67, 34, 33, 33},
						{ 34, 67, 33, 33}, { 67, 67, 33, 33},
};

static MixPos MixPosInfo9[] = {
   {  0,  0, 34, 34}, { 34,  0, 33, 34}, { 67,  0, 33, 34},
   {  0, 34, 34, 33}, { 34, 34, 33, 33}, { 67, 34, 33, 33},
   {  0, 67, 34, 33}, { 34, 67, 33, 33}, { 67, 67, 33, 33}
};


class PVFilter
{
private:
   AVFrame* iframe;
   AVFrame* oframe;
   
   AVFilterGraph * _graph;
   AVFilterContext* _ctx;
   AVFilterContext* _ctxsrc; 
   AVFilterContext* _ctxsink; 
   AVFilter* _filtersrc;
   AVFilter* _filtersink;


};

class PAVMixer 
{
private:
   unsigned int _gid;  //group id

   //input buffer
   PAVJittBuffer      _ibufA[MAX_SES_PER_GROUP];
   PAVRingBuffer	  _ibufV[MAX_SES_PER_GROUP];


   //output buffer 
   PAVRingBuffer  _obufA[MAX_SES_PER_GROUP];
   PAVBuffer  _obufV; 

   //for mixing
   PAVBuffer   _mbufA[NUM_MIX_DIV1][NUM_MIX_DIV2];
   PAVBuffer   _mbuf[MAX_SES_PER_GROUP];
   

   std::map <unsigned int, unsigned int> _usedMap;
   std::deque <unsigned int> _idleQue;

   PMutex	_mutex;

public:
   PAVMixer();
   PAVMixer();

   bool init(unsigned int gid)
   {
	  LOG(LINF, "PAVMixer[gid:%d]->init()\n", gid);
	  _mutex.lock();
	  _gid = gid;

	  _idleQue.clear();
	  _usedMap.clear();

	  for (unsigned int bid=0; bid < _sessions; bid++)
	  {
		 _idleQue.push_back(bid);
	  }
	  _mutex.unlock();
	  return true;
   }

   bool final()
   {
	  LOG(LINF, "PAVMixer[gid:%d]->final()\n", _gid);
	  _mutex.lock();
	  _gid = 0;
	  _mutex.unlock();
	  return true;
   }


   bool  alloc(unsigned int sid)
   {
	  _mutex.lock();
	  if (_usedMap.find(sid) != _usedMap.end())
	  {
		 LOG(LERR, "PAVMixer[gid:%d]->alloc(sid:%04d) :Error, can't alloc(already used session)\n", _gid, sid);
		 _mutex.unlock();
		 return false;
	  }

	  if(_idleQue.empty())
	  {
		 LOG(LERR, "PAVMixer[gid:%d]->alloc(sid:%04d) :Error, can't alloc(no idle session)\n", _gid, sid);
		 _mutex.unlock();
		 return false;
	  }

	  unsigned int bid = _idleQue.front();
	  _idleQue.pop_front();
	  _usedMap[sid] = bid;

	  LOG(LINF, "PAVMixer[gid:%d]->alloc(sid:%04d)-bid:%d\n", _gid, sid, bid);
	  _mutex.unlock();
	  return true;
   }

   bool  dealloc(const unsigned int& sid) // ▒▒▒▒▒▒ buffer Index id return
   {
	  _mutex.lock();
	  if (_usedMap.find(sid) == _usedMap.end())
	  {
		 LOG(LERR, "PAVMixer[gid:%d]->dealloc(sid:%04d) :Error, can't dealloc(not used session)\n", _gid, sid);
		 _mutex.unlock();
		 return false;
	  }
	  unsigned int bid = _usedMap[sid];
	  _idleQue.push_back(bid);
	  _usedMap.erase(sid);
	  LOG(LINF, "PAVMixer[gid:%d]->dealloc(sid:%04d)-bid:%d\n", _gid, sid, bid);
	  _mutex.unlock();
	  return true;
   }

   bool putA(unsigned int sid, const PAVBuffer& in)
   {
	  _mutex.lock();
	  if (_usedMap.find(sid) == _usedMap.end())
	  {
	     _mutex.unlock();
		 return false;
	  }
	  unsigned int bid = _usedMap[sid];
	  _mutex.unlock();
	  if(in.skip() == 0)
	  {
		 bres = _ibufA[bid].put(in);
#ifdef _MDEBUG
		 CTime_ ctime;
		 LOG(LINF, "%s M-PUTA [id=%04d/%04d] %s",
			   (LPCSTR)ctime.Format("%T"), bid, sid, in.str().c_str());
#endif
	  }
	  return bres;
   }

   bool getA(unsigned sid, PAVBuffer& out)
   {
	  _mutex.lock();
	  if (_usedMap.find(sid) == _usedMap.end())
	  {
		 _mutex.unlock();
		 return false;
	  }

	  unsigned int bid = _usedMap[sid];
	  _mutex.unlock();

	  return _obufA[bid].get(out);
   }

   bool putV(unsigned int sid, const PAVBuffer& in)
   {
	  _mutex.lock();
	  if (_usedMap.find(sid) == _usedMap.end())
	  {
	     _mutex.unlock();
		 return false;
	  }
	  unsigned int bid = _usedMap[sid];
	  _mutex.unlock();
	  if(in.skip() == 0)
	  {
		 bres = _ibufV[bid].put(in);
#ifdef _MDEBUG
		 CTime_ ctime;
		 LOG(LINF, "%s M-PUTV [id=%04d/%04d] %s",
			   (LPCSTR)ctime.Format("%T"), bid, sid, in.str().c_str());
#endif
	  }
	  return bres;
   }

   bool getV(unsigned int sid, PAVBuffer& out)
   {
	  _mutex.lock();
	  if (_usedMap.find(sid) == _usedMap.end())
	  {
		 _mutex.unlock();
		 return false;
	  }

	  //unsigned int bid = _usedMap[sid];
	  out.set(_obufV);
	  _mutex.unlock();

	  return true;
	  //return _obufV[bid].get(out);
   }


   bool mix()
   {
	  // SID maps
	  _tmpBuf.clear(); // & reset() ?
	  // read bgm & mixing

	  unsigned int tot_count = 0;
	  unsigned int div_count = 0;
	  unsigned int count = 0;
	  unsigned int i1 = 0;
	  unsigned int i2 = 0;

	  _mutex.lock();
	  if(_usedMap.size() == 0)
	  {
		 _mutex.unlock();
		 return false;
	  }

	  for (auto &oIndex : _usedMap)
	  {
		 i2 = tot_count % NUM_MIX_DIV2;
		 i1 = tot_count / NUM_MIX_DIV2;
		 iv = tot_count;
		 
		 _mbufA[i1][i2].clear(); //mix buffer
		 _index[i1][i2] = oIndex.second; //mix buffer index

		 _ibufA[oIndex.second].get(_tbufA[i1][i2]); //temp buffer
		 _ibufV[oIndex.second].get(_tbufV[iv]); //temp videobuffer

#ifdef _MDEBUG
		 CTime_ ctime;
		 printf("%s M-GET [id=%04d] %s",
			   (LPCSTR)ctime.Format("%T"), _index[i1][i2],
			   _tbufA[i1][i2].str().c_str());
#endif
		 tot_count++;
	  }
	  _mutex.unlock();

	  // 2. mix audio 
	  div_count = i1+1;
	  count = 0;
	  //printf(" # count : %d/%d\n", div_count, tot_count);
	  for(i1=0; i1 < div_count; i1++)
	  {
		 _pMidBuffer[i1].clear();
		 for(i2=0; i2<NUM_MIX_DIV2 && count < tot_count; i2++, count++)
		 {
			_pMidBuffer[i1].mix(_tbufA[i1][i2]);

			unsigned int count2 = 0;
			for(unsigned int iself=0; iself<NUM_MIX_DIV2 && count2 < tot_count; iself++, count2++)
			{
			   if(iself == i2) continue;
			   //printf(" # mix : [%d,%d]\n", i1, i2);
			   _mbufA[i1][i2].mix(_tbufA[i1][iself]);
			}
		 }
	  }

	  count = 0;
	  for(i1=0; i1 < div_count; i1++)
	  {
		 for(i2=0; i2<NUM_MIX_DIV2 && count < tot_count; i2++, count++)
		 {
			for(unsigned int iself1=0; iself1<div_count; iself1++)
			{
			   if(iself1 == i1) continue;
			   _mbufA[i1][i2].mix(_pMidBuffer[iself1]);
			}
		 }
	  }

	  count = 0;
	  for(i1=0; i1 < div_count; i1++)
	  {
		 for(i2=0; i2<NUM_MIX_DIV2 && count < tot_count; i2++, count++)
		 {
			_obufA[_index[i1][i2]].put(_mbufA[i1][i2]);
#ifdef _MDEBUG
			CTime_ ctime;
			printf("%s M-PUT [id=%04d] %s",
				  (LPCSTR)ctime.Format("%T"), _index[i1][i2],
				  _tbufA[i1][i2].str().c_str());
#endif
		 }
	  }


	  if(tot_count != prev_count) 
	  {
		 // frame  init -> black screen
	  }

	  for(iv=0; iv<tot_count && iv<10; iv++) 
	  {
		 _mixFilter[iv].apply(_obufV, _tbufV[iv]); 
	  }

	  return true;
   }
};




#endif //__PAVMixer__
