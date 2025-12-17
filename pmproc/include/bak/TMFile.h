#ifndef __TMFile__
#define __TMFile__


#include <vector>
#include <string>
#include <fstream>

#include "pbase.h"

#include "TMpaInterface.h"
#include "TMBase.h"
#include "TMBuffer.h"


// Memory File
class TMFile
{
private:
   TCODEC        _codec;
   TFORMAT		 _format; // if amr/amwwb , 0 : IF2, 1:octec alignment, 2:bandwidth effient
   unsigned int  _ptime; // all 160 
   unsigned int  _frameSizeLPCM;
   std::vector<TMBuffer *> _bufVec;  //pcm buffer 

public:
   TMFile();

   ~TMFile();

   bool init(TCODEC codec, TFORMAT format = TFORMAT_DEFAULT);
   bool final(); 

   TCODEC  codec();
   unsigned int frames();
   bool get(const unsigned int& index, TMBuffer & out);
   bool put(const TMBuffer & in);

   bool save(const std::string& file);
   bool load(const std::string& file);
   bool loadWav(const std::string& file);

   unsigned int ptime() { return _ptime; } // timestamp diff  / 20msec

private:
   bool _readLPCM(std::ifstream & fs, unsigned int frameSize);
   bool _readAmr(std::ifstream & fs);
   bool _readAmrwb(std::ifstream & fs);
   
   bool _writeLPCM(std::ofstream & fs, unsigned int frameSize);
   bool _writeAmr(std::ofstream & fs);
   bool _writeAmrwb(std::ofstream & fs);
 
};



class TSharedMFiles
{
private:
   unsigned int _pos;
   unsigned int _timestamp;
   struct timeval _tvStartTime;
   unsigned int _lastfIndex;
public:
   enum STATUS{
	  END =  0, // be done
	  NODATA = 1, 
	  DATA   = 2,
   };

   TSharedMFiles() {};
   ~TSharedMFiles() {};

   bool init() ;
   bool final() ;

   TSharedMFiles::STATUS get(TCODEC codec, unsigned int fIndex, TMBuffer & out);

   static bool load(unsigned int fIndex, const std::string fileName);

private:
   static std::map<TCODEC,std::map<unsigned int, TMFile>> _files;
};


#if 0
class TNumberFiles
{
public:
   enum STATUS{
	  END =  0, // be done
	  NODATA = 1, 
	  DATA   = 2,
   };

   TNumberFiles() {};
   ~TNumberFiles() {};
   
   bool init(unsigned int number);
   bool final();

   TNumberFiles::STATUS get(TCODEC codec, TMBuffer & out);

   
   static bool load(const std::string prefix);

private:
   unsigned int _step;
   unsigned int _pos;
   unsigned int _posStep;
   unsigned int _timestamp;

   unsigned int _digit1, _digit2, _digit3;
   struct timeval _tvStartTime;



   static std::map<TCODEC, TMFile>	     _preFile;
   static std::map<TCODEC, TMFile>	     _postFile;
   static std::map<TCODEC, std::map<unsigned int, TMFile>> _files1;
   static std::map<TCODEC, std::map<unsigned int, TMFile>> _files10;
   static std::map<TCODEC, std::map<unsigned int, TMFile>> _files100;
};
#endif
#endif // __TMFile__

