//#include "mtcodectype.h"
//#include "mtrtpdefine.h"

#include "havrtputil.h"
#include "havrtputil_int.h"
#include "havrtputil_int2.h"
#include "havutils.h"



extern "C" {
#include "libavutil/base64.h"
#include "libavutil/intreadwrite.h"
}

////////////////////////////////////////////////////////////////////


#define BOP_IS_ON(a, n)        ((a)[(n)>>3] & _BOP_BITS[(n) & 0x07])
#define BOP_BIT_GET(a, n)      (((a)[(n)>>3] & _BOP_BITS[(n) & 0x07])?1:0)

#define BOP_BIT_ON(a, n)       ((a)[(n)>>3] |= _BOP_BITS[(n) & 0x07])
#define BOP_BIT_OFF(a, n)      ((a)[(n)>>3] &= _BOP_BITS_NOT[(n) & 0x07])
#define BOP_BIT_SET(a, n, b)   (((b) == 0)?BOP_BIT_ON(a,n):BOP_BIT_OFF(a,b))

#define BOPR_IS_ON(a, n)       ((a)[(n)>>3] & _BOPR_BITS[(n) & 0x07])

#define BOPR_ON(a, n)          ((a)[(n)>>3] |= _BOPR_BITS[(n) & 0x07])
#define BOPR_OFF(a, n)         ((a)[(n)>>3] &= _BOPR_BITS_NOT[(n) & 0x07])

#define BOP_CLR_FRONT(b, n)    ( ((b) << (n)) >> (n))
#define BOP_CLR_REAR(b, n)     ( ((b) >> (n)) << (n))

#define amt_av_rb16(x) ((((const unsigned char*)(x))[0] << 8) | ((const unsigned char*)(x))[1])




namespace AMT 
{

static char const _BOP_BITS[8]       = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
static char const _BOP_BITS_NOT[8]   = {~(char)0x80,~(char)0x40,~(char)0x20,~(char)0x10,~(char)0x08,~(char)0x04,~(char)0x02,~(char)0x01};

static char const _BOPR_BITS[8]      = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
static char const _BOPR_BITS_NOT[8]  = {~(char)0x01,~(char)0x02,~(char)0x04,~(char)0x08,~(char)0x10,~(char)0x20,~(char)0x40,~(char)0x80};

static int  BitsGetInt(const unsigned char* pBytes, int nBitFrom, int nBitLen) {
   int n = 0;
   for(int i=0;i<nBitLen;i++) {
      n <<= 1;
      if(BOP_IS_ON(pBytes, nBitFrom+i)) {
         n |= 0x01;   
      }
   }
   return n;   
}

const static unsigned char psc[4] = {0,0,0,1};
/////////////////////////////
// ffmpeg source bring some in here
/////////////////////////////
static unsigned char * amt_ff_avc_mp4_find_startcode(unsigned char *start, unsigned char *end, int nal_length_size)
{
    unsigned int res = 0;

    if (end - start < nal_length_size)
        return NULL;
    while (nal_length_size--)
        res = (res << 8) | *start++;

    if (res > end - start)
        return NULL;

    return start + res;
}

static unsigned char * amt_ff_avc_find_startcode_internal(unsigned char *p,unsigned char *end)
{
    unsigned char *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(uint32_t*)p;
//      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
//      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
        if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

static unsigned char * amt_ff_avc_find_startcode(unsigned char *p, unsigned char *end){
    unsigned char *out= amt_ff_avc_find_startcode_internal(p, end);
    if(p<out && out<end && !out[-1]) out--;
    return out;
}

int CHAVStreamConv::ff_h264_handle_aggregated_packet(
    const char *buf, int len)
{       
    int pass         = 0;                      
    int total_length = 0;                      
    unsigned char *dst     = NULL;                   
    int ret;
            
    // first we are going to figure out the total size
    for (pass = 0; pass < 2; pass++) {
        const unsigned char *src = (const unsigned char*)buf;
        int src_len        = len;
        
        while (src_len > 2) {
            //unsigned short nal_size =  ntohs(*((unsigned short*)src));
            unsigned short nal_size = amt_av_rb16(src);
        
            // consume the length of the aggregate
            src     += 2;
            src_len -= 2;

            if (nal_size <= src_len) {
                if (pass == 0) {
                    // counting
                    total_length += sizeof(psc) + nal_size;
                } else {
                    // copying
                    memcpy(m_pBuf + m_nPutPos,psc,sizeof(psc));
                    m_nPutPos += sizeof(psc);
                    memcpy(m_pBuf + m_nPutPos, src, nal_size);
                    m_nPutPos += nal_size;
                }
            } else {
                fprintf(stderr,
                       "nal size exceeds length: %d %d\n", nal_size, src_len);
                return false;
            }

            // eat what we handled
            src     += nal_size;//if use donl field , need add donl field size here
            src_len -= nal_size;//if use donl field , need add donl field size here
        }
    }
    return true;
}


////////////////////////////////////////////////////////////////////
CHAVStreamConv* CHAVStreamConv::CreateNew(PAVCODEC nCodecType, 
                                          STREAM_TYPE nStreamFrom, 
                                          STREAM_TYPE nStreamTo, 
                                          int nFrameAccumCount)
{
   CHAVStreamConv *rc = NULL;
   if(nStreamFrom == nStreamTo) {
      rc = new CHAVStreamConv;
   }
   switch(nCodecType) {
#if 0
      case CTI_H265: 
         if(nStreamFrom == ST_RTP && nStreamTo == ST_BYTE) {
            rc = new CHAVRtpToH265;
         } else
         if(nStreamFrom == ST_BYTE && nStreamTo == ST_RTP) {
            rc = new CHAVH265ToRtp;
         }
         break;
#endif
      case PAVCODEC_H264: 
         if(nStreamFrom == ST_RTP && nStreamTo == ST_BYTE) {
            rc = new CHAVRtpToH264;
            //rc = new CHAVRtpToH264Bs;
         } else
         if(nStreamFrom == ST_BYTE && nStreamTo == ST_RTP) {
            rc = new CHAVH264ToRtp;
         }
         break;
#if 0
      case CTI_JPEG: 
         if(nStreamFrom == ST_BYTE && nStreamTo == ST_RTP) {
            rc = new CHAVJPEGBsToRtp;
         }
         break;
// 2011.06.30 AMRWB Add.
#endif 
      case PAVCODEC_AMR:
      case PAVCODEC_AMRWB:
         if(nStreamFrom == ST_RTP && nStreamTo == ST_BYTE) {
            rc = new CHAVRtpToAMRBs;
         } else
         if(nStreamFrom == ST_BYTE && nStreamTo == ST_RTP) {
            rc = new CHAVAMRBsToRtp;
         }
         break;
      default : //PCM
         rc = new CHAVStreamConv;
         break;
   }
   if(rc != NULL) {
      rc->m_nAccumCount = nFrameAccumCount;
   };
   return rc;
}

CHAVStreamConv::CHAVStreamConv() {
   m_pBuf = NULL;
   Clear();
}

CHAVStreamConv::~CHAVStreamConv() {
   Clear();
}

//bool CHAVStreamConv::Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode,int nNalLengthSize) {
bool CHAVStreamConv::Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode) {
   Clear();
   m_nMaxOutputSize = nMaxOutputSize;// - 10; // 10:safety.

   m_nPacketMode = nPktMode;
   PrepareBuffer(nMaxOutputSize);
   return true;
}

bool CHAVStreamConv::Close() {
   Clear();
   return true;
}

bool CHAVStreamConv::Clear() {
   if(m_pBuf != NULL) {
      delete[] m_pBuf;
      m_pBuf = NULL;
   }
   m_nMaxOutputSize = 0;
   m_bFragEnd = false;
   m_nBufLen = 0;
   m_nPutPos = 0;
   m_nGetPos = 0;
   m_nFragIndex = 0;
   m_nFramesPut = 0;
   return true;
}

bool CHAVStreamConv::PrepareBuffer(int nLength)
{
   if(m_nPutPos + nLength > m_nBufLen) {
      nLength = (nLength + 255) & (~255); // multiple of 256
      unsigned char* p = new unsigned char[m_nPutPos + nLength];
      memcpy(p, m_pBuf, m_nPutPos);
      if(m_pBuf) delete[] m_pBuf;
      m_pBuf = p;
      m_nBufLen = m_nPutPos + nLength;
   }
   return true;
}

bool CHAVStreamConv::Put(bool bFragEnd, unsigned char* pData, int nLength) {
	if(nLength < 0) {
		return false;
   }
   m_nFragIndex = 0;
   if(m_bFragEnd) {
      m_nGetPos = 0;
      m_nPutPos = 0;
      m_nFramesPut = 0;
   }
   PrepareBuffer(nLength);
   memcpy(m_pBuf + m_nPutPos, pData, nLength);
   m_nPutPos += nLength;
   if(bFragEnd) {
      m_nFramesPut++;
   }
   m_bFragEnd = bFragEnd && (m_nFramesPut == m_nAccumCount);
   return true;
}

bool CHAVStreamConv::TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd) {
   if(m_nPutPos == 0) {
      return false;  
   }
   if(m_bFragEnd == false) {
      return false;  
   }

   int nMaxOutSize = (nInLength < m_nMaxOutputSize) ? nInLength : m_nMaxOutputSize;
   *pnFragEnd = (m_nPutPos - m_nGetPos <= nMaxOutSize) ? 1 : 0;

   if(*pnFragEnd) {
      *pnOutLength = m_nPutPos - m_nGetPos;
      memcpy(pData, m_pBuf + m_nGetPos, m_nPutPos - m_nGetPos);
      m_nPutPos = 0;
      m_nGetPos = 0;
   } else {
      *pnOutLength = nMaxOutSize;
      memcpy(pData, m_pBuf + m_nGetPos, nMaxOutSize);
      m_nGetPos += nMaxOutSize;
   }
   m_nFragIndex++;
   return true;
}

