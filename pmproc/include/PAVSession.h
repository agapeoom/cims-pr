#ifndef __PAVSession__
#define __PAVSession__

#include <map>
#include <string>
#include <deque>

#include "pmodule.h"
#include "phandler.h"

#include "PMPBase.h"

#include "PAVBase.h"
#include "PAVCodec.h"
#include "PAVBuffer.h"
#include "PAVFilter.h"
#include "PAVFormat.h"

class PAVSession : public PHandler
{
public:
   // 1. 
   PAVSession(const std::string & name);
   // 4. 
   virtual ~PAVSession();


   bool init(const std::string & ip, unsigned int port);
   void final();

   bool join(unsigned int gid, int priority);

   // 2 or 3. 
   //setRmtA(unsigned int codec, std::string & ip, unsigned int port, unsigned char pt, unsigned char ptDtmf)
   bool setRmtA(PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt, unsigned char ptDtmf);
   bool setRmtV(PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt);
   //bool setRmtV(PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt, int width, int height, int bitrate);

   bool setPlayFile(const std::string fname, const std::string fmt);
   bool setRecordFile(const std::string fname, const std::string fmt);

   bool start();
   void stop();
   // 2 or 3.
   // get Local RTP Address
   std::string & ipRtp() { return ipLoc; };    
   unsigned int portRtpA() { return portLocA; };
   unsigned int portRtpV() { return portLocV; };

   bool proc();
   bool proc(int id, const std::string & name, PEvent *spEvent);

private:
   bool _procRxRtpA();
   bool _procTxRtpA();
   bool _procRxRtpV();
   bool _procTxRtpV();
   bool _procRxFile();

   bool _procA();
   bool _procV();

   void _reset(); // clear instance setted by setRmtA/V, setPlayFile, setRecordFile

private:
   unsigned int	  _gid;
   unsigned int	  _priority;

   // Local RTP parameter 
   std::string	  ipLoc;
   unsigned int	  portLocA;
   unsigned int	  portLocV;

   // for audio(voice) processing
   PAVCODEC		  codecA;
   PAVParam		  _paramA;

   PAVBuffer	  rxBufA;
   PAVBuffer	  txBufA;
   

   PAVRtpComm	  rtpA;
   PAVEncoder	  encA;
   PAVDecoder	  decA;
   PAMixFilter	  mixA;

   PAResampler	  resamplerRx;
   PAResampler	  resamplerTx;


   // for video processing
   PAVCODEC		  codecV;
   PAVParam		  _paramV;

   PAVBuffer	  rxBufV;
   PAVBuffer	  txBufV;
   
   PAVRtpComm	  rtpV;
   PAVEncoder	  encV;
   PAVDecoder	  decV;
   PVMixFilter	  mixV;

   // play & record ...
   PAVSyncFormat  _fileRx;
   PAVFormat	  _fileTx;

   // running status
   bool			  _bplay; // PLAY_FILE(no RTP receiving)
   bool			  _brecord; // PLAY_FILE(no RTP receiving)

   bool			  _bstart;
   bool			  _baudio;
   bool			  _bvideo;

   PMutex		  _mutex;

};


class  PAVSesModule : public PModule
{
public:
   PAVSesModule(const std::string & name);
   ~PAVSesModule();

   // 0. initialize all rtp socket & resource
   bool init(unsigned int groups, unsigned int sessions, const std::string ip, unsigned int baseport);
   void final();

   // 1. alloc session  
   bool alloc(unsigned int uid, unsigned int gid, int priority);
  
   // 2. get local-rtp address infomation.
   std::string & ipRtp(unsigned int uid);
   unsigned int portRtpA(unsigned int uid);
   unsigned int portRtpV(unsigned int uid);

   // 3. set remote-rtp address & media parameters ...
   bool setRmtA(unsigned int uid, PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt, unsigned char ptDtmf);
   bool setRmtV(unsigned int uid, PAVCODEC codec, const std::string & ip, unsigned int port, unsigned char pt);
   
   bool setPlayFile(unsigned int uid, const std::string fname, const std::string fmt);
   bool setRecordFile(unsigned int uid, const std::string fname, const std::string fmt);

   // 4 run rtp session thread ...
   bool start(unsigned int uid);

   // 5. stop rtp session thread & dealloc ..
   bool dealloc(unsigned int uid); //stop & dealloc
private:
   PAVSession * _pctx;

   unsigned int _groups;
   unsigned int _sessions;

   std::map<unsigned int, PAVSession *> _sesMap; //index of session array : session
   std::map<unsigned int, unsigned int > _used;  //user id : index of session array
   std::deque<unsigned int> _idle;

};

#endif //__PAVSession__

