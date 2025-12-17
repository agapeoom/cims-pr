#include <iostream>

#include "TCodec.h"
#include "TMFile.h"
#include "PNumFiles.h"
#include "wavreader.h"

////////////////////////////////////////////////
// class TMFile
///////////////////////////////////////////////
TMFile::TMFile()
{
   _codec = TCODEC_NONE;
}

TMFile::~TMFile()
{
   final();
}

bool TMFile::init(TCODEC codec, TFORMAT format)
{
   _codec = codec;
   _format = format;
   switch (_codec)
   {
	  case  TCODEC_LPCM8K:
	  case TCODEC_AMR:
		 _ptime = 160;
		 _frameSizeLPCM = 320;
		 break;
	  case TCODEC_LPCM16K:
	  case TCODEC_AMRWB:
		 _ptime = 160;
		 _frameSizeLPCM = 640;
		 break;
	  case TCODEC_LPCM32K:
		 _ptime = 160;
		 _frameSizeLPCM = 1280;
		 break;
	  default:
		 _ptime = 0;
		 break; 
   };

   return true;
}

bool TMFile::final() 
{
   _codec = TCODEC_NONE;

   if(_bufVec.empty()) return true;

   for(auto &x: _bufVec) 
   {
	  delete x;
   }

   _bufVec.clear();

}

TCODEC TMFile::codec() { return _codec;  }
//int  TMFile::codec() { return _codec;  }
unsigned int TMFile::frames()
{
   return _bufVec.size();
}

bool TMFile::get(const unsigned int& index, TMBuffer & out)
{
   if(index<_bufVec.size())
   {
	  out = *_bufVec.at(index);
	  return true;
   }

   out.len()=0;
   return false;
}


bool TMFile::put(const TMBuffer & in) 
{
   TMBuffer * pbuf = new TMBuffer;
   *pbuf = in;
   _bufVec.push_back(pbuf);
}

bool TMFile::save(const std::string& file)
{
   char amrhdr[16] = "#!AMR\n";
   char amrwbhdr[16] = "#!AMR-WB\n";

   std::ofstream fs(file, std::fstream::in | std::fstream::binary | std::fstream::trunc);

   // can't open file!
   if (!fs) return false;
   if(_bufVec.empty()) return true;


   switch(_codec) 
   {
   case TCODEC_AMR:
	  fs.write(amrhdr, 6);
	  for(auto &x: _bufVec) 
	  {
		 if(_format == 0) 
			fs.write(x->ptr(), x->len());
		 else if(_format ==1)
			fs.write(x->ptr(1), x->len()-1);
		 else 
		 { // bandwidth efficient
		   // not yet
		 }
	  }
	  break;
   case TCODEC_AMRWB:
	  fs.write(amrwbhdr, 9);
	  for(auto &x: _bufVec) 
	  {
		 if(_format == 0) 
			fs.write(x->ptr(), x->len());
		 else if(_format ==1) 
		 {
			std::cout << "### >> " ;
			for(int j=0; j<x->len()-1; j++)
			{
			   std::cout << formatStr("%02x ", *x->ptr(1+j));
			}
			std::cout << std::endl;

			fs.write(x->ptr(1), x->len()-1);
		 }
		 else 
		 { // bandwidth efficient
		   // not yet
		 }
	  }
	  break;
   default:
	  for(auto &x: _bufVec) 
	  {
		 fs.write(x->ptr(), x->len());
	  }
	  break;
   }

   fs.close();
   return true;
}