void CHAVStreamConv::SetNalLengthSize(unsigned char *pExtraData)
{

}


// 2011.07.01 AMR-WB
CHAVAMRBsToRtp::CHAVAMRBsToRtp() {
}

CHAVAMRBsToRtp::~CHAVAMRBsToRtp() {
}

bool CHAVAMRBsToRtp::Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode) {
   if( (nPktMode != PM_AMRNB_BAND) && (nPktMode != PM_AMRWB_BAND)) {
      nPktMode = PM_AMR_OCTET;
   } 
	return CHAVStreamConv::Open(nMaxOutputSize, nPktMode);
}

/*
RFC3267 4.4
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | CMR=6 |R|R|R|R|1|FT#1=5 |Q|P|P|0|FT#2=5 |Q|P|P|   f1(0..7)    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   f1(8..15)   |  f1(16..23)   |  ....                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         ...   |f1(152..158) |P|   f2(0..7) ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

RFC3267 5.3
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |P| FT=2  |Q|P|P|f(0..7) ...                                    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool CHAVAMRBsToRtp::Put(bool bFragEnd, unsigned char* pData, int nLength)
{  
	bool bRes;

	// RFC3267_53 -> 44, File format->RTP format;
	// octet align
	if (m_nPacketMode == PM_AMR_OCTET) 
	{
		memmove(pData+1, pData, nLength);
		nLength++;
		pData[0] = 0x0f << 4;
	}
	else // bandwidth efficient 
	{
		unsigned short header = 0x0;
      int nBitCount = 0;
      int nFrameType;
      nFrameType = (pData[0] >> 3) & 0xf;
		if (m_nPacketMode == PM_AMRNB_BAND)
      	nBitCount = AMR_BITS[nFrameType];
		else if (m_nPacketMode == PM_AMRWB_BAND)
      	nBitCount = AMRWB_BITS[nFrameType];
		else
			return false;

      unsigned char pDataBuf1[100]; 

		memset(&pDataBuf1, 0, 100);
		header = (0x0f << 12) | (0 << 11) | (nFrameType << 7) | (1 << 6);
		pDataBuf1[0] = header >> 8;
		pDataBuf1[1] = header & 0x00ff;

		for(int i=0;i<nBitCount;i++)
		{
			if (pData[1+(i>>3)] & BIT_IN_BYTE_R[i & 0x07])
				pDataBuf1[(i+10)>>3] |= 1 << (~(i+10) & 0x07);
		}
		memcpy(pData, pDataBuf1, nLength);
	}

   bRes = CHAVStreamConv::Put(true, pData, nLength);
	return bRes;
}

// AMRWB Rtp to Bytes
CHAVRtpToAMRBs::CHAVRtpToAMRBs() {
}

CHAVRtpToAMRBs::~CHAVRtpToAMRBs() {
}

bool CHAVRtpToAMRBs::Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode) {
   if( (nPktMode != PM_AMRNB_BAND) && (nPktMode != PM_AMRWB_BAND)) {
      nPktMode = PM_AMR_OCTET;
   } 
	return CHAVStreamConv::Open(nMaxOutputSize, nPktMode);
}


bool CHAVRtpToAMRBs::Put(bool bFragEnd, unsigned char* pData, int nLength)
{  
	if(nLength <= 1) {
		return false;
   }
	bool bRes;
	// RFC3267_44 -> 53, RTP -> File format;
	// octet align
	if (m_nPacketMode == PM_AMR_OCTET) 
	{
		nLength = nLength - 1;
	
		memmove(pData, pData+1, nLength);
	}
	else // bandwidth efficient
	{
		unsigned short header = 0x0;
		int nBitCount;
		int nFrameType;
		unsigned char pDataBuf1[100];
		memset(&pDataBuf1, 0, 100);

		header = pData[0] << 8;
		header |= pData[1];

		nFrameType = (header & 0x0780) >> 7;
		if (m_nPacketMode == PM_AMRNB_BAND)
			nBitCount = AMR_BITS[nFrameType];
		else if (m_nPacketMode == PM_AMRWB_BAND)
			nBitCount = AMRWB_BITS[nFrameType];
		else
			return false;
		pDataBuf1[0] = (nFrameType << 3) | (0x01 << 2);
		for(int i=0;i<nBitCount;i++)
		{
			if (pData[(i+10)>>3] & BIT_IN_BYTE_R[(i+10) & 0x07] )
				pDataBuf1[1+(i>>3)] |= 1 << (~i & 0x07);
		}
		memcpy(pData, pDataBuf1, nLength); 
	}

   bRes = CHAVStreamConv::Put(true, pData, nLength);
	return bRes;
}

CHAVH264ToRtp::CHAVH264ToRtp() {
	m_pNalBuffer = NULL;
}

CHAVH264ToRtp::~CHAVH264ToRtp() {
	if(m_pNalBuffer != NULL) {
		delete[] m_pNalBuffer;
		m_pNalBuffer = NULL;
	}
}

bool CHAVH264ToRtp::Clear() {
   CHAVStreamConv::Clear();
   ClearPacketList();
   //m_nNalLengthSize = 0;
   m_nExtraSize = 0;
   // 2021.01.21
   m_unbuffered_size = 0;
	m_unbuffered_nals = 0;
	memset(m_pNalBuffer, 0, m_nMaxOutputSize);
   return true;
}

bool CHAVH264ToRtp::Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode) {
   if(nPktMode == PM_DEF) {
      nPktMode = PM_H264_SINGLE;
   }
//fprintf(stderr,"pktmode = %d\n",nPktMode); 
   //m_nNalLengthSize = nNalLengthSize;
   //m_nNalLengthSize = 4;
   m_nNalLengthSize = 0;

   if(nPktMode == PM_H264_NON_INT || nPktMode == PM_H264_SINGLE || nPktMode == PM_DEF_AP)
   {
		if(m_pNalBuffer != NULL) {
			delete[] m_pNalBuffer;
			m_pNalBuffer = NULL;
		}
		m_pNalBuffer = new unsigned char[nMaxOutputSize];

      return CHAVStreamConv::Open(nMaxOutputSize, nPktMode);
   }
   return false;
}

void CHAVH264ToRtp::ClearPacketList()
{
   int n = m_PacketList.size();
   for(int i=0; i<n; i++) {
      if(m_PacketList[i]) delete (PLINFO*)m_PacketList[i];
   }
   m_PacketList.clear();
   m_unbuffered_size = 0;
	m_unbuffered_nals = 0;
	memset(m_pNalBuffer, 0, m_nMaxOutputSize);
}

void CHAVH264ToRtp::AddPacket(unsigned char *buf,int size,int last)
{
//fprintf(stderr,"%s size:%d last:%d\n",__func__,size,last);
   PLINFO * pInfo;
   unsigned int nRemainSize=size;
   unsigned int header_size = 2;
   unsigned char * p;
   bool bBegin = true;
   bool bEnd = false;
   if(size <= m_nMaxOutputSize)
   {
      // 2021.01.21 minimei7 for AP mode
		if (m_nPacketMode == PM_DEF_AP)
		{
         if (m_unbuffered_size + size > m_nMaxOutputSize)
         {
            printf("TEST55 buf(size:%d nal:%d) size:%d\n", m_unbuffered_size, m_unbuffered_nals, size);
            FlushBuffered();
         }
         else
         {
            printf("TEST54 buf(size:%d nal:%d) size:%d\n", m_unbuffered_size, m_unbuffered_nals, size);
            // len+2
            AV_WB16(m_pNalBuffer+m_unbuffered_size, size);
            m_unbuffered_size += 2;
            memcpy(&m_pNalBuffer[m_unbuffered_size], buf, size);
            m_unbuffered_size += size;
            m_unbuffered_nals++;
            printf("TEST54-1 %02X %02X %02X %02X %02X\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
            printf("TEST54-2 %02X %02X %02X %02X %02X\n", m_pNalBuffer[0], m_pNalBuffer[1], m_pNalBuffer[2], m_pNalBuffer[3], m_pNalBuffer[4]);
         }
		}
		else
		{
			pInfo = new PLINFO;
			pInfo->type = PLT_SINGLE;
			pInfo->single.ptr = buf;
			pInfo->single.len = size;
			m_PacketList.push_back(pInfo);
		}
   }
   else
   {
      // 2021.01.21 minimei7 for AP mode
		if (m_nPacketMode == PM_DEF_AP)
		{
			printf("TEST56 buf(size:%d nal:%d) size:%d\n", m_unbuffered_size, m_unbuffered_nals, size);
			FlushBuffered();
		}
      p = buf;
      while((nRemainSize + (header_size)) > m_nMaxOutputSize)
      {
         pInfo = new PLINFO;
         pInfo->type = PLT_FU;
         pInfo->fu.ptr = p;
         pInfo->fu.len = m_nMaxOutputSize - (header_size);
         pInfo->fu.begin = bBegin;
         pInfo->fu.end = false;
         pInfo->fu.type = buf[0] & 0x1f;
         pInfo->fu.nri = buf[0] & 0x60;
         p += (m_nMaxOutputSize - header_size);
         nRemainSize -= (m_nMaxOutputSize - header_size);
         bBegin = false;
         if(nRemainSize < 1)
         {
            pInfo->fu.end = true;
         }
         m_PacketList.push_back(pInfo);
//fprintf(stderr,"FU size = %d\n",pInfo->fu.len);
      }//while

      if(nRemainSize > 0)
      {
         pInfo = new PLINFO;
         pInfo->type = PLT_FU;
         pInfo->fu.ptr = p;
         pInfo->fu.len = nRemainSize;
         pInfo->fu.begin = bBegin;
         pInfo->fu.end = true;
         pInfo->fu.type = buf[0] & 0x1f;
         pInfo->fu.nri = buf[0] & 0x60;
      }
//fprintf(stderr,"FU END size = %d\n",pInfo->fu.len);
      m_PacketList.push_back(pInfo); 
   }
}

bool CHAVH264ToRtp::Put(bool bFragEnd, unsigned char* pData, int nLength)
{
   bool brc = true;
   brc = CHAVStreamConv::Put(bFragEnd, pData, nLength);
   if(brc == false) {
      return brc;
   }
   if(bFragEnd) {
      ClearPacketList();
      //GetExtraData();
      m_nFragIndex = 0;
   }
#if 0
unsigned char* databuff = pData;
fprintf(stderr,"%s:%s:%d\n"
   "0x%02x-%02x-%02x-%02x %02x-%02x-%02x-%02x %02x-%02x-%02x-%02x %02x-%02x-%02x-%02x\n"
   "0x%02x-%02x-%02x-%02x %02x-%02x-%02x-%02x %02x-%02x-%02x-%02x %02x-%02x-%02x-%02x\n"
   "0x%02x-%02x-%02x-%02x %02x-%02x-%02x-%02x %02x-%02x-%02x-%02x %02x-%02x-%02x-%02x\n",
   __FILE__,__func__,__LINE__,
   databuff[0]&0xff,databuff[1]&0xff,databuff[2]&0xff,databuff[3]&0xff,databuff[4]&0xff,databuff[5]&0xff,databuff[6]&0xff,databuff[7]&0xff,
   databuff[8]&0xff,databuff[9]&0xff,databuff[10]&0xff,databuff[11]&0xff,databuff[12]&0xff,databuff[13]&0xff,databuff[14]&0xff,databuff[15]&0xff,
   databuff[16]&0xff,databuff[17]&0xff,databuff[18]&0xff,databuff[19]&0xff,databuff[20]&0xff,databuff[21]&0xff,databuff[22]&0xff,databuff[23]&0xff,
   databuff[24]&0xff,databuff[25]&0xff,databuff[26]&0xff,databuff[27]&0xff,databuff[28]&0xff,databuff[29]&0xff,databuff[30]&0xff,databuff[31]&0xff,
   databuff[32]&0xff,databuff[33]&0xff,databuff[34]&0xff,databuff[35]&0xff,databuff[36]&0xff,databuff[37]&0xff,databuff[38]&0xff,databuff[39]&0xff,
   databuff[40]&0xff,databuff[41]&0xff,databuff[42]&0xff,databuff[43]&0xff,databuff[44]&0xff,databuff[45]&0xff,databuff[46]&0xff,databuff[47]&0xff);
#endif
   unsigned char *r, *end = pData + nLength;
   unsigned char *buffed_pos=NULL;
   unsigned int buffed_size=0;
   unsigned int remain=0;
   if (m_nNalLengthSize)
   {
//fprintf(stderr,"nal_length_size = %d buf=0x%02x-%02x-%02x-%02x\n",m_nNalLengthSize,pData[0],pData[1],pData[2],pData[3]);
        r = amt_ff_avc_mp4_find_startcode(pData, end, m_nNalLengthSize) ? pData : end;
   }
    else
        r = amt_ff_avc_find_startcode(pData, end);

   //fprintf(stderr,"pkt size =%d\n",end - r);
    unsigned char *r1;
    while (r < end) {

        if (m_nNalLengthSize) {
//fprintf(stderr,"nal_length_size = %d buf=0x%02x-%02x-%02x-%02x\n",m_nNalLengthSize,pData[0],pData[1],pData[2],pData[3]);
            r1 = amt_ff_avc_mp4_find_startcode(r, end, m_nNalLengthSize);
            if (!r1)
                r1 = end;
            r += m_nNalLengthSize;
        } else {
            while (!*(r++));
            r1 = amt_ff_avc_find_startcode(r, end);
        }
//fprintf(stderr,"r=0x%02x-%02x-%02x-%02x size=%d end=%d nal:%x bufpos:%d bufsize:%d\n",r[0],r[1],r[2],r[3],r1-r,r1==end,r[0]&0x1f, buffed_pos,buffed_size);
        AddPacket(r,r1-r,r1==end);
      
#if 0
        if(buffed_pos && ((buffed_size + r1-r) > 1300))
         {
            AddPacket(buffed_pos,buffed_size + r1-r,r1==end);
            buffed_pos = NULL;
            buffed_size = 0;
         }
         else if((r1 -r)>1300)
         {
            if(buffed_pos!=NULL)
            {
               AddPacket(buffed_pos,buffed_size + r1-r,r1==end);
               buffed_pos = NULL;
               buffed_size = 0;
            }
            else
            {
            }
         }
         else
         {
            if(buffed_pos==NULL)
               buffed_pos = r;
            buffed_size += (r1-r); 
fprintf(stderr,"buffed_size=%d\n",buffed_size);
         }
#endif
         r = r1;
   }

   // 2021.01.21 minimei7 for AP mode
   FlushBuffered();

   return brc;
}

bool CHAVH264ToRtp::TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd) {
   if(m_bFragEnd == false) {
      return false;
   }
   bool brc  = false;
   if(m_nFragIndex >= (int)m_PacketList.size()) {
      return false;
   }
   PLINFO *pInfo = (PLINFO *)m_PacketList[m_nFragIndex];

   if(m_nFragIndex == (int)m_PacketList.size()-1) {
      *pnFragEnd = 1;
   } else {
      *pnFragEnd = 0;
   }

//fprintf(stderr,"nal type=%d\n",pInfo->type);
   switch(pInfo->type) {
/*
      case PLT_STAPA  :
         brc = MakeDecInfoPacket(m_nFragIndex, pData, nInLength, pnOutLength);

         break;
*/
      case PLT_SINGLE :
         brc = MakeSinglePacket(m_nFragIndex, pData, nInLength, pnOutLength);
         break;
      case PLT_FU     :
			// 2020.12.04 minimei7 for ffmpeg marker logic
         //brc = MakeFuPacket(m_nFragIndex, pData, nInLength, pnOutLength, pnFragEnd);
         brc = MakeFuPacket(m_nFragIndex, pData, nInLength, pnOutLength);
         break;
      case PLT_STAPA:
         brc = MakeAPPacket(m_nFragIndex, pData, nInLength, pnOutLength);
         break;
