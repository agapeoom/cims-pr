#ifndef __HAV_RTP_UTIL_INT_
#define __HAV_RTP_UTIL_INT_

#include <vector>
//#include "mtcodectype.h"
//#include "mtrtpdefine.h"
#include "havrtputil.h"

using namespace std;

namespace AMT 
{

typedef enum {
   PLT_UNKNOWN = 0, 
   PLT_STAPA, 
   PLT_SINGLE, 
   PLT_FU,
   PLT_SPSPPS,
   PLT_HEVC_FU
} PL_TYPE;

typedef struct {
   unsigned char *sps_ptr;
   int           sps_len;
   unsigned char *pps_ptr;
   int           pps_len;
} STAPAINFO, SPSPPSINFO;

typedef struct {
   unsigned char *ptr;
   int           len; 
} SINGLEINFO;

typedef struct {
   unsigned char *ptr;
   int           len; 
   bool          begin;
   bool          end; 
   unsigned char type;
   unsigned char nri; 
} FUINFO;

typedef struct {
   unsigned char *ptr;
   int            len;
   bool           begin;
   bool           end;
   unsigned char nal_type;
} HEVC_FUINFO;

// 2021.01.20 for H265 AP Mode
typedef struct {
   unsigned char *ptr;
   int            len;
   unsigned char nal_type;
} APINFO;

typedef struct {
   PL_TYPE type;
   union {
      STAPAINFO stapa;
      SPSPPSINFO stsps;
      SINGLEINFO single;
      FUINFO fu;
      HEVC_FUINFO hevc_fu;
      APINFO ap;
   };
} PLINFO;


// H264 Byte-stream to RTP
class CHAVH264ToRtp : public CHAVStreamConv
{
public:
	CHAVH264ToRtp();
	~CHAVH264ToRtp();

   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   bool Put(bool bFragEnd, unsigned char* pData, int nLength);
   bool TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);
   void SetNalLengthSize(unsigned char *pExtraData);
   void FlushBuffered();
   
   static   int ms_nPrintSprop;
   static   void SetPrintSprop(int nPrintSprop) { ms_nPrintSprop = nPrintSprop;}

protected:
   bool Clear();

protected:
   enum {PSC_LENGTH = 4};
   vector<void *> m_PacketList;
   bool			  m_bFu;
   unsigned char m_cFuHeader;
   unsigned int  m_nFuIndex;
   unsigned int  m_nFuCount;
   char m_pExtraData[1024];
   unsigned int m_nExtraSize;
   unsigned int m_nNalLengthSize;

   // 2021.01.21 AP Mode..
   unsigned char *m_pNalBuffer;
   unsigned int m_unbuffered_size;
   unsigned int m_unbuffered_nals;


protected:
   void ClearPacketList();
   void GetExtraData();
   bool MakeSinglePacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength);
   bool MakeFuPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength);
   bool MakeAPPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength);
   void AddPacket(unsigned char *buf,int size,int last);
};

/////////////////////////////////////////////////////////////////////////////////////
// RTP to H264 Byte-stream
/////////////////////////////////////////////////////////////////////////////////////
class CHAVRtpToH264 : public CHAVStreamConv
{
public:
	CHAVRtpToH264();
	virtual ~CHAVRtpToH264();

   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   virtual bool Put(bool bFragEnd, unsigned char* pData, int nLength);
   virtual bool TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);
   int ff_h264_handle_frag_packet(const unsigned char *buf, int len, 
                               int start_bit, const unsigned char *nal_header,
                               int nal_header_len);
   int h264_handle_packet_fu_a(bool bFragEnd, unsigned char* pData, int nLength);

protected:
   virtual bool Clear();
   enum {PSC_LENGTH = 4};
   int  m_nFragLeft;

};
//////////////////////////////////////////////////
// H264 Byte-stream to RTP
//////////////////////////////////////////////////
class CHAVH264BsToRtp : public CHAVStreamConv
{
public:
	CHAVH264BsToRtp();
	~CHAVH264BsToRtp();

   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   bool Put(bool bFragEnd, unsigned char* pData, int nLength);
   bool TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);
	// 2012.08.02 by minimei7 for print sprop-paramter-sets
   static   int ms_nPrintSprop;
   static   void SetPrintSprop(int nPrintSprop) { ms_nPrintSprop = nPrintSprop;}

