#ifndef __TCodecs__
#define __TCodecs__

#include <string>

#include "TMBase.h"

#include "TMBuffer.h"
#include "TCodec.h"


// opencore-amr 
#include "interf_enc.h"
#include "interf_dec.h"
#include "dec_if.h"

//vo-amrwbenc
#include "enc_if.h"



class TAmrDecoder : public TCodec
{
   private:
	  void* _handle;
	  short _lastSample;

	  int   _resmaplingX; // + : upsampling(multiple)  or - downsampling (div); 
	  int   _lastFrametype ; // for bfi

   
   public:
	  TAmrDecoder(TFORMAT format=TFORMAT_DEFAULT, TRATE hz=LPCM_RATE_DEFAULT) 
		 : TCodec(TCODEC_AMR, format, hz), _handle(NULL) 
	  { 
		 //format -> 0: file, 1: octet-alingend, 2: bandwidth-efficient.. 
	  }

	  ~TAmrDecoder() { final(); }

	  bool init()
	  {
		 final();
		 
		 _handle = Decoder_Interface_init(); 
		 if(!_handle) return false;

		 _lastSample = 0;
	     _lastFrametype = 7 ; //???

		 // for resmapling
		 _resmaplingX = (_hz)>=8000?(_hz/8000):(-8000/_hz);

		 return true;
	  }

	  bool final()
	  {
		 if (!_handle) return true;
		 Decoder_Interface_exit(_handle);
		 _handle = NULL;
		 return true;
	  }

	  bool code(const TMBuffer& in, TMBuffer& out)
	  {
		 if(in.len() == 0) return false;
		 unsigned char* ptrIn = in.ptr();
		 short * ptrOut = (short *)out.ptr();
		 int len = in.len();

		 int frametype; 

		 if(_format == 1) // octet-aligned 
		 {
			ptrIn = ptrIn + 1; 
			len -= 1;

			frametype = (*ptrIn >> 3) & 0xf;
		 }
		 else if (_format == 2) // bandwidth efficient
		 {
			unsigned char idata[256]; // for bandwidth efficient
			unsigned short hdr = (unsigned short)(*ptrIn)<<8 | *(ptrIn +1);
		
			frametype = (hdr & 0x0780) >> 7;

		    int bits = AMR_BITS[frametype];

			idata[0] = (frametype << 3) | (0x01 << 2);
			for(int i=0; i<bits; i++)
			{
			   if(ptrIn[(i+10)>>3] & AMR_BIT_IN_BYTE_R[(i+10) & 0x07])
				   idata[1+(i>>3)] |= 1 << (~i & 0x07);
			}
			ptrIn = idata;

		 } 
		 else // file format if2
		 {
			ptrIn = in.ptr();
			len = in.len();
			frametype = (*ptrIn >> 3) & 0xf;
		 }

		 out.sequence() = in.sequence();
		 out.timestamp() = in.timestamp();
		 out.codec()  = TCODEC_LPCM8K;
		 out.format() = _format;

		 if(frametype > 7)  //SID or NO-DATA  ...
		 {
			if(_lastFrametype < 8) 
			{
			   Decoder_Interface_Decode(_handle, ptrIn, ptrOut, 1);
			   out.len() = 320;
			   out.skip() = false;
			} 
			else 
			{
			   out.skip() = true;
			   out.reset(FRAME_SIZE_LPCM);
			   out.len() = FRAME_SIZE_LPCM;
			}
		 } else {
			Decoder_Interface_Decode(_handle, ptrIn, ptrOut, 0);

			out.len() = 320;
			out.skip() = false;
		 // if input sampling rate 16000, rate(_resmaplingX) : 2
		 // if input sampling rate 32000, rate : 4

			if(_resmaplingX>1)
			{
			   _lastSample = out.upsample(_resmaplingX, _lastSample);
			} 
			else if(_resmaplingX < -1)
			{
			   out.downsample(-_resmaplingX);
			}

		 }
		 _lastFrametype = frametype;
		 return true;
	  }
};