/*
      case PLT_SPSPPS  :
         brc = MakeSpsInfoPacket(m_nFragIndex, pData, nInLength, pnOutLength);
         break;
*/
      default: break;
   }
   m_nFragIndex++;

//fprintf(stderr,"# TryGet len=%d, end=%d\n", *pnOutLength, *pnFragEnd);
   return brc;
}

// 2020.10.05 minimei7
void CHAVH264ToRtp::SetNalLengthSize(unsigned char *pExtraData)
{
	//printf("TEST H264tToRtp Before NalLengthSize:%d\n", m_nNalLengthSize);
	int nNalLengthSize = 0;
	// 2021.03.08 minimei7 for AVI file read. avi는 nal len 0으로 설정.
	if (pExtraData[0] == 1)
		nNalLengthSize = (pExtraData[4] & 0x03) + 1;
	else
		printf("H264StreamConv ExtraData[0] NULL. NalLenSize=0\n", nNalLengthSize);

	m_nNalLengthSize = nNalLengthSize;
	printf("TEST H264tToRtp After NalLengthSize:%d\n", m_nNalLengthSize);
}

// 2021.01.21 minimei7 for AP Mode
void CHAVH264ToRtp::FlushBuffered()
{
	if (m_nPacketMode == PM_DEF_AP && m_unbuffered_size != 0)
	{
   	PLINFO * pInfo;
		if (m_unbuffered_nals == 1)
		{
			pInfo = new PLINFO;
			pInfo->type = PLT_SINGLE;
			pInfo->single.ptr = m_pNalBuffer+2; // AP NAL size 2byte remove
			pInfo->single.len = m_unbuffered_size-2; // AP NAL size 2byte remove
      	m_PacketList.push_back(pInfo);
		}
		else
		{
			pInfo = new PLINFO;
			pInfo->type = PLT_STAPA; 
			pInfo->ap.ptr = m_pNalBuffer;
			pInfo->ap.len = m_unbuffered_size;
      	m_PacketList.push_back(pInfo);
		}
		m_unbuffered_size = 0;
		m_unbuffered_nals = 0;
	}
}

