# Makefile for SipClient
#
# programmer : yee young han ( websearch@naver.com )
#            : http://blog.naver.com/websearch
# start date : 2012/07/24

include ../Makefile.mk

INC=-I../SipUserAgent -I../SipStack -I../SipParser -I../SdpParser \
	-I../XmlParser -I../SipPlatform $(OPENSSL_INC)
LIB=-L/home/nex/work/cims/build/csp/psip_build \
	-lsipuseragent -lsipstack -lsipparser -lsdpparser -lxmlparser -lsipplatform \
	$(OPENSSL_LIB) -lssl -lcrypto -lpthread

.SUFFIXES: .o .cpp
%.o : %.cpp
	$(CC) $(CFLAGS) -DNO_ALSA -c $< $(INC)

all: SipClient.exe

SRV_OBJ=SipClient.o SipClientMain.o SipClientSetup.o \
	G711.o RtpThread.o

SipClient.exe: $(SRV_OBJ)
	$(CC) $(CFLAGS) -o $@ $(SRV_OBJ) $(INC) $(LIB)

clean:
	rm -f *.exe
	rm -f *.o
	rm -f core.*

