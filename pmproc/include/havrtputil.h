#ifndef __HAV_RTP_UTIL_
#define __HAV_RTP_UTIL_

#include <vector>
//#include "mtcodectype.h"
//#include "mtrtpdefine.h"

#include "PMPBase.h"
#include "PAVBase.h"

using namespace std;


namespace AMT 
{

/**************************************************************************
1. converts RTP to Byte-stream, and vice versa.
2. converts raw frame data to RTP packet as RTP specification.
   ; fragmenting/assembling, adding/removing payload headers for H263,...
3. handles only payload-data including payload-header, not RTP header.
**************************************************************************/


class CHAVStreamConv
{
public:
   typedef enum {
      ST_RTP  = 1,
      ST_BYTE = 2 // BYTE_STREAM
   } STREAM_TYPE;

   typedef enum {
      PM_DEF = 0,

      // H.263
      PM_H263 =  1, // RFC2190  (default) MODEA
      PM_H263P = 2, // RFC2429
      //PM_H263_MODEA = 9,
      PM_H263_MODEB = 9,
      // H.264
      PM_H264_SINGLE = 3,   // Single NAL unit mode (default) 
      PM_H264_NON_INT = 4,  // Non-interleaved mode
      PM_H264_INT = 5,      // Interleaved mode
		
		// AMR(NB,WB)
      PM_AMR_OCTET = 6,     // Octet Align mode
      PM_AMRNB_BAND = 7,      // Bandwidth Efficient mode
      PM_AMRWB_BAND = 8,      // Bandwidth Efficient mode
      //reserve 9   for PM_H263_MODEB = 9,

      PM_DEF_AP    = 10,      // Aggregation Mode H.264/H.265
   } PACKETIZATION_MODE;

public:
   // nFrameAccumCount : Frame count in a packet.
   static CHAVStreamConv* CreateNew(PAVCODEC nCodecType, STREAM_TYPE nStreamFrom, 
                                    STREAM_TYPE nStreamTo, int nFrameAccumCount=1);
   
	virtual ~CHAVStreamConv();

   // nFrameAccumCount : Frame count in a packet.
   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   virtual bool Close();
	
   virtual bool Put(bool bFragEnd, unsigned char* pData, int nLength);
   virtual bool TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);

   virtual void SetNalLengthSize(unsigned char *pExtraData);
   virtual int ff_h264_handle_aggregated_packet(const char *buf, int len);

protected:
   CHAVStreamConv();

   virtual bool Clear();
   bool PrepareBuffer(int nLength);

   int m_nMaxOutputSize;
   unsigned char* m_pBuf;
   bool m_bFragEnd;
   int  m_nBufLen;
   int  m_nPutPos;
   int  m_nGetPos;
   int  m_nFragIndex;
   int  m_nAccumCount;
   int  m_nFramesPut;

   PACKETIZATION_MODE m_nPacketMode; 
};


}; //namespace AMT 


#endif // __HAV_RTP_UTIL_