void CHAVH264ToRtp::GetExtraData()
{
   //TODO : Need NalLength find logic implement
   //m_nNalLengthSize = 4;
#if 0
   unsigned int nGetPos=0;
   unsigned char * pCurr;
   unsigned char *pPps = NULL;
   unsigned char *pEnd,*pStart;
   int nSpsLen = 0;
   int nPpsLen = 0;
   int nSpsPSC;
   int nPpsPSC;
   int nPSCLen;
   char szSps[128]={0,};
   char szPps[128]={0,};

   pCurr = H264_ScanStartCode(m_pBuf + nGetPos, 0, m_nPutPos - nGetPos, &nPSCLen);
   fprintf(stderr,"LINE:%d pCurr = 0x%x GetPos %d PutPos %d nPSCLen %d\n",__LINE__,pCurr,nGetPos,m_nPutPos,nPSCLen);
   if(pCurr == NULL) {
      m_nNalLengthSize = 0;
      return;
   }
   m_nNalLengthSize = nPSCLen;
#endif

#if 0
   while(nGetPos < m_nPutPos) {
      pCurr = H264_ScanStartCode(m_pBuf + nGetPos, 0, m_nPutPos - nGetPos, &nPSCLen);
      fprintf(stderr,"LINE:%d pCurr = 0x%x GetPos %d PutPos %d nPSCLen %d\n",__LINE__,pCurr,nGetPos,m_nPutPos,nPSCLen);
      if(pCurr == NULL) {
         break;
      }
      nGetPos = pCurr - m_pBuf;
      fprintf(stderr,"LINE:%d pCurr = 0x%x GetPos %d PutPos %d nPSCLen %d\n",__LINE__,pCurr,nGetPos,m_nPutPos,nPSCLen);
      if((pCurr[nPSCLen] & 0x1F) == 7)
      {
         pPps = H264_ScanStartCode(m_pBuf+nGetPos, PSC_LENGTH,m_nPutPos - nGetPos, &nSpsPSC);
         if(pPps == NULL)
         {
            m_nExtraSize = 0;
            return;
         }
         if(pPps[nSpsPSC] & 0x1F != 8)
         {
            m_nExtraSize = 0;
            return;
         }
         nSpsLen = pPps - (m_pBuf+nGetPos);
         pEnd = H264_ScanStartCode(m_pBuf+nGetPos, nSpsLen+nSpsPSC, m_nPutPos - nGetPos, &nPpsPSC);
         if(pEnd == NULL)
         {
            nPpsPSC = 0;
            pEnd = m_pBuf + m_nPutPos;
         }
         nPpsLen = pEnd - pPps;
         m_nExtraSize = 0;
         if(nSpsLen > nSpsPSC)
         {
            memcpy(m_pExtraData,m_pBuf+nGetPos + nSpsPSC, nSpsLen - nSpsPSC);
            m_nExtraSize += (nSpsLen - nSpsPSC);
         }
         if(nPpsLen > nPpsPSC)
         {
            memcpy(m_pExtraData+m_nExtraSize,pPps+ nPpsPSC, nPpsLen - nPpsPSC);
            m_nExtraSize += (nPpsLen - nPpsPSC);
         }
         m_nNalLengthSize = (m_pExtraData[4] & 0x03) + 1;
         
#if 0
         if(nSpsLen > nSpsPSC)
         {
            av_base64_encode(szSps, sizeof(szSps), m_pBuf+nGetPos + nSpsPSC, nSpsLen - nSpsPSC);
         }
         if(nPpsLen > nPpsPSC)
         {
            av_base64_encode(szPps, sizeof(szPps), pPps + nPpsPSC, nPpsLen - nPpsPSC);
         }
         memset(m_pExtraData,0x00,sizeof(m_pExtraData));
         if(szSps[0]!='\0')
            sprintf(m_pExtraData,"%s",szSps);
         if(szPps[0]!='\0')
         {
            strcat(m_pExtraData,",");
            strcat(m_pExtraData,szPps);
         }
         m_nExtraSize = strlen(m_pExtraData);
         m_nNalLengthSize = (m_pExtraData[4] & 0x03) + 1;
#endif
         return;
      }//if
      else
      {
         pStart = pCurr;
         if(pStart == NULL) {
            nPSCLen = 0;
            pStart = m_pBuf + m_nGetPos;
         }
         pEnd = H264_ScanStartCode(m_pBuf + nGetPos, 4, m_nPutPos - nGetPos, NULL);
         if(pEnd == NULL) {
            pEnd = m_pBuf + m_nPutPos;
         }
         nGetPos += pEnd - (m_pBuf + nGetPos);
      }
   }//while
#endif
}

