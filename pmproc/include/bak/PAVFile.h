#ifndef __PMWriter__
#define __PMWriter__

#include <string>
#include <fstream>

#include "TMBuffer.h"

class PMWriter
{
public:
   enum FORMAT 
   {
	  WAV_LPCM16K = 1,
	  AMR		  = 2,
	  AMRWB		  = 3
   };
public:
   PMWriter() {}
   virtual ~PMWriter() { close(); }

   virtual bool open(const std::string fname){}
   virtual bool read(TMBuffer & out){}
   virtual bool write(const TMBuffer & in){}
   virtual bool close(){}

protected:
   std::string  _fname;
   FORMAT       _format;
};


class PWavWriter : public PMWriter 
{
public:
   PWavWriter();
   ~PWavWriter();

   bool open(const std::string fname);
   //bool read(TMBuffer & out);
   bool write(const TMBuffer & in);
   bool close();

private:
   void * _handle;
   int    _sampleRate;
   int	 _channels;
   int	 _format;
   int	 _bitsPerSample;
   int	 _frameSize;
};

class PAmrWriter : public PMWriter 
{
public:
   PAmrWriter();
   ~PAmrWriter();

   bool open(const std::string fname);
   bool write(const TMBuffer & in);
   bool close();

private:
   std::ofstream _fs;
};

class PAmrwbWriter : public PMWriter 
{
public:
   PAmrwbWriter();
   ~PAmrwbWriter();

   bool open(const std::string fname);
   bool write(const TMBuffer & in);
   bool close();

private:
   std::ofstream _fs;
};

#endif //__PMWriter__
