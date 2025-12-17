#pragma once
#ifndef __TMixProcessor__
#define __TMixProcessor__

#include <map>
#include <utility>

//#include "pbase.h"

#include "TMixer.h"
#include "TMProcessor.h"

class TMixProcessor : public TMProcessor
{
private:
   TGroupMixer * _pgmixer;

   unsigned int _gIndex;
   unsigned int _sIndex;

public:
   TMixProcessor() : _pgmixer(NULL) {
   }
   ~TMixProcessor() {

   }

   bool init(TGroupMixer * pgmixer, const unsigned int& sIndex, int codec) {
	  _pgmixer = pgmixer;

      _sIndex = sIndex;
      return _pgmixer->alloc(_sIndex, codec);
   }

   bool final(const unsigned int& sIndex) {
      if(!_pgmixer) return true;

	  bool bRes = _pgmixer->dealloc(_sIndex);
	  _pgmixer = NULL;
      return bRes;
   }

   bool put(const TMBuffer& in) 
   {
      if(!_pgmixer) return false;
	  _pgmixer->put(_sIndex, in);
	  return true;
   }
   
   bool get(TMBuffer& out) 
   {
      if(!_pgmixer) return false;
      return _pgmixer->get(_sIndex, out);
   }

   bool proc(const TMBuffer& in, TMBuffer& out)
   {
      if(!_pgmixer) return false;

      if(in.len() > 0) _pgmixer->put(_sIndex, in);
      return _pgmixer->get(_sIndex, out);
   }
};

#endif// __TMixProcessor__