bool CHAVH264ToRtp::MakeSinglePacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength)
{
   PLINFO *pInfo = (PLINFO *)m_PacketList[nIndex];
   unsigned char *buf_ptr = pData;
   if(pInfo->single.len > nInLength) {
      return false;
   }
   //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |F|NRI|  type   |                                               |
   // +-+-+-+-+-+-+-+-+                                               |
   // |               Bytes 2..n of a Single NAL unit                 |
     memcpy(pData, pInfo->single.ptr, pInfo->single.len);
     *pnOutLength = pInfo->single.len;
//fprintf(stderr,"LINE:%d MakeSinglePacket type:0x%x nri:0x%x\n",__LINE__,pInfo->fu.type,pInfo->fu.nri);
   return true;
}

bool CHAVH264ToRtp::MakeAPPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength)
{
   PLINFO *pInfo = (PLINFO *)m_PacketList[nIndex];
   
   if(pInfo->ap.len > nInLength) {
      return false;
   }
   unsigned char *p = pData;

   // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
   // |                         NALU 1 Data                           |
   // :                                                               :
   // +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |               | NALU 2 Size                   | NALU 2 HDR    |

   p[0] = (3 << 5) | 24;

	printf("TEST57 AP len:%d\n", pInfo->ap.len);
   memcpy(&p[1], &pInfo->ap.ptr[0], pInfo->ap.len);

	*pnOutLength = pInfo->ap.len + 2;

   return true;
}

bool CHAVH264ToRtp::MakeFuPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength)
{
   PLINFO *pInfo = (PLINFO *)m_PacketList[nIndex];
   int nOutLen = 2 + pInfo->fu.len;
   if(nOutLen > nInLength) {
      return false;
   }
   //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // | FU indicator  |   FU header   |                               |
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
   // |                         FU payload                            |
   unsigned char *p = pData;

   p[0] = 28; /* FU Indicator */
   p[0] |= pInfo->fu.nri;
   p[1] = pInfo->fu.type;
   p[1] |= 1<<7;
//fprintf(stderr,"LINE:%d MakeFUPacket type:0x%x nri:0x%x\n",__LINE__,pInfo->fu.type,pInfo->fu.nri);
   
   unsigned char flag_byte = 1;
   unsigned char header_size = 2;
   memcpy(&p[header_size],&pInfo->fu.ptr[1],pInfo->fu.len);
   if(pInfo->fu.end)
   {
      p[flag_byte] &= ~(1 << 7);
      p[flag_byte] |= 1 << 6;
//fprintf(stderr,"LINE:%d MakeFUPacket header = x0%x-%x\n",__LINE__,p[0],p[1]);
   }
   else if(!pInfo->fu.begin)
   {
      p[flag_byte] &= ~(1 << 7);
//fprintf(stderr,"LINE:%d MakeFUPacket header = x0%x-%x\n",__LINE__,p[0],p[1]);
   }
   *pnOutLength = nOutLen;
   //*pnFragEnd = pInfo->fu.end?1:0;
//fprintf(stderr,"LINE:%d MakeFUPacket header = x0%x-%x outlen:%d\n",__LINE__,p[0],p[1],nOutLen);
   return true;
}
//////////////////////////////////////////////////////////////////////////////////////
// CHAVH264ToRtp
//////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////
// CHAVH264ToRtp
//////////////////////////////////////////////////////////////////////////////////////

CHAVH265ToRtp::CHAVH265ToRtp() {

}

CHAVH265ToRtp::~CHAVH265ToRtp() {

}

bool CHAVH265ToRtp::Clear() {
   CHAVStreamConv::Clear();
   ClearPacketList();
   //m_nNalLengthSize = 0;
   //m_nExtraSize = 0;
   return true;
}

//bool CHAVH265ToRtp::Open(int nMaxOutputSize,int nNalLengthSize) {
bool CHAVH265ToRtp::Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode) {
   //m_nNalLengthSize = nNalLengthSize;
   m_nNalLengthSize = 4;
   return CHAVStreamConv::Open(nMaxOutputSize,PM_DEF);
}

void CHAVH265ToRtp::ClearPacketList()
{
   int n = m_PacketList.size();
   for(int i=0; i<n; i++) {
      if(m_PacketList[i]) delete (PLINFO*)m_PacketList[i];
   }
   m_PacketList.clear();
}

void CHAVH265ToRtp::AddPacket(unsigned char *buf,int size,int last)
{
//fprintf(stderr,"%s size:%d last:%d\n",__func__,size,last);
   PLINFO * pInfo;
   unsigned int nRemainSize=size;
	// 2020.10 header_size는 fu type에서만 사용하므로 3으로 고정.
   //unsigned int header_size = 2;
   unsigned int header_size = 3;
   unsigned char * p;
   bool bBegin = true;
   bool bEnd = false;
   if(size <= m_nMaxOutputSize)
   {
      pInfo = new PLINFO;
      pInfo->type = PLT_SINGLE;
      pInfo->single.ptr = buf;
      pInfo->single.len = size;
      m_PacketList.push_back(pInfo);
   }
   else
   {
      p = buf;
      while((nRemainSize + (header_size)) > m_nMaxOutputSize)
      {
         pInfo = new PLINFO;
         pInfo->type = PLT_HEVC_FU;
         pInfo->hevc_fu.ptr = p;
         pInfo->hevc_fu.len = m_nMaxOutputSize - (header_size);
         pInfo->hevc_fu.begin = bBegin;
         pInfo->hevc_fu.end = false;
         pInfo->hevc_fu.nal_type = (buf[0] >> 1) & 0x3F;
         p += (m_nMaxOutputSize - header_size);
         nRemainSize -= (m_nMaxOutputSize - header_size);
         bBegin = false;
         if(nRemainSize < 1)
         {
            pInfo->hevc_fu.end = true;
         }
         m_PacketList.push_back(pInfo);
      }//while

      if(nRemainSize > 0)
      {
         pInfo = new PLINFO;
         pInfo->type = PLT_HEVC_FU;
         pInfo->hevc_fu.ptr = p;
         pInfo->hevc_fu.len = nRemainSize;
         pInfo->hevc_fu.begin = bBegin;
         pInfo->hevc_fu.end = true;
         pInfo->hevc_fu.nal_type = (buf[0] >> 1) & 0x3F;
      }
//fprintf(stderr,"FU END size = %d\n",pInfo->fu.len);
      m_PacketList.push_back(pInfo); 
   }
}

