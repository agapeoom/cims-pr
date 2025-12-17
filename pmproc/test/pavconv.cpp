#include <string>
#include <iostream>

#include "pbase.h"
#include "PAVFileConv.h"


#define PMP_MAX_GROUPS  1
#define PMP_MAX_SESSIONS  1

int main(int argc, char ** argv)
{
   PAVFileConv conv;

   int bres;
   bres = conv.init(argv[1], argv[2]);
   if(!bres)
   {
	  printf("# failed to init File Converter!\n");
	  exit(1);
   }

   PAVParam param;
   param.init(PAVCODEC_PCMA);
   conv.setRmtA(param);
   if(!bres)
   {
	  printf("# failed to init setRmtA!\n");
	  exit(1);
   }
   param.init(PAVCODEC_H264);
   conv.setRmtV(param);
   if(!bres)
   {
	  printf("# failed to init setRmtV!\n");
	  exit(1);
   }

   conv.proc();

   conv.final();

   return 0;
}