bool TMFile::load(const std::string& file)
//bool TMFile::load(const std::string& file)
{

   // 16 bits Linear-PCM  fmrae size(20msec)
   // 2bytes / 1sample * 8000 / 1000 * 20
   //std::ifstream  fs(file, std::fstream::out | std::fstream::binary);
   //std::ifstream  fs(file, std::fstream::in | std::fstream::binary);
   std::ifstream  fs(file, std::fstream::binary);
   // can't open file!
   if (!fs) return false;

   bool bRes = false;
   switch (_codec)
   {
	  case  TCODEC_LPCM8K:
		 bRes = _readLPCM(fs, 320);
		 break;
	  case TCODEC_LPCM16K:
		 bRes = _readLPCM(fs, 640);
		 break;
	  case TCODEC_LPCM32K:
		 bRes = _readLPCM(fs, 1280);
		 break;
	  case TCODEC_AMR:
		 bRes = _readAmr(fs);
		 break;
	  case TCODEC_AMRWB:
		 bRes = _readAmrwb(fs);
		 break;
	  default:
		 break; 
   };

   fs.close();
  
   return bRes; 

   return true;
}


bool TMFile::loadWav(const std::string& file) 
{
   void * wav;
   int format, channels, sampleRate, bitsPerSample;

   wav = wav_read_open(file.c_str());
   if(!wav) return false;

   if(!wav_get_header(wav, &format, &channels, &sampleRate, &bitsPerSample, NULL))
   {	 
	  wav_read_close(wav);
	  return false;
   }

   if(format !=1 || bitsPerSample != 16 || channels != 1) 
   {
	  std::cerr << formatStr("Unsupported WAV format(format:%d, ch:%d, bits:%d )!\n", 
						     format, channels, bitsPerSample);
	  wav_read_close(wav);
	  return false;
   }

   switch(sampleRate) 
   {
   case 8000:
	  init(TCODEC_LPCM8K, 0);
	  break;
   case 16000:
	  init(TCODEC_LPCM16K, 0);
	  break;
   case 32000:
	  init(TCODEC_LPCM32K, 0);
	  break;
   default:
	  wav_read_close(wav);
	  return false;
	  break;
   }

   int len = 0; 
   unsigned int seq = 1;
   unsigned int timestamp = _ptime;
   while(true)
   {
	  TMBuffer * pbuf = new TMBuffer;
	  // if(!fs.read(pbuf->ptr(), frameSize)) break;

	  len = wav_read_data(wav, pbuf->ptr(), _frameSizeLPCM);
	  if(len!=_frameSizeLPCM) break;

	  pbuf->codec() = _codec;
	  pbuf->len() = _frameSizeLPCM;
	  pbuf->skip() = false;
	  pbuf->timestamp() = timestamp;
	  pbuf->sequence() = seq;

	  _bufVec.push_back(pbuf);

	  seq++;
	  timestamp += _ptime; 
   }

   wav_read_close(wav);
   return true;
}


bool TMFile::_readLPCM(std::ifstream & fs, unsigned int frameSize)
{
   unsigned int seq = 1;

   unsigned int timestamp = _ptime;
   while(true)
   {
	  TMBuffer * pbuf = new TMBuffer;
	  if(!fs.read(pbuf->ptr(), frameSize)) break;
	  pbuf->codec() = _codec;
	  pbuf->len() = frameSize;
	  pbuf->skip() = false;
	  pbuf->timestamp() = timestamp;
	  pbuf->sequence() = seq;

	  _bufVec.push_back(pbuf);

	  seq++;
	  timestamp += _ptime; 
   }
   return true;
}

/* From WmfDecBytesPerFrame in dec_input_format_tab.cpp */
const int amrFrameSizes[] = { 12, 13, 15, 17, 19, 20, 26, 31, 5, 6, 5, 5, 0, 0, 0, 0 };

