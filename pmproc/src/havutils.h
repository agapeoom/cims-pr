#ifndef __HAV_HAV_UTILS_H__
#define __HAV_HAV_UTILS_H__

namespace AMT 
{

#define AMR_MAX_FRAME_INDEX 16

const static unsigned int BIT_IN_BYTE[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
const static unsigned int BIT_IN_BYTE_R[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
const static int AMR_BITS[AMR_MAX_FRAME_INDEX] = {95, 103, 118, 134, 148, 159, 204, 244, 39, 43, 38, 37, -4, -4, -4, 0};
const static int AMRWB_BITS[AMR_MAX_FRAME_INDEX] = {132, 177, 253,285, 317, 365, 397, 461, 477, 40, -4, -4, -4, -4, 0, 0};

const static int AmrRates[AMR_MAX_FRAME_INDEX] = { 4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200 , -1, -1, -1, -1, -1, -1, -1, -1};
const static int AmrWbRates[AMR_MAX_FRAME_INDEX] = { 6600, 8850, 12650, 14250, 15850, 18250, 19850, 23050, 23850 , -1, -1, -1, -1, -1, -1, -1};

#define AMRWB_MAGIC_NUMBER "#!AMR-WB\n"
#define AMR_MAGIC_NUMBER "#!AMR\n"

typedef struct {
   unsigned int uTimestamp;
   unsigned int uOffset;
   unsigned int uSize;
   bool         bKeyFrame;
} HAVSyncEntry;

class CHAVSyncEntries 
{
public:
   CHAVSyncEntries(int nClusterSize=1024);
   ~CHAVSyncEntries();

   void Clear();
   int Add(const HAVSyncEntry* pEntry);
   HAVSyncEntry* GetAt(int nIndex);

   int Seek(unsigned int uTimestamp);

   unsigned int GetEntryCount() { return m_uEntryCount; };

protected:
   unsigned int m_uClusterSize;
   unsigned int m_uClusterBits;
   unsigned int m_uAllocated;
   unsigned int m_uEntryCount;
   HAVSyncEntry  **m_pClusters;
};


class CHAVCrc
{
public:
   static void Reset(unsigned int *crc);
   static void Update(unsigned int *ptr, int offset, int len, unsigned int *crc);
   static void UpdatePtr(unsigned char *ptr, int len, unsigned int *crc);
   static void UpdateImm(unsigned int val, int len, unsigned int *crc);
   static void UpdateZero(int len, unsigned int *crc);
};

#define            SIGN_BIT            (0x80)                        /* Sign bit for a A-law byte. */
#define            QUANT_MASK            (0xf)                        /* Quantization field mask. */
#define            NSEGS                        (8)                        /* Number of A-law segments. */
#define            SEG_SHIFT            (4)                        /* Left shift for segment number. */
#define            SEG_MASK            (0x70)                        /* Segment field mask. */

short search(short val,short            *table,            short size);
unsigned char linear2alaw(short            pcm_val);


}; //namespace AMT 

#endif /*__HAV_HAV_UTILS_H__*/
