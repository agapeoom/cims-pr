#ifndef __PNumFiles__
#define __PNumFiles__

#include <iostream>

#include "TMFile.h"



class PNumFiles
{
public:
   enum STATUS{
	  END =  0, // be done
	  NODATA = 1, 
	  DATA   = 2,
   };

   PNumFiles() {};
   ~PNumFiles() {};
   
   bool init(unsigned int number, int mode=1)
   {
	  _mode = mode; // 1: part, 2: group 
	  //_number = number;
	  _digit3 = (number / 100) % 10;
	  _digit2 = (number / 10) % 10;
	  _digit1 = number % 10;

	  _step = 4;

	  _pos = 0;
	  _posStep = 0;
	  _timestamp = 0;
	  gettimeofday(&_tvStartTime, NULL);
	  return true;
   }
   bool final()
   {
	  return true; 
   }

   PNumFiles::STATUS get(TCODEC codec, TMBuffer & out)
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

	  if(_mode==1) {
		 if(_postFile[codec].get(_posStep++, out))
		 {
			_timestamp += _postFile[codec].ptime();
			out.timestamp() = _timestamp;
			return DATA;
		 }
	  } else {
		 if(_postFile2[codec].get(_posStep++, out))
		 {
			_timestamp += _postFile2[codec].ptime();
			out.timestamp() = _timestamp;
			return DATA;
		 }
	  }
	  return END;
   }

  
   static bool load(const std::string prefix, TCODEC codec)
   {
	  std::string fileName;
	  std::string ext;

	  std::map<TCODEC, std::string> cname;
	  cname[TCODEC_LPCM16K] = "lpcm16k";
	  cname[TCODEC_AMR] = "amr";
	  cname[TCODEC_AMRWB] = "amrwb";

	  
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

	  fileName = prefix + "_END2." + cname[codec];
	  std::cout << "## file : " << fileName << std::endl;
	  _postFile2[codec].init(codec);
	  if(!_postFile2[codec].load(fileName)) return false;

	  return true;
   }

   static bool load(const std::string prefix)
   {

	  if(!load(prefix, TCODEC_LPCM16K)) return false;
	  if(!load(prefix, TCODEC_AMR)) return false;
	  if(!load(prefix, TCODEC_AMRWB)) return false;

	  return true;
   }
private:
   unsigned int _mode;
   unsigned int _step;
   unsigned int _pos;
   unsigned int _posStep;
   unsigned int _timestamp;

   unsigned int _digit1, _digit2, _digit3;
   struct timeval _tvStartTime;



   static std::map<TCODEC, TMFile>	     _preFile;
   static std::map<TCODEC, TMFile>	     _postFile;
   static std::map<TCODEC, TMFile>	     _postFile2;
   static std::map<TCODEC, std::map<unsigned int, TMFile>> _files1;
   static std::map<TCODEC, std::map<unsigned int, TMFile>> _files10;
   static std::map<TCODEC, std::map<unsigned int, TMFile>> _files100;
};

#endif //__PNumFiles__

