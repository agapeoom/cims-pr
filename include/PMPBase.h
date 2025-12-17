#ifndef __PMPBase__
#define __PMPBase__


#include <time.h>
#include <ctime>
#include <memory>
#include <string>
#include <chrono>
#include <string>
#include <sstream>

#define LERR		 stdout
#define LINF		 stdout
//#define LOG(X)	 fprint(X)

template<typename ... Args>
inline void LOG(void * x, Args ... args)
{
   fprintf(stdout, args...); 
}


enum PAVCODEC { 
   PAVCODEC_NONE, 
   PAVCODEC_PCMA, 
   PAVCODEC_PCMU, 
   PAVCODEC_AMR, 
   PAVCODEC_AMRWB,
   PAVCODEC_H264,
   PAVCODEC_H265,
};

enum PAVTYPE { 
   PAVTYPE_NONE, 
   PAVTYPE_AUDIO,
   PAVTYPE_VIDEO,
   PAVTYPE_EOS = 0x99,
};

template<typename ... Args>
inline std::string formatStr(const std::string& format, Args ... args)
{
   size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1;
   std::unique_ptr<char[]> buffer(new char[size]);
   snprintf(buffer.get(), size, format.c_str(), args ...);
   return std::string(buffer.get(), buffer.get() + size - 1);
}

inline std::string timeStr(time_t t)
{
   struct tm tt;
   localtime_r(&t, &tt);
   return formatStr("%02d:%02d:%02d", tt.tm_hour, tt.tm_min, tt.tm_sec);
}

inline std::string timeStr()
{
   struct tm tt;
   time_t t = time(NULL);
   localtime_r(&t, &tt);

   return formatStr("%02d:%02d:%02d", tt.tm_hour, tt.tm_min, tt.tm_sec);
}

inline std::string mstimeStr()
{
   struct tm      tt;
   auto now = std::chrono::system_clock::now();
   auto mill = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

   int msec = (int)(mill.count()%1000);
   time_t time  = (time_t)(mill.count()/1000);
   localtime_r(&time, &tt);
   return formatStr("%02d:%02d:%02d.%03d", tt.tm_hour, tt.tm_min, tt.tm_sec, msec);
}



#endif // __PMPBase__