bool CHAVH265ToRtp::Put(bool bFragEnd, unsigned char* pData, int nLength)
{
   bool brc = true;
   brc = CHAVStreamConv::Put(bFragEnd, pData, nLength);
   if(brc == false) {
      return brc;
   }
   if(bFragEnd) {
      ClearPacketList();
      //GetExtraData();
      m_nFragIndex = 0;
   }
   unsigned char *r, *end = pData + nLength;
   unsigned char *buffed_pos=NULL;
   unsigned int buffed_size=0;
   unsigned int remain=0;
   if (m_nNalLengthSize)
   {
//fprintf(stderr,"nal_length_size = %d buf=0x%02x-%02x-%02x-%02x\n",m_nNalLengthSize,pData[0],pData[1],pData[2],pData[3]);
        r = amt_ff_avc_mp4_find_startcode(pData, end, m_nNalLengthSize) ? pData : end;
   }
    else
        r = amt_ff_avc_find_startcode(pData, end);
//   fprintf(stderr,"pkt size =%d\n",end - r);
        unsigned char *r1;
    while (r < end) {

        if (m_nNalLengthSize) {
//fprintf(stderr,"nal_length_size = %d buf=0x%02x-%02x-%02x-%02x\n",m_nNalLengthSize,pData[0],pData[1],pData[2],pData[3]);
            r1 = amt_ff_avc_mp4_find_startcode(r, end, m_nNalLengthSize);
            if (!r1)
                r1 = end;
            r += m_nNalLengthSize;
        } else {
            while (!*(r++));
            r1 = amt_ff_avc_find_startcode(r, end);
        }
//fprintf(stderr,"r=0x%02x-%02x-%02x-%02x size=%d end=%d nal:%x bufpos:%d bufsize:%d\n",r[0],r[1],r[2],r[3],r1-r,r1==end,r[0]&0x1f, buffed_pos,buffed_size);
        AddPacket(r,r1-r,r1==end);
        r = r1;
   }
   return brc;
}

bool CHAVH265ToRtp::TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd) {
   if(m_bFragEnd == false) {
      return false;
   }
   bool brc  = false;
   if(m_nFragIndex >= (int)m_PacketList.size()) {
      return false;
   }
   PLINFO *pInfo = (PLINFO *)m_PacketList[m_nFragIndex];

   if(m_nFragIndex == (int)m_PacketList.size()-1) {
      *pnFragEnd = 1;
   } else {
      *pnFragEnd = 0;
   }

//fprintf(stderr,"nal type=%d\n",pInfo->type);
   switch(pInfo->type) {
      case PLT_SINGLE :
         brc = MakeSinglePacket(m_nFragIndex, pData, nInLength, pnOutLength);
         break;
      case PLT_HEVC_FU     :
			// 2020.12.04 minimei7 for ffmpeg marker logic
         //brc = MakeFuPacket(m_nFragIndex, pData, nInLength, pnOutLength, pnFragEnd);
         brc = MakeFuPacket(m_nFragIndex, pData, nInLength, pnOutLength);
         break;
      default: break;
   }
   m_nFragIndex++;

   return brc;
}

// 2020.10.05 minimei7
void CHAVH265ToRtp::SetNalLengthSize(unsigned char *pExtraData)
{
	//printf("TEST H265tToRtp Before NalLengthSize:%d\n", m_nNalLengthSize);
	int nNalLengthSize = 0;
	nNalLengthSize = (pExtraData[21] & 0x03) + 1;
	m_nNalLengthSize = nNalLengthSize;
	//printf("TEST H265tToRtp After NalLengthSize:%d\n", m_nNalLengthSize);
}


void CHAVH265ToRtp::GetExtraData()
{
   //TODO : Need NalLength find logic implement
   //m_nNalLengthSize = 4;

}

bool CHAVH265ToRtp::MakeSinglePacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength)
{
   PLINFO *pInfo = (PLINFO *)m_PacketList[nIndex];
   
   if(pInfo->single.len > nInLength) {
      return false;
   }
   /*
    * create the HEVC payload header for Single Packet (SP)
    *
    *    0                   1
    *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |F|   Type    |  LayerId  | TID |
    *   +-------------+-----------------+
    *
    *      F       = 0
    *      Type    = 39 (Single nal Packet (SP))
    *      LayerId = 0
    *      TID     = 1
    */
    //agrregation packet not implement so, not add payload header, just send payload

     memcpy(pData, pInfo->single.ptr, pInfo->single.len);
     *pnOutLength = pInfo->single.len;


//fprintf(stderr,"LINE:%d MakeSinglePacket type:0x%x nri:0x%x\n",__LINE__,pInfo->fu.type,pInfo->fu.nri);
   return true;
}

bool CHAVH265ToRtp::MakeFuPacket(int nIndex, unsigned char* pData, int nInLength, int *pnOutLength)
{
   PLINFO *pInfo = (PLINFO *)m_PacketList[nIndex];
   //int nOutLen = 2 + pInfo->hevc_fu.len;
   int nOutLen = 3 + pInfo->hevc_fu.len;
   if(nOutLen > nInLength) {
      return false;
   }
   /*
    * create the HEVC payload header and transmit the buffer as fragmentation units (FU)
    *
    *    0                   1
    *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |F|   Type    |  LayerId  | TID |
    *   +-------------+-----------------+
    *
    *      F       = 0
    *      Type    = 49 (fragmentation unit (FU))
    *      LayerId = 0
    *      TID     = 1
    */
   unsigned char *p = pData;

   p[0] = 49 << 1;
   p[1] = 1;
   /*
    *     create the FU header
    *
    *     0 1 2 3 4 5 6 7
    *    +-+-+-+-+-+-+-+-+
    *    |S|E|  FuType   |
    *    +---------------+
    *
    *       S       = variable
    *       E       = variable
    *       FuType  = NAL unit type
    */ 
   p[2] = pInfo->hevc_fu.nal_type;
   p[2] |= 1 << 7;
//fprintf(stderr,"LINE:%d MakeFUPacket type:0x%x nri:0x%x\n",__LINE__,pInfo->fu.type,pInfo->fu.nri);
   
   unsigned char flag_byte = 2;
   unsigned char header_size = 3;
   memcpy(&p[header_size],&pInfo->hevc_fu.ptr[flag_byte],pInfo->hevc_fu.len);
   if(pInfo->hevc_fu.end)
   {
      p[flag_byte] &= ~(1 << 7);
      p[flag_byte] |= 1 << 6;
//fprintf(stderr,"LINE:%d MakeFUPacket header = x0%x-%x\n",__LINE__,p[0],p[1]);
   }
   else if(!pInfo->hevc_fu.begin)
   {
      p[flag_byte] &= ~(1 << 7);
//fprintf(stderr,"LINE:%d MakeFUPacket header = x0%x-%x\n",__LINE__,p[0],p[1]);
   }
   *pnOutLength = nOutLen;
   //*pnFragEnd = pInfo->hevc_fu.end?1:0;
//fprintf(stderr,"LINE:%d MakeFUPacket header = x0%x-%x outlen:%d\n",__LINE__,p[0],p[1],nOutLen);
   return true;
}

