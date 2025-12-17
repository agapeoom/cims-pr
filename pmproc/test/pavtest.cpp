#include <string>
#include <iostream>

#include "pbase.h"
#include "PMPManager.h"


#define PMP_MAX_GROUPS  1
#define PMP_MAX_SESSIONS  2
#define PMP_SES_PER_GROUP 10

int main(int argc, char ** argv)
{

   bool bres = false;
   PMPManager pmp;

   //0. initialize rtp sockets  
   bres = pmp.init(PMP_MAX_GROUPS, PMP_MAX_SESSIONS, PMP_SES_PER_GROUP, "121.134.202.137", 50000);
   if(!bres)
   {
	  printf("# failed to init rtp session!\n");
	  exit(1);
   }
  
   // 1. create rtp session & assign rtp socket

   int gid =1 ; //if gid == even : 1, odd : 2;
   unsigned int uid;
   for(uid=0; uid<PMP_MAX_SESSIONS; uid++)
   {
	  //gid = i/2==0?1:2;
	  //2. get local rtp address 

	  bool bres = pmp.alloc(uid, gid, uid);
	  if(!bres)
	  {
		 fprintf(stderr, "# falied to alloc pmp session [%d]!\n", uid);
		 return -1;
	  }
	  std::string ipAddr = pmp.ipRtp(uid);
	  unsigned int portA = pmp.portRtpA(uid);
	  unsigned int portV = pmp.portRtpV(uid);
	  printf("PMPSession[G%02d:S%03d](ip_addr=%s, port_audio=%d, port_video=%d\n", 
			   gid, uid, ipAddr.c_str(), portA, portV);


	  //3. set remote address
	  // set audio remote target
#if 0
	  bres = pmp.setRmtA(uid, PAVCODEC_PCMA, "192.168.0.68", 12000, 8, 101);
	  if(!bres) 
	  {
		 printf("# failed setRmtA!\n");
		 return -1;
	  }
#endif

#if 1
	  // set audio video target
	  bres = pmp.setRmtV(uid, PAVCODEC_H264, "192.168.0.68", 60000, 125);
	  if(!bres) 
	  {
		 printf("# failed setRmtV!\n");
		 return -1;
	  }
#endif

	  std::string fname;
	  std::string fmt;
	  
	  fname = formatStr("sample%02d.avi", uid);
	  fmt = "avi";
	  bres = pmp.setPlayFile(uid, fname, fmt); 
	  if(!bres) 
	  {
		 printf("# failed pmp.setPlayFile!\n");
		 return -1;
	  }

	  fname = formatStr("record%02d.avi", uid);
	  bres = pmp.setRecordFile(uid, fname, fmt); 
	  if(!bres) 
	  {
		 printf("# failed pmp.setRecordFile!\n");
		 return -1;
	  }

	  pmp.start(uid);
	  msleep(2000);
   }

#if 1
   struct timeval tvStartTime, tvCurTime;

   gettimeofday(&tvStartTime, NULL);

   for(int i=0; i<30000; i++) 
   {
	  gettimeofday(&tvCurTime, NULL);

	  int interval = PDIFFTIME(tvCurTime, tvStartTime);
	  if(interval > 65000)
	  {

	  }
	  msleep(10);
   }

   for(uid=0; uid<PMP_MAX_SESSIONS; uid++)
   {
	  pmp.dealloc(uid);
   }
#endif
   return 0;
}
