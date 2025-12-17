#ifndef _HAV_RTP_UTIL_INT2_
#define _HAV_RTP_UTIL_INT2_

#include <vector>
//#include "mtcodectype.h"
//#include "mtrtpdefine.h"
#include "havrtputil.h"

using namespace std;

namespace AMT
{

class CHAVH265ToRtp : public CHAVStreamConv
{
public:
   CHAVH265ToRtp();
   virtual ~CHAVH265ToRtp();

   virtual bool Open(int nMaxOutputSize,PACKETIZATION_MODE nPktMode = PM_DEF);
   //virtual bool Open(int nMaxOutputSize,int nNalLengthSize=1);
   bool Put(bool bFragEnd, unsigned char* pData, int nLength);
   bool TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);
	void SetNalLengthSize(unsigned char *pExtraData);

protected:
   enum {PSC_LENGTH = 4};
   vector<void *> m_PacketList;
   bool m_bFu;
   unsigned char m_cFuHeader;
   unsigned int m_nFuIndex;
   unsigned int m_nFuCount;
   char m_pExtraData[1024];
   char m_pSps[256];
   char m_pPps[256];
   char m_pVps[256];
   char m_pSei[256];
   unsigned int m_nSpsSize;
   unsigned int m_nPpsSize;
   unsigned int m_nVpsSize;
   unsigned int m_nSeiSize;
   unsigned int m_nProfileId;
   unsigned int m_nUsingDonl;
   unsigned int m_nNalLengthSize;

   bool Clear();
   void ClearPacketList();
   void GetExtraData();
   bool MakeSinglePacket(int nIndex,unsigned char* pData,int nInLength, int *pnOutLength);
   //bool MakeFuPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);
   bool MakeFuPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength);
   void AddPacket(unsigned char *buf,int size,int last);
};

class CHAVRtpToH265 : public CHAVStreamConv
{
public:
   CHAVRtpToH265();
   virtual ~CHAVRtpToH265();
   
   virtual bool Open(int nMaxOutputSize,PACKETIZATION_MODE nPktMode = PM_DEF);
   virtual bool Put(bool bFragEnd, unsigned char* pData, int nLength);
   virtual bool TryGet(unsigned char *pData, int nInLength, int *pnOutLength, int *pnFragEnd);
   int ff_h264_handle_frag_packet(const unsigned char *buf, int len,
                               int start_bit, const unsigned char *nal_header,
                               int nal_header_len);
   int h264_handle_packet_fu_a(bool bFragEnd, unsigned char* pData, int nLength);
   int ff_h264_handle_aggregated_packet(const char* buf, int len);
protected:
   virtual bool Clear();
   int m_nFragLeft;
};

};//namespace AMT


#endif // _HAV_RTP_UTIL_INT2_
////
// EOF
////