//////////////////////////////////////////////////////////////////////////////////////
// CHAVRtpToH264
//////////////////////////////////////////////////////////////////////////////////////

CHAVRtpToH264::CHAVRtpToH264()
{
   
}

CHAVRtpToH264::~CHAVRtpToH264() 
{
   
}

bool CHAVRtpToH264::Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode) {
   if(nPktMode == PM_DEF) {
      nPktMode = PM_H264_SINGLE;
   } 
   
   if(nPktMode == PM_H264_NON_INT 
      || nPktMode == PM_H264_SINGLE) 
   {
      return CHAVStreamConv::Open(nMaxOutputSize, nPktMode);
   }
   return false;
}

bool CHAVRtpToH264::Clear() 
{
   CHAVStreamConv::Clear();
   m_nFragLeft = 0;
   return true;
}

int CHAVRtpToH264::ff_h264_handle_frag_packet(const unsigned char *buf, int len,
                               int start_bit, const unsigned char *nal_header,
                               int nal_header_len)
{
    int ret;
    int tot_len = len;
    int pos = 0;
    if (start_bit)
        tot_len += sizeof(psc) + nal_header_len;

    PrepareBuffer(tot_len);
    if (start_bit) {
        memcpy(m_pBuf + m_nPutPos,psc,sizeof(psc));
        m_nPutPos += sizeof(psc);
        memcpy(m_pBuf + m_nPutPos,nal_header, nal_header_len);
        m_nPutPos += nal_header_len;
    }
    memcpy(m_pBuf + m_nPutPos, buf, len);
    m_nPutPos += len; 
//fprintf(stderr,"type:2 PutPos %d len:%d\n",m_nPutPos,start_bit?len+sizeof(psc)+nal_header_len:len);
    return 1;
}

int CHAVRtpToH264::h264_handle_packet_fu_a(bool bFragEnd, unsigned char* pData, int nLength)
{
    unsigned char fu_indicator, fu_header, start_bit, nal_type, nal;

    if (nLength < 3) {
        //fprintf(stderr,"Too short data for FU-A H.264 RTP packet\n");
        return false;
    }

    fu_indicator = pData[0];
    fu_header    = pData[1];
    start_bit    = fu_header >> 7;
    nal_type     = fu_header & 0x1f;
    nal          = fu_indicator & 0xe0 | nal_type;

    // skip the fu_indicator and fu_header
    pData += 2;
    nLength -= 2;

    return ff_h264_handle_frag_packet(pData, nLength, start_bit, &nal, 1);
}

bool CHAVRtpToH264::Put(bool bFragEnd, unsigned char* pData, int nLength)
{
   if(m_nGetPos >= m_nPutPos) {
      m_nGetPos = 0;
      m_nPutPos = 0;
   }
   m_nFragLeft = 0;
   m_bFragEnd = bFragEnd;
   bool result = true; 
   unsigned nType = (*pData) & 0x1F;

   unsigned char *buf = pData;
   int len = nLength;


//fprintf(stderr, "# RTPRX-PUT nal_unit_type = %d/0x%x\n",  nType, *pData);
   if(nType >=1 && nType <=23)
      nType = 1;
   switch(nType)
   {
      case 0:
      case 1:
         PrepareBuffer(sizeof(psc) + nLength);
         memcpy(m_pBuf + m_nPutPos,psc,sizeof(psc));
         memcpy(m_pBuf + m_nPutPos + sizeof(psc), pData, nLength);
         m_nPutPos += sizeof(psc) + nLength;
//fprintf(stderr,"type:1 PutPos %d len:%d\n",m_nPutPos,sizeof(psc)+nLength);
		 break;
      case 24:
		 buf++;
		 len--;
		 result = ff_h264_handle_aggregated_packet((const char*)buf, len);
		 if (result < 0)
		 {
			printf("StreamConv RtpToH264 STAP-A error\n");
			break;
		 }
		 break;
		 
		 break;
      case 28:
//fprintf(stderr,"type:2 PutPos %d len:%d\n",m_nPutPos,nLength);
         result = h264_handle_packet_fu_a(bFragEnd,pData,nLength);
      break;
      default:
		 printf("StreamConv RtpToH264 unknwon type:%d\n", nType);
      break;//switch
   }
   return result;
}

bool CHAVRtpToH264::TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd)
{
   if(m_bFragEnd == false) {
      return false;   
   }
   if(m_nGetPos > m_nPutPos) {
      return false;   
   }   
   int nMaxOutSize = (nInLength < m_nMaxOutputSize) ? nInLength : m_nMaxOutputSize;
   m_nFragLeft = m_nPutPos - m_nGetPos;
   if(m_nFragLeft > 0) {
      if(m_nFragLeft > nMaxOutSize) {
         memcpy(pData, m_pBuf + m_nGetPos, nMaxOutSize);
         *pnOutLength = nMaxOutSize;
         *pnFragEnd = 0;
         m_nFragLeft -= *pnOutLength;
         m_nGetPos += *pnOutLength;
      } else {
         memcpy(pData, m_pBuf + m_nGetPos, m_nFragLeft);
         *pnOutLength =  m_nFragLeft;
         *pnFragEnd = 1;
         m_bFragEnd = false;
         m_nFragLeft = 0;
         m_nPutPos = m_nGetPos = 0;
      }
//fprintf(stderr, "# RTPRX-GET size = %d\n",  *pnOutLength);
      return true;
   }
//fprintf(stderr, "# RTPRX-GET failed\n");
   return false;
}

//////////////////////////////////////////////////////////////////////////////////////
// CHAVRtpToH265
//////////////////////////////////////////////////////////////////////////////////////

CHAVRtpToH265::CHAVRtpToH265()
{
   
}

CHAVRtpToH265::~CHAVRtpToH265() 
{
   
}

bool CHAVRtpToH265::Open(int nMaxOutputSize, PACKETIZATION_MODE nPktMode) {
   return CHAVStreamConv::Open(nMaxOutputSize, PM_DEF);
}

bool CHAVRtpToH265::Clear() 
{
   CHAVStreamConv::Clear();
   m_nFragLeft = 0;
   return true;
}

int CHAVRtpToH265::ff_h264_handle_frag_packet(const unsigned char *buf, int len,
                               int start_bit, const unsigned char *nal_header,
                               int nal_header_len)
{
    int ret;
    int tot_len = len;
    int pos = 0;
    if (start_bit)
        tot_len += sizeof(psc) + nal_header_len;

    PrepareBuffer(tot_len);
    if (start_bit) {
        memcpy(m_pBuf + m_nPutPos,psc,sizeof(psc));
        m_nPutPos += sizeof(psc);
        memcpy(m_pBuf + m_nPutPos,nal_header, nal_header_len);
        m_nPutPos += nal_header_len;
    }
    memcpy(m_pBuf + m_nPutPos, buf, len);
    m_nPutPos += len; 
//fprintf(stderr,"type:2 PutPos %d len:%d\n",m_nPutPos,start_bit?len+sizeof(psc)+nal_header_len:len);
    return 1;
}

int CHAVRtpToH265::h264_handle_packet_fu_a(bool bFragEnd, unsigned char* pData, int nLength)
{
    unsigned char fu_indicator, fu_header, start_bit, nal_type, nal;

    if (nLength < 3) {
        //fprintf(stderr,"Too short data for FU-A H.264 RTP packet\n");
        return false;
    }

    fu_indicator = pData[0];
    fu_header    = pData[1];
    start_bit    = fu_header >> 7;
    nal_type     = fu_header & 0x1f;
    nal          = fu_indicator & 0xe0 | nal_type;

    // skip the fu_indicator and fu_header
    pData += 2;
    nLength -= 2;

    return ff_h264_handle_frag_packet(pData, nLength, start_bit, &nal, 1);
}