bool TMFile::_readAmr(std::ifstream & fs)
{
   char  amrhdr[16];
   fs.read(amrhdr, 6);
   if(!fs) return false;

   if(memcmp(amrhdr, "#!AMR\n", 6) != 0) 
   {
	  return false;
   }

   int frameSize=0;
   unsigned int seq = 1;
   unsigned int timestamp = _ptime;

   while(true) 
   {
	  TMBuffer * pbuf = new TMBuffer;

	  if(_format ==0) 
	  {
		 if(!fs.read(pbuf->ptr(), 1)) break;
		 frameSize = amrFrameSizes[(*pbuf->ptr() >> 3) & 0x0f];

		 if(frameSize > 0) 
			if(!fs.read(pbuf->ptr()+1 , frameSize)) break;

		 frameSize += 1;
	  }
	  else if(_format ==1) // default octet alignment
	  {
		 *pbuf->ptr() = 0x0f << 4;
		 if(!fs.read(pbuf->ptr(1), 1)) break;
		 frameSize = amrFrameSizes[(*(pbuf->ptr(1)) >> 3) & 0x0f];

		 if(frameSize > 0) 
			if(!fs.read(pbuf->ptr(2) , frameSize)) break;

		 frameSize += 2;
	  } 
	  else  //2: bandwidth efficient
	  {
		 //not yet
	  }
	  pbuf->len() = frameSize;
	  pbuf->codec() = _codec;
	  pbuf->skip() = false;
	  pbuf->timestamp() = timestamp;
	  pbuf->sequence() = seq;
	  _bufVec.push_back(pbuf);

	  seq++;
	  timestamp += _ptime;
   }

   return true;
}

/* From pvamrwbdecoder_api.h, by dividing by 8 and rounding up */
const int amrwbFrameSizes[] = { 17, 23, 32, 36, 40, 46, 50, 58, 60, 5, -1, -1, -1, -1, -1, 0 };

bool TMFile::_readAmrwb(std::ifstream & fs)
{
   char  amrhdr[16];
   fs.read(amrhdr, 9);
   if(!fs) return false;

   if(memcmp(amrhdr, "#!AMR-WB\n", 9) != 0) 
   {
	  return false;
   }

   int frameSize=0;
   unsigned int seq = 1;
   unsigned int timestamp = _ptime;

   while(true) 
   {
	  TMBuffer * pbuf = new TMBuffer;

	  if(_format ==0) 
	  {
		 if(!fs.read(pbuf->ptr(), 1)) break;
		 frameSize = amrwbFrameSizes[(*pbuf->ptr() >> 3) & 0x0f];

		 if(frameSize > 0) 
			if(!fs.read(pbuf->ptr()+1 , frameSize)) break;

		 frameSize += 1;
	  }
	  else if(_format ==1) // default octet alignment
	  {
		 *pbuf->ptr() = 0x0f << 4;
		 if(!fs.read(pbuf->ptr(1), 1)) break;
		 frameSize = amrwbFrameSizes[(*(pbuf->ptr(1)) >> 3) & 0x0f];

		 if(frameSize > 0) 
			if(!fs.read(pbuf->ptr(2) , frameSize)) break;

		 frameSize += 2;
	  } 
	  else  //2: bandwidth efficient
	  {
		 //not yet
	  }

	  pbuf->len() = frameSize;
	  pbuf->codec() = _codec;
	  pbuf->skip() = false;
	  pbuf->timestamp() = timestamp;
	  pbuf->sequence() = seq;
	  _bufVec.push_back(pbuf);

	  seq++;
	  timestamp += _ptime;
   }
   return true;
}

////////////////////////////////////////////////
// class TSharedMFiles
///////////////////////////////////////////////

static std::map<TCODEC, std::map<unsigned int, TMFile>> TSharedMFiles::_files;

bool TSharedMFiles::init()
{
   _pos = 0;
   _timestamp = 0;
   gettimeofday(&_tvStartTime, NULL);
   return true;
}

bool TSharedMFiles::final()
{
   _pos = 0;
   return true;
}

TSharedMFiles::STATUS TSharedMFiles::get(TCODEC codec, unsigned int fIndex, TMBuffer & out)
{
   struct timeval tvCurTime;

   if(fIndex != _lastfIndex) {
	  _pos = 0;
	  _lastfIndex = fIndex;
   }

   gettimeofday(&tvCurTime, NULL);

   if(PDIFFTIME(tvCurTime, _tvStartTime) < (20*_pos)) 
   {
	  out.len() = 0;
	  return NODATA;
   }

	  _timestamp += _files[codec][fIndex].ptime();

   if(_files[codec].find(fIndex) == _files[codec].end()) 
   {
	  _timestamp += _files[codec][fIndex].ptime();
	  _pos = 0;
	  gettimeofday(&_tvStartTime, NULL);

	  return END;
   }

   bool bRes = _files[codec][fIndex].get(_pos++, out); // play done
   if(bRes) 
   {
	  _timestamp += _files[codec][fIndex].ptime();
	  out.timestamp() = _timestamp;
	  return DATA;
   }

   return END;
}