class TAmrEncoder : public TCodec
{
   private:
	  void* _handle;
	  int    _dtx;
	  int	 _forceSpeech;
	  int    _mode; // 0~8
      
	  int _resmaplingX;
	  int _lastSample;

   public:
	  TAmrEncoder(TFORMAT format=TFORMAT_DEFAULT, TRATE hz=LPCM_RATE_DEFAULT) 
		 : TCodec(TCODEC_AMR, format, hz), _handle(NULL) 
	  { 
		 //format -> 0: file, 1: octet-alingend, 2: bandwidth-efficient.. 
	  }
	  ~TAmrEncoder() { final(); }


	  bool init()
	  {
		 final();
		 _dtx = 0;
		 _forceSpeech = 0;
		 _mode = Mode::MR122;
		 _handle = Encoder_Interface_init(_dtx);
		 if(!_handle) return false;

		 // for resmapling
		 _resmaplingX = (_hz)>=8000?(-_hz/8000):(8000/_hz);
		 _lastSample = 0;

		 return true;
	  }

	  bool final()
	  {
		 if (!_handle) return true;
		 Encoder_Interface_exit(_handle);
		 _handle = NULL;
		 return true;
	  }

	  bool code(const TMBuffer & in, TMBuffer & out)
	  {
		 if(_resmaplingX>1)
		 {
			_lastSample = in.upsample(_resmaplingX, _lastSample);
		 } 
		 else if (_resmaplingX < -1)
		 {
			in.downsample(-_resmaplingX);
		 }

		 short* ptrIn = (short*)in.ptr();
		 unsigned char* ptrOut = out.ptr();
		 int len = 0;
		 int frametype = 15; //15 No-Data

		 if(_format == 1) // octet-aligned 
		 {
			if(in.skip())
			{
			   //len = sizeof(AMR_SID_FIRST);
			   //memcpy(ptrOut+1, AMR_SID_FIRST, len);
			   if(_skipEncSid) 
			   {
				  len = sizeof(AMR_SID_FIRST);
				  memcpy(ptrOut+1, AMR_SID_FIRST, len);
			   }
			   else 
			   {
				  in.set(LPCM_SID, 320, 0, true);
				  //ptrIn = (short *) LPCM_SID;
				  len = Encoder_Interface_Encode(_handle, _mode, ptrIn, ptrOut+1, _forceSpeech);
			   }
			}
			else 
			{
			   len = Encoder_Interface_Encode(_handle, _mode, ptrIn, ptrOut+1, _forceSpeech);
			}
			ptrOut[0] = 0x0f << 4; 
			len += 1;
			frametype = (ptrOut[1] >> 3) & 0xf;
		 }
		 else if (_format == 2) // bandwidth efficient
		 {
		    unsigned char odata[256]; // for bandwidth efficient

			if(in.skip())
			{
			   //len = sizeof(AMR_SID_FIRST);
			   //memcpy(odata, AMR_SID_FIRST, len);
			   if(_skipEncSid) 
			   {
				  len = sizeof(AMR_SID_FIRST);
				  memcpy(odata, AMR_SID_FIRST, len);
			   }
			   else 
			   {
				  in.set(LPCM_SID, 320, 0, true);
				  //ptrIn = (short *) LPCM_SID;
				  len =  Encoder_Interface_Encode(_handle, _mode, ptrIn, odata, _forceSpeech);
			   }
			}
			else 
			{
			   len =  Encoder_Interface_Encode(_handle, _mode, ptrIn, odata, _forceSpeech);
			}
			frametype = (odata[0] >> 3) & 0xf;
		    int bits = AMR_BITS[frametype];

			unsigned short hdr = (0x0f << 12) | (0 << 11) | (frametype << 7) | (1 << 6);

			memset(ptrOut, 0, 128);
			ptrOut[0] = hdr >> 8;
			ptrOut[1] = hdr & 0x00ff;

			for(int i=0; i<bits; i++)
			{
			   if(odata[1+(i>>3)] & AMR_BIT_IN_BYTE_R[i & 0x07])
				  ptrOut[(i+10)>>3] |= 1 << (~(i+10) & 0x07);
			}
		 } 
		 else // file format if2
		 {
			if(in.skip())
			{
			   //len = sizeof(AMR_SID_FIRST);
			   //memcpy(ptrOut, AMR_SID_FIRST, len);
			   if(_skipEncSid) 
			   {
				  len = sizeof(AMR_SID_FIRST);
				  memcpy(ptrOut, AMR_SID_FIRST, len);
			   }
			   else 
			   {
				  in.set(LPCM_SID, 320, 0, true);
				  //ptrIn = (short *) LPCM_SID;
				  len= Encoder_Interface_Encode(_handle, _mode, ptrIn, ptrOut, _forceSpeech);
			   }
			}
			else 
			{
			   len= Encoder_Interface_Encode(_handle, _mode, ptrIn, ptrOut, _forceSpeech);
			}
			frametype = (ptrOut[0] >> 3) & 0xf;
		 }
		
		 if(len==0 || frametype == 15) out.skip() = true;
		 else out.skip() = false;
		 out.len() = len;
		 out.sequence() = in.sequence();
		 out.timestamp() = in.timestamp();
		 out.codec() = _codec;
		 out.format() = _format;
		 return true;
	  }
};