bool CHAVRtpToH265::Put(bool bFragEnd, unsigned char* pData, int nLength)
{
   if(m_nGetPos >= m_nPutPos) {
      m_nGetPos = 0;
      m_nPutPos = 0;
   }
   m_nFragLeft = 0;
   m_bFragEnd = bFragEnd;
   const unsigned char *rtp_pl = pData;
   bool result = true; 
   int tid, lid, nal_type;
   int first_fragment, last_fragment, fu_type;
   unsigned char new_nal_header[2];
   unsigned char * buf = pData;
   int len = nLength;
 /*
  * decode the HEVC payload header according to section 4 of draft version 6:
  *
  *    0                   1
  *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |F|   Type    |  LayerId  | TID |
  *   +-------------+-----------------+
  *
  *      Forbidden zero (F): 1 bit
  *      NAL unit type (Type): 6 bits
  *      NUH layer ID (LayerId): 6 bits
  *      NUH temporal ID plus 1 (TID): 3 bits
  */ 
   
   nal_type =  (buf[0] >> 1) & 0x3f;
   lid  = ((buf[0] << 5) & 0x20) | ((buf[1] >> 3) & 0x1f);
   tid  =   buf[1] & 0x07;

    /* sanity check for correct NAL unit type */
    if (nal_type > 50) {
        //fprintf(stderr, "Unsupported (HEVC) NAL type (%d)\n", nal_type);
        return false;
    }

    switch (nal_type) {
    /* video parameter set (VPS) */
    case 32:
    /* sequence parameter set (SPS) */
    case 33:
    /* picture parameter set (PPS) */
    case 34:
    /*  supplemental enhancement information (SEI) */
    case 39:
    /* single NAL unit packet */
    default:
      PrepareBuffer(sizeof(psc) + nLength);
      memcpy(m_pBuf + m_nPutPos,psc,sizeof(psc));
      memcpy(m_pBuf + m_nPutPos + sizeof(psc), pData, nLength);
      m_nPutPos += sizeof(psc) + nLength;
        break;
    /* aggregated packet (AP) - with two or more NAL units */
    case 48:
        /* pass the HEVC payload header */
        buf += 2;//RTP_HEVC_PAYLOAD_HEADER_SIZE;
        len -= 2;//RTP_HEVC_PAYLOAD_HEADER_SIZE;

/* no use DONL field, default set
        //pass the HEVC DONL field
        if (rtp_hevc_ctx->using_donl_field) {
            buf += RTP_HEVC_DONL_FIELD_SIZE;
            len -= RTP_HEVC_DONL_FIELD_SIZE;
        }
*/
        result = ff_h264_handle_aggregated_packet((const char*)buf, len);
        if (result < 0)
        break;
   /* fragmentation unit (FU) */
    case 49:
        /* pass the HEVC payload header */
        buf += 2;//RTP_HEVC_PAYLOAD_HEADER_SIZE;
        len -= 2;//RTP_HEVC_PAYLOAD_HEADER_SIZE;

        /*
         *    decode the FU header
         *
         *     0 1 2 3 4 5 6 7
         *    +-+-+-+-+-+-+-+-+
         *    |S|E|  FuType   |
         *    +---------------+
         *
         *       Start fragment (S): 1 bit
         *       End fragment (E): 1 bit
         *       FuType: 6 bits
         */
        first_fragment = buf[0] & 0x80;
        last_fragment  = buf[0] & 0x40;
        fu_type        = buf[0] & 0x3f;

        /* pass the HEVC FU header */
        buf += 1;//RTP_HEVC_FU_HEADER_SIZE;
        len -= 1;//RTP_HEVC_FU_HEADER_SIZE;
/* no use donl filed, default
        //pass the HEVC DONL field 
        if (rtp_hevc_ctx->using_donl_field) {
            buf += RTP_HEVC_DONL_FIELD_SIZE;
            len -= RTP_HEVC_DONL_FIELD_SIZE;
        }
*/

        //fprintf(stderr, " FU type %d with %d bytes\n", fu_type, len);

        /* sanity check for size of input packet: 1 byte payload at least */
        if (len <= 0) {
          //fprintf(stderr,
                 //"Too short RTP/HEVC packet, got %d bytes of NAL unit type %d\n",
                 //len, nal_type);
            return false;
        }

        if (first_fragment && last_fragment) {
            //fprintf(stderr, "Illegal combination of S and E bit in RTP/HEVC packet\n");
            return false;
        }

        new_nal_header[0] = (rtp_pl[0] & 0x81) | (fu_type << 1);
        new_nal_header[1] = rtp_pl[1];

        result = ff_h264_handle_frag_packet(buf, len, first_fragment,
            new_nal_header, sizeof(new_nal_header));

        break;
    /* PACI packet */
    case 50:
        /* Temporal scalability control information (TSCI) */
        //fprintf(stderr, "not implement PACI packets for RTP/HEVC\n");
        return false;
        break;
    }
   return result;
}

bool CHAVRtpToH265::TryGet(unsigned char* pData, int nInLength, int *pnOutLength, int *pnFragEnd)
{
   if(m_bFragEnd == false) {
      return false;   
   }
   if(m_nGetPos > m_nPutPos) {
      return false;   
   }   
   int nMaxOutSize = (nInLength < m_nMaxOutputSize) ? nInLength : m_nMaxOutputSize;
   m_nFragLeft = m_nPutPos - m_nGetPos;
   if(m_nFragLeft > 0) {
      if(m_nFragLeft > nMaxOutSize) {
         memcpy(pData, m_pBuf + m_nGetPos, nMaxOutSize);
         *pnOutLength = nMaxOutSize;
         *pnFragEnd = 0;
         m_nFragLeft -= *pnOutLength;
         m_nGetPos += *pnOutLength;
      } else {
         memcpy(pData, m_pBuf + m_nGetPos, m_nFragLeft);
         *pnOutLength =  m_nFragLeft;
         *pnFragEnd = 1;
         m_bFragEnd = false;
         m_nFragLeft = 0;
         m_nPutPos = m_nGetPos = 0;
      }
      return true;
   }
   return false;
}

int CHAVRtpToH265::ff_h264_handle_aggregated_packet(
    const char *buf, int len)
{       
    int pass         = 0;                      
    int total_length = 0;                      
    unsigned char *dst     = NULL;                   
    int ret;
            
    // first we are going to figure out the total size
    for (pass = 0; pass < 2; pass++) {
        const unsigned char *src = (const unsigned char*)buf;
        int src_len        = len;
        
        while (src_len > 2) {
            //unsigned short nal_size =  ntohs(*((unsigned short*)src));
            unsigned short nal_size = amt_av_rb16(src);
        
            // consume the length of the aggregate
            src     += 2;
            src_len -= 2;

            if (nal_size <= src_len) {
                if (pass == 0) {
                    // counting
                    total_length += sizeof(psc) + nal_size;
                } else {
                    // copying
                    memcpy(m_pBuf + m_nPutPos,psc,sizeof(psc));
                    m_nPutPos += sizeof(psc);
                    memcpy(m_pBuf + m_nPutPos, src, nal_size);
                    m_nPutPos += nal_size;
                }
            } else {
                fprintf(stderr,
                       "nal size exceeds length: %d %d\n", nal_size, src_len);
                return false;
            }

            // eat what we handled
            src     += nal_size;//if use donl field , need add donl field size here
            src_len -= nal_size;//if use donl field , need add donl field size here
        }
    }
    return true;
}

}; //namespace AMT 

