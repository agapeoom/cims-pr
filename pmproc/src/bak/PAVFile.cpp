#include <sys/stat.h>
#include <iostream>

#include "TCodec.h"
#include "PAVFile.h"
#include "wavreader.h"
#include "wavwriter.h"




PWavWriter::PWavWriter()
{
   _handle = NULL;

   _sampleRate = LPCM_RATE_DEFAULT; // LPCM16K
   _channels  = 1;
   _format    = 1;
   _bitsPerSample = 16;

   _frameSize  = _sampleRate * 2  / 50; // bytes / 20msec (default 640bytes)
}

PWavWriter::~PWavWriter()
{
   close();
}

static std::string dirnameOf(const std::string& fname)
{
   size_t pos = fname.find_last_of("\\/");
   return (std::string::npos == pos) ? "" : fname.substr(0, pos);
}

static int mkpath(std::string s,mode_t mode)
{
   size_t pos=0;
   std::string dir;
   int mdret;

   if(s[s.size()-1]!='/'){
	  // force trailing / so we can handle everything in loop
	  s+='/';
   }

   while((pos=s.find_first_of('/',pos))!=std::string::npos){
	  dir=s.substr(0,pos++);
	  if(dir.size()==0) continue; // if leading / first time is 0 length
	  if((mdret=mkdir(dir.c_str(),mode)) && errno!=EEXIST){
		 return mdret;
	  }
   }
   return mdret;
}


bool PWavWriter::open(const std::string fname)
{
   _fname = fname;
   _format = WAV_LPCM16K;

#if 0 // reader
   _handle = wav_read_open(_fname.c_str());
   if(!_handle) return false;

   if(!wav_get_header(_handle, &_format, &_channels, &_sampleRate, &_bitsPerSample, NULL))
   {
	  wav_read_close(_handle);
	  return false;
   }

   if(_format !=1 || _bitsPerSample != 16 || _channels != 1)
   {	 
	  std::cerr << formatStr("Unsupported WAV format(format:%d, ch:%d, bits:%d )!\n",
			_format, _channels, _bitsPerSample);
	  wav_read_close(_handle);
	  return false;
   }

   _frameSize = _sampleRate * 2  / 50; // bytes / 20msec 
#endif
   mkpath(dirnameOf(_fname), 0755);
   _handle = wav_write_open(_fname.c_str(), _sampleRate, _bitsPerSample, _channels);
   if(!_handle) return false;
   return true;
}

bool PWavWriter::close()
{
   if(!_handle) return false;
  
   //if(_mode == READER)
   //	 wav_read_close(_handle);
   //if(_mode == WRITER)
   wav_write_close(_handle);
  
   _handle = NULL;
   
   _sampleRate = LPCM_RATE_DEFAULT;
   _channels  = 1;
   _format    = 1;
   _bitsPerSample = 16;
   _frameSize  = _sampleRate * 2  / 50; // bytes / 20msec (default 640bytes)

   return true;
}

/*
bool PWavWriter::read(TMBuffer & out)
{
   if(!_handle || _mode != READER) return false;
   int len = wav_read_data(_handle, out.ptr(), _frameSize);

   if(len != _frameSize) return false;
   
   return true;
}
*/

static char _gpcmSID[1024];
static int _gpcmSIDInit() 
{
   memset(_gpcmSID, 0, 1024);
   return 0;
}

static int xxx = _gpcmSIDInit();

bool PWavWriter::write(const TMBuffer & in)
{
   //if(!_handle || _mode != WRITER) return false;
   if(!_handle) return false;

   if(in.len() != _frameSize) 
   {
	  wav_write_data(_handle, _gpcmSID, _frameSize);
   } 
   else 
   {
	  wav_write_data(_handle, in.ptr(), _frameSize);
   }
   
   return true;
}

PAmrWriter::PAmrWriter() {}
PAmrWriter::~PAmrWriter() {}

bool PAmrWriter::open(const std::string fname)
{
   _fname = fname;
   _format = AMR;
   mkpath(dirnameOf(_fname), 0755); 

   _fs.open(_fname, std::fstream::in | std::fstream::binary | std::fstream::trunc);
   if (!_fs.is_open()) return false;
   _fs.write(AMR_MAGIC_NUMBER, sizeof(AMR_MAGIC_NUMBER)-1);

   return true;
}


bool PAmrWriter::close()
{
   _fs.close();
}

bool PAmrWriter::write(const TMBuffer & in)
{
   if (!_fs.is_open()) return false;
   if(in.len() > 0) //skip
   {
	  _fs.write(in.ptr(1), in.len()-1); // default octet-align 
   }
   else 
   {
	  _fs.write(AMR_NODATA, sizeof(AMR_NODATA));
   }
   return true;
}



PAmrwbWriter::PAmrwbWriter() {}
PAmrwbWriter::~PAmrwbWriter() {}

bool PAmrwbWriter::open(const std::string fname)
{
   _fname = fname;
   _format = AMRWB;
   mkpath(dirnameOf(_fname), 0755); 
   _fs.open(_fname, std::fstream::in | std::fstream::binary | std::fstream::trunc);
   if (!_fs.is_open()) return false;
   
   _fs.write(AMRWB_MAGIC_NUMBER, sizeof(AMRWB_MAGIC_NUMBER)-1);

   return true;
}


bool PAmrwbWriter::close()
{
   _fs.close();
}

bool PAmrwbWriter::write(const TMBuffer & in)
{
   if (!_fs.is_open()) return false;
   if(in.len() > 0) //skip
   {
	  _fs.write(in.ptr(1), in.len()-1); // default octet-align 
   }
   else 
   {
	  _fs.write(AMRWB_NODATA, sizeof(AMRWB_NODATA));
   }
   return true;
}