static bool TSharedMFiles::load(unsigned int fIndex, const std::string prefix)
{
   std::map<TCODEC, std::string> cname;
   cname[TCODEC_LPCM16K] = "lpcm16k";
   cname[TCODEC_AMR] = "amr";
   cname[TCODEC_AMRWB] = "amrwb";
  
   bool bRes;

   TCODEC codec = TCODEC_LPCM16K;
   std::string fileName = prefix + "." + cname[codec];
   _files[codec][fIndex].init(codec);
   bRes = _files[codec][fIndex].load(fileName);
   if(!bRes) return false;

   codec = TCODEC_AMR;
   fileName = prefix + "." + cname[codec];
   _files[codec][fIndex].init(codec);
   bRes = _files[codec][fIndex].load(fileName);
   if(!bRes) return false;

   codec = TCODEC_AMRWB;
   fileName = prefix + "." + cname[codec];
   _files[codec][fIndex].init(codec);
   bRes = _files[codec][fIndex].load(fileName);
   if(!bRes) return false;

   return true;
}

static std::map<TCODEC, TMFile>       PNumFiles::_preFile;
static std::map<TCODEC, TMFile>       PNumFiles::_postFile;
static std::map<TCODEC, TMFile>       PNumFiles::_postFile2;
static std::map<TCODEC, std::map<unsigned int, TMFile>> PNumFiles::_files1;
static std::map<TCODEC, std::map<unsigned int, TMFile>> PNumFiles::_files10;
static std::map<TCODEC, std::map<unsigned int, TMFile>> PNumFiles::_files100;


#if 0

bool TNumberFiles::init(unsigned int number)
{
   //_number = number;
   _digit3 = (number / 100) % 10;
   _digit2 = (number / 10) % 10;
   _digit1 = number % 10;

   _step = 4;

   _pos = 0;
   _posStep = 0;
   _timestamp = 0;
   gettimeofday(&_tvStartTime, NULL);
}

bool TNumberFiles::final()
{
   return true;
}


TNumberFiles::STATUS TNumberFiles::get(TCODEC codec, TMBuffer & out)
{
   struct timeval tvCurTime;
   gettimeofday(&tvCurTime, NULL);
   if(PDIFFTIME(tvCurTime, _tvStartTime) < (20*_pos)) 
   {
	  out.len() = 0;
	  return NODATA;
   }

   _pos++;
   if(_step == 4) {

	  if(_preFile[codec].get(_posStep++, out))
	  {
		 _timestamp += _preFile[codec].ptime();
		 out.timestamp() = _timestamp;
		 return DATA;  
	  }
	  _step--;
	  _posStep = 0;
   }

   if(_step == 3) {
	  if(_digit3 > 0) {
		 if(_files100[codec][_digit3].get(_posStep++, out))
		 {
			_timestamp += _files100[codec][_digit3].ptime();
			out.timestamp() = _timestamp;
			return DATA;  
		 }
	  }
	  _step--;
	  _posStep = 0;
   }

   if(_step == 2) {
	  if(_digit2 > 0) {
		 if(_files10[codec][_digit2].get(_posStep++, out))
		 {
			_timestamp += _files10[codec][_digit2].ptime();
			out.timestamp() = _timestamp;
			return DATA;  
		 }
	  }
	  _step--;
	  _posStep = 0;
   }

   if(_step == 1) {
	  if(_digit1 > 0) {
		 if(_files1[codec][_digit1].get(_posStep++, out))
		 {
			_timestamp += _files1[codec][_digit1].ptime();
			out.timestamp() = _timestamp;
			return DATA;  
		 }
	  }
	  _step--;
	  _posStep = 0;
   }

   if(_postFile[codec].get(_posStep++, out))
   {
	  _timestamp += _postFile[codec].ptime();
	  out.timestamp() = _timestamp;
	  return DATA;  
   }

   return END;
}

