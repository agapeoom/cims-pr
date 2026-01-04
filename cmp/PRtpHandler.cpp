#include "PRtpHandler.h"
#include "McpttGroup.h"
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

PRtpTrans::PRtpTrans(const std::string & name) : PHandler(name), _group(NULL), _myName(name), _sessionId(name), _localPort(0), _localVideoPort(0) {
}

PRtpTrans::~PRtpTrans() {
    final();
}

void PRtpTrans::setGroup(McpttGroup* group) {
    PAutoLock lock(_mutex);
    _group = group;
}

bool PRtpTrans::init(const std::string & ipLoc, unsigned int portLoc, unsigned int videoPortLoc)
{
    PAutoLock lock(_mutex);
    bool res = _rtpSock.init(ipLoc, portLoc);
    if(res) {
        // Init RTCP on port + 1
        res = _rtcpSock.init(ipLoc, portLoc + 1);
    }
    
    if (res && videoPortLoc > 0) {
        res = _videoRtpSock.init(ipLoc, videoPortLoc);
        if (res) {
            res = _videoRtcpSock.init(ipLoc, videoPortLoc + 1);
        }
    }
    
    if (res) {
        _localPort = portLoc;
        _localVideoPort = videoPortLoc;
    }
    return res;
}

bool PRtpTrans::final()
{
    PAutoLock lock(_mutex);
    _rtcpSock.final();
    _videoRtpSock.final();
    _videoRtcpSock.final();
    return _rtpSock.final();
}

bool PRtpTrans::setRmt(const std::string & ipRmt, unsigned int portRmt, unsigned int videoPortRmt) {
    PAutoLock lock(_mutex);
    bool res = _rtpSock.setRmt(ipRmt, portRmt);
    if(res) {
        // Set RTCP Rmt on port + 1
        res = _rtcpSock.setRmt(ipRmt, portRmt + 1);
    }
    
    if (res && videoPortRmt > 0) {
        res = _videoRtpSock.setRmt(ipRmt, videoPortRmt);
        if (res) {
            res = _videoRtcpSock.setRmt(ipRmt, videoPortRmt + 1);
        }
    }
    return res;
}

void PRtpTrans::sendRtcp(char* data, int len) {
    PAutoLock lock(_mutex);
    if (_rtcpSock.getFd() != INVALID_SOCKET) {
        int ret = _rtcpSock.send(data, len);
        if (ret < 0) {
            perror("PRtpTrans::sendRtcp send failed");
        } else {
            printf("TX RTCP: len=%d\n", ret);
        }
    } else {
        printf("PRtpTrans::sendRtcp INVALID SOCKET\n");
    }
}

void PRtpTrans::sendRtp(char* data, int len) {
    PAutoLock lock(_mutex);
    if (_rtpSock.getFd() != INVALID_SOCKET) {
        _rtpSock.send(data, len);
    }
}

void PRtpTrans::sendVideoRtp(char* data, int len) {
    PAutoLock lock(_mutex);
    if (_videoRtpSock.getFd() != INVALID_SOCKET) {
        _videoRtpSock.send(data, len);
    }
}

bool PRtpTrans::proc() 
{
    std::string ipRmt;
    int portRmt;
    char rtcpBuf[2048];
    char pkt[1600];
    
    // Process RTP
    while (true) {
        int rtpFd;
        {
            PAutoLock lock(_mutex);
            rtpFd = _rtpSock.getFd();
        }
        if (rtpFd == INVALID_SOCKET) break;

        int len = 0;
        // recv is non-blocking because init sets O_NONBLOCK
        {
            PAutoLock lock(_mutex);
            len = _rtpSock.recv(pkt, sizeof(pkt), ipRmt, portRmt);
            if (len > 0) {
                 if (_group) {
                      _group->onRtpPacket(_sessionId, pkt, len);
                 } else {
                      _rtpSock.send(pkt, len);
                 }
            }
        }
        
        if (len <= 0) break; // EAGAIN or Error, stop loop
    }

    // Process RTCP
    while(true) {
         int rtcpFd;
         {
            PAutoLock lock(_mutex);
            rtcpFd = _rtcpSock.getFd();
         }
         if (rtcpFd == INVALID_SOCKET) break;

         int len = 0;
         McpttGroup* grp = NULL;
         
         {
             PAutoLock lock(_mutex);
             len = _rtcpSock.recv(rtcpBuf, sizeof(rtcpBuf), ipRmt, portRmt);
             if (len > 0) {
                 grp = _group;
                 printf("RX RTCP: len=%d from %s:%d\n", len, ipRmt.c_str(), portRmt);
                 fflush(stdout);
             }
         }
         
         if (len > 0 && grp) {
             grp->onRtcpPacket(_sessionId, rtcpBuf, len);
         }
         
         if (len <= 0) break; // EAGAIN or Error
    }

    // Process Video RTP
    while (true) {
        int rtpFd;
        {
            PAutoLock lock(_mutex);
            rtpFd = _videoRtpSock.getFd();
        }
        if (rtpFd == INVALID_SOCKET) break;

        int len = 0;
        {
            PAutoLock lock(_mutex);
            len = _videoRtpSock.recv(pkt, sizeof(pkt), ipRmt, portRmt);
            if (len > 0) {
                 if (_group) {
                      _group->onVideoRtpPacket(_sessionId, pkt, len);
                 } else {
                      _videoRtpSock.send(pkt, len);
                 }
            }
        }
        
        if (len <= 0) break;
    }

    // Process Video RTCP
    while(true) {
         int rtcpFd;
         {
            PAutoLock lock(_mutex);
            rtcpFd = _videoRtcpSock.getFd();
         }
         if (rtcpFd == INVALID_SOCKET) break;

         int len = 0;
         McpttGroup* grp = NULL;
         
         {
             PAutoLock lock(_mutex);
             len = _videoRtcpSock.recv(rtcpBuf, sizeof(rtcpBuf), ipRmt, portRmt);
             if (len > 0) {
                 // Forward to group like audio RTCP? 
                 // For now, video floor control separate? 
                 // Or shared? Usually shared floor control on Audio RTCP.
                 // So we might just ignore video RTCP or echo it.
                 // Let's echo for now or attach to group same as audio if we want unified floor control.
                 // But specification implies floor control is usually on one stream (Audio).
                 // We will ECHO video RTCP for now (keep alive etc).
                 _videoRtcpSock.send(rtcpBuf, len);
             }
         }
         
         if (len <= 0) break;
    }

    return true; // Return true to be called again immediately? Or false to yield?
    // Usually false yields a bit, true loops immediately.
    // If we return true always, one worker might spin 100% CPU.
    // Ideally we return true if we did work, false if idle?
    // Let's return false to be safe and avoid CPU 100% in this loop if pasf doesn't sleep.
    // But original code returned false.
    return false;
}

bool PRtpTrans::proc(int id, const std::string & name, PEvent::Ptr spEvent) {
    return false;
}