protected:
   bool Clear();

protected:
   enum {PSC_LENGTH = 4};
   vector<void *> m_PacketList;
   bool   m_bFu;
   unsigned char m_cFuHeader;
   unsigned int  m_nFuIndex;
   unsigned int  m_nFuCount;

protected:
   void ClearPacketList();
   void SetNRI(int nNalType, unsigned char* pcOut);
   bool AddDecInfoPacket();
   bool AddPicturePacket();
   bool MakeDecInfoPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength);
   bool MakeSpsInfoPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength);
   bool MakeSinglePacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength);
   bool MakeFuPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);

};

/////////////////////////////////////////////////////////////////////////////////////
// RTP to H264 Byte-stream
/////////////////////////////////////////////////////////////////////////////////////
class CHAVRtpToH264Bs : public CHAVStreamConv
{
public:
	CHAVRtpToH264Bs();
	virtual ~CHAVRtpToH264Bs();

   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   virtual bool Put(bool bFragEnd, unsigned char* pData, int nLength);
   virtual bool TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);

protected:
   virtual bool Clear();
   enum {PSC_LENGTH = 4};
   int  m_nFragLeft;

};


#if 0
/////////////////////////////////////////////////////////////////////////////
// H263 Byte-stream to RTP
/////////////////////////////////////////////////////////////////////////////
class CHAVH263BsToRtp : public CHAVStreamConv
{
public:
	CHAVH263BsToRtp();
	~CHAVH263BsToRtp();

   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   bool TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);

protected:
   bool Clear();

protected:
   union {
      H263_PAYLOAD_HDR  m_Ph263D; // RFC2190  (default)
      H263B_PAYLOAD_HDR m_Ph263B;
		H263C_PAYLOAD_HDR m_Ph263C;
      H263P_PAYLOAD_HDR m_Ph263P; // RFC2429
   };

};



/////////////////////////////////////////////////////////////////////////////////////
// RTP to H263 Byte-stream
/////////////////////////////////////////////////////////////////////////////////////

class CHAVRtpToH263Bs : public CHAVStreamConv
{
public:
	CHAVRtpToH263Bs();
	virtual ~CHAVRtpToH263Bs();

   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   virtual bool Put(bool bFragEnd, unsigned char* pData, int nLength);

protected:
   virtual bool Clear();
   int  m_nFragLeft;
   int m_nIFrameReaded;

};

#endif

/////////////////////////////////////////////////////////////////////////////
// JPEG Byte-stream to RTP
/////////////////////////////////////////////////////////////////////////////

class CHAVJPEGBsToRtp : public CHAVStreamConv
{
public:
	CHAVJPEGBsToRtp();
	virtual ~CHAVJPEGBsToRtp();

   bool TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd);

protected:
   virtual bool Clear();

   int READ_WORD(int *pn);
   int READ_BYTE(int *pn);
   int ParseJPEGHeader();
   int GetNextMarker(unsigned char* pMarker);

   int m_nWidth;
   int m_nHeight;
   unsigned char* m_pQTbl;
   int m_nQTblLen;

};

class CHAVAMRBsToRtp: public CHAVStreamConv
{
public:
	CHAVAMRBsToRtp();
	virtual ~CHAVAMRBsToRtp();
   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   bool Put(bool bFragEnd, unsigned char* pData, int nInLength);
};

class CHAVRtpToAMRBs: public CHAVStreamConv
{
public:
	CHAVRtpToAMRBs();
	virtual ~CHAVRtpToAMRBs();
   virtual bool Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode = PM_DEF);

   bool Put(bool bFragEnd, unsigned char* pData, int nInLength);
};

}; //namespace AMT 


#endif // __HAV_RTP_UTIL_INT_