class TAmrwbDecoder : public TCodec
{
   private:
	  void* _handle;

	  int _resmaplingX;
	  short  _lastSample;
	  int _lastFrametype; // for bfi

   public:
	  TAmrwbDecoder(TFORMAT format=TFORMAT_DEFAULT, TRATE hz=LPCM_RATE_DEFAULT) 
		 : TCodec(TCODEC_AMR, format, hz), _handle(NULL) 
	  { 
		 //format -> 0: file, 1: octet-alingend, 2: bandwidth-efficient.. 
	  }
	  ~TAmrwbDecoder() { final(); }

	  bool init()
	  {
		 final();
		 _handle = D_IF_init();
		 if(!_handle) return false;

		 // for resmapling
		 _resmaplingX = (_hz)>=16000?(_hz/16000):(-16000/_hz);
		 _lastSample = 0;
		 _lastFrametype = 8; // for bfi
		 return true; 
	  }

	  bool final()
	  {
		 if (!_handle) return true;
		 D_IF_exit(_handle);
		 _handle = NULL;
		 return true;
	  }
	  bool code(const TMBuffer& in, TMBuffer& out)
	  {
		 if(in.len() == 0) return false;
		 unsigned char * ptrIn = in.ptr();
		 short* ptrOut = (short *)out.ptr();
		 int len = in.len();
	
		 int frametype = 15;
		 unsigned char idata[256]; // for bandwidth efficient
		 
		 if(_format == 1) // octet-aligned 
		 {
			ptrIn = ptrIn + 1; 
			len -= 1;
			frametype = (*ptrIn >> 3) & 0xf;
		 }
		 else if (_format == 2) // bandwidth efficient
		 {
			unsigned short hdr = (unsigned short)(*ptrIn)<<8 | *(ptrIn +1);
		
			frametype = (hdr & 0x0780) >> 7;

		    int bits = AMRWB_BITS[frametype];

			idata[0] = (frametype << 3) | (0x01 << 2);
			for(int i=0; i<bits; i++)
			{
			   if(ptrIn[(i+10)>>3] & AMR_BIT_IN_BYTE_R[(i+10) & 0x07])
				   idata[1+(i>>3)] |= 1 << (~i & 0x07);
			}
			ptrIn = idata;

		 } 
		 else // file format if2
		 {
			ptrIn = in.ptr();
			len = in.len();
			frametype = (*ptrIn >> 3) & 0xf;
		 }

		 out.sequence() = in.sequence();
		 out.timestamp() = in.timestamp();
		 out.codec() = TCODEC_LPCM16K;
         out.format() = _format;
		 
		 if(frametype > 8)  //SID or NO-DATA  ...
		 {
			if(_lastFrametype < 9)
			{
			   D_IF_decode(_handle, ptrIn, ptrOut, 1);
			   out.skip() = false;
			   out.len() = 640; //where  error code ?
			   if(_resmaplingX>1)
			   {
				  _lastSample = out.upsample(_resmaplingX, _lastSample);
			   } 
			   else if(_resmaplingX < -1)
			   {
				  out.downsample(-_resmaplingX);
			   }
			}
			else 
			{
			   out.skip() = true;
			   out.reset(FRAME_SIZE_LPCM);
			   out.len() = FRAME_SIZE_LPCM;
			}
		 } 
		 else 
		 {
			D_IF_decode(_handle, ptrIn, ptrOut, 0);
			out.skip() = false;
			out.len() = 640; //where  error code ?
			if(_resmaplingX>1)
			{
			   _lastSample = out.upsample(_resmaplingX, _lastSample);
			} 
			else if(_resmaplingX < -1)
			{
			   out.downsample(-_resmaplingX);
			}
		 }
		 _lastFrametype = frametype;
		 return true;
	  }

};