static std::map<TCODEC, TMFile>       TNumberFiles::_preFile;
static std::map<TCODEC, TMFile>       TNumberFiles::_postFile;
static std::map<TCODEC, std::map<unsigned int, TMFile>> TNumberFiles::_files1;
static std::map<TCODEC, std::map<unsigned int, TMFile>> TNumberFiles::_files10;
static std::map<TCODEC, std::map<unsigned int, TMFile>> TNumberFiles::_files100;


static bool TNumberFiles::load(const std::string prefix)
{
   std::string fileName;
   std::string ext;
   TCODEC	   codec;

   std::map<TCODEC, std::string> cname;
   cname[TCODEC_LPCM16K] = "lpcm16k";
   cname[TCODEC_AMR] = "amr";
   cname[TCODEC_AMRWB] = "amrwb";
  
   codec = TCODEC_LPCM16K;

   fileName = prefix + "_BEGIN." + cname[codec];
   std::cout << "## file : " << fileName << std::endl;
   _preFile[codec].init(codec);
   if(!_preFile[codec].load(fileName)) return false;

   for(int i=1; i<10; i++)
   {
	  fileName = formatStr("%s000%d.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files1[codec][i].init(codec);
	  if(!_files1[codec][i].load(fileName)) return false;
   
	  fileName = formatStr("%s00%d0.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files10[codec][i].init(codec);
	  if(!_files10[codec][i].load(fileName)) return false;
   
	  fileName = formatStr("%s0%d00.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files100[codec][i].init(codec);
	  if(!_files100[codec][i].load(fileName)) return false;
   }

   fileName = prefix + "_END." + cname[codec];
   std::cout << "## file : " << fileName << std::endl;
   _postFile[codec].init(codec); 
   if(!_postFile[codec].load(fileName)) return false;
   
  
   codec = TCODEC_AMR;

   fileName = prefix + "_BEGIN." + cname[codec];
   std::cout << "## file : " << fileName << std::endl;
   _preFile[codec].init(codec);
   if(!_preFile[codec].load(fileName)) return false;

   for(int i=1; i<10; i++)
   {
	  fileName = formatStr("%s000%d.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files1[codec][i].init(codec);
	  if(!_files1[codec][i].load(fileName)) return false;
   
	  fileName = formatStr("%s00%d0.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files10[codec][i].init(codec);
	  if(!_files10[codec][i].load(fileName)) return false;
   
	  fileName = formatStr("%s0%d00.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files100[codec][i].init(codec);
	  if(!_files100[codec][i].load(fileName)) return false;
   }

   fileName = prefix + "_END." + cname[codec];
   std::cout << "## file : " << fileName << std::endl;
   _postFile[codec].init(codec); 
   if(!_postFile[codec].load(fileName)) return false;
   
  
   codec = TCODEC_AMRWB;

   fileName = prefix + "_BEGIN." + cname[codec];
   std::cout << "## file : " << fileName << std::endl;
   _preFile[codec].init(codec);
   if(!_preFile[codec].load(fileName)) return false;

   for(int i=1; i<10; i++)
   {
	  fileName = formatStr("%s000%d.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files1[codec][i].init(codec);
	  if(!_files1[codec][i].load(fileName)) return false;
   
	  fileName = formatStr("%s00%d0.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files10[codec][i].init(codec);
	  if(!_files10[codec][i].load(fileName)) return false;
   
	  fileName = formatStr("%s0%d00.%s", prefix.c_str(), i, cname[codec].c_str());
	  std::cout << "## file : " << fileName << std::endl;
	  _files100[codec][i].init(codec);
	  if(!_files100[codec][i].load(fileName)) return false;
   }

   fileName = prefix + "_END." + cname[codec];
   std::cout << "## file : " << fileName << std::endl;
   _postFile[codec].init(codec); 
   if(!_postFile[codec].load(fileName)) return false;
   
   return true;
}
#endif
