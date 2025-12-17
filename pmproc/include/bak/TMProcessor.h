#ifndef __TMProcessor__
#define __TMProcessor__

#include "TMBuffer.h"

class TMProcessor
{
public:
   TMProcessor() {};
   virtual ~TMProcessor() {}

    
    /*
    //   virtual bool init(MInfo & info);

    virtual bool init(TCodec::TYPE codec, TCodec::FORMAT format);
    virtual bool proc(const MBuf& in, MBuf& out) = 0;
    //virtual bool put(const TMBuffer& in) = 0;

    //virtual bool get(TMBuffer& out) = 0;
    */

    virtual bool proc(const TMBuffer& in, TMBuffer& out) = 0;
};
#endif // __TMProcessor__

