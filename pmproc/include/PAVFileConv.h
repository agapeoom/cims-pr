#ifndef __PAVFileConv__
#define __PAVFileCon__

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

class PAVFileConv 
{
private:
   PAVParam			 _rxParamA;
   PAVParam			 _rxParamV;
   PAVParam			 _txParamA;
   PAVParam			 _txParamV;
   PAVBuffer	     _rxBufA;
   PAVBuffer	     _rxBufV;
   
   PAVEncoder        _encA;
   PAVDecoder        _decA;

   PAResampler      _resamplerRx;
   PAResampler      _resamplerTx;

   PAVEncoder        _encV;
   PAVDecoder        _decV;
   PVMixFilter		 _mixV;

   std::string ipLoc;
   unsigned int portLocA;

   PAVCODEC  codecA;
   std::string ipRmtA;
   unsigned int portRmtA;
   unsigned int ptA;
   unsigned int ptDtmf;

   unsigned int portLocV;

   PAVCODEC  codecV;
   std::string ipRmtV;
   unsigned int portRmtV;
   unsigned int ptV;

   //PAVBuffer bufA;
   //PAVBuffer bufV;

   bool _bstartA;
   bool _bstartV;

   PMutex _mutex;

   PAVFormat   _rxFile;
   PAVFormat   _txFile;

   std::string _rxfname;
   std::string _txfname;
public:
   PAVFileConv() {};
   // 4. 
   virtual ~PAVFileConv() { final(); }


   bool init(const std::string & ifname, const::std::string & ofname) 
   {
	  bool bres; 
	  _rxParamA.init();
	  _txParamA.init();
	  _rxParamV.init();
	  _txParamV.init();

	  _rxfname = ifname;
	  _txfname = ofname;


	  bres = _rxFile.init(ifname, "avi", PAVFormat::RX);
	  if(!bres)
	  {
		 printf("# %s(%d):%s : failed to init RX File !\n", __FILE__, __LINE__, __FUNCTION__);
		 return false;
	  }

	  _rxParamA = _rxFile.getstream(PAVTYPE_AUDIO);
	  if(_rxParamA.codec != PAVCODEC_NONE)
	  {
		 bres = _decA.init(_rxParamA); 
		 if(!bres) 
		 {
			printf("# %s(%d):%s : failed to init Decoder(%s)!\n", __FILE__, __LINE__, __FUNCTION__, _rxParamA.str().c_str());
			return false;	
		 }
	  }

	  _rxParamV = _rxFile.getstream(PAVTYPE_VIDEO);
	  if(_rxParamA.codec != PAVCODEC_NONE)
	  {
		 bres = _decV.init(_rxParamV); 
		 if(!bres) 
		 {
			printf("# %s(%d):%s : failed to init Decoder(%s)!\n", __FILE__, __LINE__, __FUNCTION__, _rxParamV.str().c_str());
			return false;	
		 }
	  }

	  bres = _txFile.init(ofname, "avi", PAVFormat::TX);
	  if(!bres)
	  {
		 printf("# %s(%d):%s : failed to init TX File !\n", __FILE__, __LINE__, __FUNCTION__);
		 return false;
	  }

	 
	  return true;
   }
   void final()
   {
	  
	  _rxParamA.final();
	  _txParamA.final();
	  _rxParamV.final();
	  _txParamV.final();

	  _rxFile.final();
	  _txFile.final();
	  _decA.final();
	  _encA.final();
	  _decV.final();
	  _encV.final();
   }
   
   

   bool setRmtA(PAVParam & param)
   {
	  bool bres;

	  _txParamA = param;

	  bres = _txFile.addstream(_txParamA);
	  if(!bres) 
	  {
		 printf("# %s(%d):%s : failed to init addstream(%s)!\n", __FILE__, __LINE__, __FUNCTION__, _txParamA.str().c_str());
		 return false;	
	  }


	  bres = _encA.init(_txParamA); 
	  if(!bres) 
	  {
		 printf("# %s(%d):%s : failed to init Encoder(%s)!\n", __FILE__, __LINE__, __FUNCTION__, _txParamA.str().c_str());
		 return false;	
	  }
	  
   }
   bool setRmtV(PAVParam & param)
   {
	  bool bres;
	  _txParamV = param;
	  bres = _txFile.addstream(_txParamV);
	  if(!bres) 
	  {
		 printf("# %s(%d):%s : failed to init addstream(%s)!\n", __FILE__, __LINE__, __FUNCTION__, _txParamV.str().c_str());
		 return false;	
	  }

	  bres = _encV.init(_txParamV); 
	  if(!bres) 
	  {
		 printf("# %s(%d):%s : failed to init Encoder(%s)!\n", __FILE__, __LINE__, __FUNCTION__, _txParamV.str().c_str());
		 return false;	
	  }
   }

   std::string str() 
   {
	  return formatStr("# FConv\n"
					   "  - IN :[file:%s, audio:%s, vidoe:%s]\n"
					   "  - OUT:[file:%s, audio:%s, vidoe:%s]\n",
					   _rxfname.c_str(), _rxParamA.str().c_str(), _rxParamV.str().c_str(), 
					   _txfname.c_str(), _txParamA.str().c_str(), _txParamV.str().c_str()); 
   };    

   bool proc()
   {
	  int rxCountA=0;
	  int txCountA=0;
	  int rxCountV=0;
	  int txCountV=0;
	  PAVBuffer buf; 
	  buf.init();
	  bool bres;
	  while(_rxFile.get(buf))
	  {
		 buf._packet->pts = buf._packet->dts;
		 if(buf._type == PAVTYPE_AUDIO) 
		 {
			rxCountA++;		
			bres = _decA.put(buf);
			if(!bres) break;

			while(_decA.get(buf))
			{
			   bres = _encA.put(buf);
			   if(!bres) break;
			   while(_encA.get(buf))
			   {
				  txCountA++;
				  printf("# File Audio Write : %s \n", buf.str().c_str());
				  bres = _txFile.put(buf);
				  if(!bres) break;
			   }
			}
		 }
		 else if(buf._type == PAVTYPE_VIDEO) 
		 {
			rxCountV++;		
			bres = _decV.put(buf);
			if(!bres) break;

			while(_decV.get(buf))
			{
			   bres = _encV.put(buf);
			   if(!bres) break;

			   while(_encV.get(buf))
			   {
				  txCountV++;		

				  printf("# File Video Write : %s \n", buf.str().c_str());

				  bres = _txFile.put(buf);
				  if(!bres) break;
			   }
			}
		 }
		 else 
		 {
			printf("# read unknown packet!\n");
		 } 
	  }

	  if(rxCountA>0) 
	  {
		 bres = _encA.put();
		 if(bres)
		 {
			while(_encA.get(buf))
			{
			   txCountA++;		
			   bres = _txFile.put(buf);
			   if(!bres) break;
			}
		 }
	  }

	  if(rxCountV>0)
	  {
		 rxCountV++;		
		 bres = _encV.put();
		 if(bres)
		 {
			while(_encV.get(buf))
			{
			   txCountV++;		
			   bres = _txFile.put(buf);
			   if(!bres) break;
			}
		 }
	  }

	  printf("# Done(audio:%d/%d, video:%d/%d)!\n", rxCountA, txCountA, rxCountV, txCountV);
   }

};


#endif //__PAVFileCon__