class TAmrwbEncoder : public TCodec
{
   private:
	  void* _handle;
	  int    _mode; // 
	  int    _dtx; //

	  int _resmaplingX;
	  short  _lastSample;

   public:
	  TAmrwbEncoder(TFORMAT format=TFORMAT_DEFAULT, TRATE hz=LPCM_RATE_DEFAULT) 
		 : TCodec(TCODEC_AMR, format, hz), _handle(NULL) 
	  { 
		 //format -> 0: file, 1: octet-alingend, 2: bandwidth-efficient.. 
	  }
	  
	  ~TAmrwbEncoder() { final(); }

	  bool init()
	  {
	     final();
		 _handle = E_IF_init();
		 if(!_handle) return false;

		 _mode = 8;   // 0:7kbps, ... 8:24kbps;
		 _dtx = 0;
		 _lastSample = 0;

		 // for resmapling
		 _resmaplingX = (_hz)>=16000?(-_hz/16000):(16000/_hz);
		 return true; 
	  }

	  bool final()
	  {
		 if(!_handle) return true;
		 E_IF_exit(_handle);
		 _handle = NULL;

		 return true; 
	  }

	  bool code(const TMBuffer& in, TMBuffer& out)
	  {
		 if(_resmaplingX>1)
		 {
			_lastSample = in.upsample(_resmaplingX, _lastSample);
		 } 
		 else if (_resmaplingX < -1)
		 {
			in.downsample(-_resmaplingX);
		 }
		 
		 short* ptrIn = (short*)in.ptr();
		 unsigned char* ptrOut = out.ptr();
		 int len=0;
		 int frametype = 15; // no-data
		 if(_format == 1) // octet-aligned 
		 {
			if(in.skip())
			{
			   if(_skipEncSid) 
			   {
				  len = sizeof(AMRWB_SID_FIRST);
				  memcpy(ptrOut+1, AMRWB_SID_FIRST, len);
			   } 
			   else 
			   {
				  ptrIn = (short *) LPCM_SID;
				  len = E_IF_encode(_handle, _mode, ptrIn, ptrOut+1, _dtx);

				  //printf("# non-skip enc sid -> len:%d, dtx:%d\n", len, _dtx);
			   }
			}
			else 
			{
			   len = E_IF_encode(_handle, _mode, ptrIn, ptrOut+1, _dtx);
			}	
			ptrOut[0] = 0x0f << 4;
			len += 1;
			frametype = (ptrOut[1] >> 3) & 0xf;
		 }
		 else if (_format == 2) // bandwidth efficient
		 {
		    unsigned char odata[256]; // for bandwidth efficient

			if(in.skip())
			{
			   if(_skipEncSid) 
			   {
				  len = sizeof(AMRWB_SID_FIRST);
				  memcpy(odata, AMRWB_SID_FIRST, len);
			   }
			   else 
			   {
				  ptrIn = (short *) LPCM_SID;
				  len = E_IF_encode(_handle, _mode, ptrIn, odata, _dtx);
			   }
			}
			else 
			{
			   len = E_IF_encode(_handle, _mode, ptrIn, odata, _dtx);
			}

			frametype = (odata[0] >> 3) & 0xf;
		    int bits = AMRWB_BITS[frametype];

			unsigned short hdr = (0x0f << 12) | (0 << 11) | (frametype << 7) | (1 << 6);

			memset(ptrOut, 0, 128); // must - be ?
			ptrOut[0] = hdr >> 8;
			ptrOut[1] = hdr & 0x00ff;

			for(int i=0; i<bits; i++)
			{
			   if(odata[1+(i>>3)] & AMR_BIT_IN_BYTE_R[i & 0x07])
				  ptrOut[(i+10)>>3] |= 1 << (~(i+10) & 0x07);
			}
		 } else // file format if2
		 {
			if(in.skip())
			{
			   if(_skipEncSid) 
			   {
				  len = sizeof(AMRWB_SID_FIRST);
				  memcpy(ptrOut, AMRWB_SID_FIRST, len);
			   }
			   else 
			   {
				  ptrIn = (short *) LPCM_SID;
				  len = E_IF_encode(_handle, _mode, ptrIn, ptrOut, _dtx);
			   }
			}
			else 
			{
			   len = E_IF_encode(_handle, _mode, ptrIn, ptrOut, _dtx);
			}
			frametype = (ptrOut[0] >> 3) & 0xf;
		 }

		 if(len==0 || frametype == 15) out.skip() = true;
		 else out.skip() = false;
		 out.len() = len;
		 out.sequence() = in.sequence();
		 out.timestamp() = in.timestamp();
		 out.codec() = _codec;
		 out.format() = _format;

		 return true;
	  }
};


#if 0
   // evs codec 
   class TEvsDecoder : public TCodec
   {
   private:
      Decoder_State_fx* _state;
      //unsigned short    _bit_stream[MAX_BITS_PER_FRAME + 16];
   public:
      TEvsDecoder() _state(NULL){};

      ~TEvsDecoder() { final(); };

      bool init()
      {
         final();
         _state = (Decoder_State_fx*)calloc(1, sizeof(Decoder_State_fx)));

         //_state->bit_stream_fx = _bit_stream;
   
         _state->codec_mode = 1; // ???
         _state->Opt_AMR_WB_fx = 1; // ???

         _state->output_Fs_fx = 16000;
         //_state->Opt_VOIP_fx = 1;
           //VoIP mode
         _state->Opt_VOIP_fx = 1;


         // bitstreamformat
         _state->bitstreamformat = MIME;
         _state->amrwb_rfc4867_flag = 0;

         // output(LPCM) Sampling Rate -- 
                
         reset_indices_dec_fx(_state);
         init_decoder_fx(_state);

      }

      void final()
      {
         if(_state)  free(_state);
      }

      bool code(const TMBuffer& in, TMBuffer& out)
      {
         _state->bit_stream_fx = in.cptr();

         if (_state->Opt_AMR_WB_fx)
         {
            amr_wb_dec_fx(out.ptr(), _state);
            out.len() = _state.output_frame_fx;
         }
         else
         {
            evs_dec_fx(_state, out.ptr(), (_state->bfi_fx == 0)?FRAMEMODE_NORMAL: FRAMEMODE_MISSING);
            out.len() = _state.output_frame_fx;
         }
	     out.skip() = false;
	     out.sequence() = in.sequence();
         out.timestamp() = in.timestamp();
         return true;
      }
   };

   class TEvsEncoder : public TCodec
   {
   private:
      Encoder_State_fx* _state;
      int    _mode; // 
      int    _dtx; //

      int    _frameType;
      int    _frameSizeIn;
   public:

      TAmrwbEncoder() {
         _state = NULL;
         //_mode = ACELP_23k85;
         //_dtx = 1;
         _frameType = ;
      }

      bool init()
      {
	     final();
         _state = (Encoder_State_fx*)calloc(1, sizeof(Encoder_State_fx)));

         // in/out Sampling rate
         _state->input_Fs_fx = 32000; // input sampling rate
         _state->max_bwidth_fx = SWB; // output sampling rate  : more input sampling rate
         // NB, WB, SWB, FB 


         _state->total_brate_fx = ACELP_23k85;  // bitrate

         // AMR-WB IO/EVS primary mode 
         _state->Opt_AMR_WB_fx = 1; //0 : EVS primary mode, 1: AMR-WB IO
         // AMR-IO Mode range -> total_brate_fx : ACELP_6k60~ACELP_23k85
         //if( EVS primary mode  -> _state->Opt_AMR_WB_fx == 0) 
         //_state->codec_mode = MODE1, or MODE2;


         _state->Opt_RF_ON = 0;   // Activate channel-aware mode Off
         _state->rf_fec_offset = FEC_OFFSET; // ? FEC_OFFSET : 3
         _state->rf_fec_indicator = 1;  // 0: LO, 1:HI
         if (_state->total_brate_fx != ACELP_13k20)
         {
            _state->Opt_RF_ON = 0;   // Activate channel-aware mode only supported at ACELP_13k20
         }



         // if DTX On
         _state->Opt_DTX_ON_fx = 1;
         _state->interval_SID_fx = FIXED_SID_RATE; // why ? range 3~100 , FIXED_SID_RATE:8
         _state->var_SID_rate_flag_fx = 0;

         _state->Opt_HE_SAD_ON_fx = 0;

         // SC-VBR Mode  
         _state->Opt_SC_VBR_fx = 0; // Off
         _state->last_Opt_SC_VBR_fx = 0;

         // Ouput file format
         _state->bitstreamformat = MIME; //G192:0,  MIME:1

         _state->input_frame_fx = _state->input_Fs_fx / 1000 * 20; // hz / 1000msec * 20msec

         _frameSizeIn = _state->input_frame_fx;

         st_fx->ind_list_fx = ind_list; //

         init_encoder_fx(st_fx);

         return true;
      }

      void final()
      {

		 if(!_state) return true;
		 
		 destroy_encoder_fx(_state)
         free(_state);
         _state = NULL ;

		 return true;
      }

      bool code(const TMBuffer& in, TMBuffer& out)
      {

         inData.Buffer = (unsigned char*)in.cptr();

         if (_mode == AMRWBIO) {
            SUB_WMOPS_INIT("amr_wb_enc");
            amr_wb_enc_fx(_state, in.ptr(), in.len());
            END_SUB_WMOPS;
         }
         else // EVS Primary mode
         {
            SUB_WMOPS_INIT("evs_enc");
            /* EVS encoder*/
            amr_wb_enc_fx(_state, buf.ptr(), buf.len());
            END_SUB_WMOPS;
         }

         inData.Length = buf.len();
         outData.Buffer = buf.ptr();

         _state->audioApi.SetInputData(_state->handle, &inData);
         _state->audioApi.GetOutputData(_state->handle, &outData, &outFormat);

	     out.skip() = false;
	     out.sequence() = in.sequence();
         out.timestamp() = in.timestamp();
         out.len() = outData.Length;
         return true;
      }

   };
#endif // EVS

#endif  //__TCodecs__

