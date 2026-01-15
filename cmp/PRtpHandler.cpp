#include "PRtpHandler.h"
#include "McpttGroup.h"
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

PRtpTrans::PRtpTrans(const std::string & name) : PHandler(name), _group(NULL), _myName(name), _sessionId(name), _localPort(0), _localVideoPort(0) {
    for(int i=0; i<2; ++i) {
        _peers[i].active = false;
        memset(&_peers[i].addrRtp, 0, sizeof(sockaddr_in));
        memset(&_peers[i].addrRtcp, 0, sizeof(sockaddr_in));
        memset(&_peers[i].addrVideoRtp, 0, sizeof(sockaddr_in));
        memset(&_peers[i].addrVideoRtcp, 0, sizeof(sockaddr_in));
    }
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
    printf("PRtpTrans::init rtp %s:%d\n", ipLoc.c_str(), portLoc);
    if(res) {
        // Init RTCP on port + 1
        printf("PRtpTrans::init rtcp %s:%d\n", ipLoc.c_str(), portLoc + 1);
        res = _rtcpSock.init(ipLoc, portLoc + 1);
    }
    
    if (res && videoPortLoc > 0) {
        printf("PRtpTrans::init video rtp %s:%d\n", ipLoc.c_str(), videoPortLoc);
        res = _videoRtpSock.init(ipLoc, videoPortLoc);
        if (res) {
            printf("PRtpTrans::init video rtcp %s:%d\n", ipLoc.c_str(), videoPortLoc + 1);
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

bool PRtpTrans::setRmt(const std::string & ipRmt, unsigned int portRmt, unsigned int videoPortRmt, int peerIdx) {
    PAutoLock lock(_mutex);
    
    // Logic: 
    // If peerIdx is provided (0 or 1), use it directly.
    // If peerIdx is -1, fallback to "Smart Add":
    //   If ip/port matches Peer 0, update it.
    //   If ip/port matches Peer 1, update it.
    //   If Peer 0 is empty, set Peer 0.
    //   If Peer 0 set but mismatch, and Peer 1 empty, set Peer 1.
    //   If both set, overwrite Peer 1 (Callee) default.

    int idx = peerIdx;
    
    if (idx == -1) {
        // Smart Logic
        if (_peers[0].active && _peers[0].ip == ipRmt) { 
            idx = 0;
        } else if (_peers[1].active && _peers[1].ip == ipRmt) {
            idx = 1;
        }
        
        if (idx == -1) {
            if (!_peers[0].active) idx = 0;
            else if (!_peers[1].active) idx = 1;
            else {
                 if (_peers[0].ip == "0.0.0.0") idx = 0;
                 else idx = 1; // Default overwrite second
            }
        }
    }
    
    if (idx < 0 || idx > 1) {
        // Invalid index
        printf("PRtpTrans::setRmt Invalid Index %d\n", idx);
        return false;
    }

    // Update Peer[idx]
    _peers[idx].ip = ipRmt;
    _peers[idx].port = portRmt;
    _peers[idx].videoPort = videoPortRmt;
    _peers[idx].active = true;

    // Helper to constructing sockaddr_in
    auto makeAddr = [](struct sockaddr_in& addr, const std::string& ip, int port) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
    };

    if (idx == 0) {
        _rtpSock.setRmt(ipRmt, portRmt);
        _rtcpSock.setRmt(ipRmt, portRmt + 1);
        if (videoPortRmt > 0) {
            _videoRtpSock.setRmt(ipRmt, videoPortRmt);
            _videoRtcpSock.setRmt(ipRmt, videoPortRmt + 1);
        }
    }

    return true;
}

void PRtpTrans::sendRtcp(char* data, int len) {
    PAutoLock lock(_mutex);
    if (_rtcpSock.getFd() != INVALID_SOCKET) {
        _rtcpSock.send(data, len);
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

// [Shared Session Support]
void PRtpTrans::sendTo(const std::string& ip, int port, char* data, int len) {
    PAutoLock lock(_mutex);
    if (_rtpSock.getFd() != INVALID_SOCKET) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        _rtpSock.sendTo(data, len, &addr);
    }
}

void PRtpTrans::sendVideoTo(const std::string& ip, int port, char* data, int len) {
    PAutoLock lock(_mutex);
    if (_videoRtpSock.getFd() != INVALID_SOCKET) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        _videoRtpSock.sendTo(data, len, &addr);
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
        {
            PAutoLock lock(_mutex);
            len = _rtpSock.recv(pkt, sizeof(pkt), ipRmt, portRmt);
            if (len > 0) {
                if (_group) {
                    _group->onRtpPacket(ipRmt, portRmt, pkt, len);
                } else {
                    // Relay Logic
                    int srcIdx = -1;
                    if (_peers[0].active && portRmt == _peers[0].port && ipRmt == _peers[0].ip) srcIdx = 0;
                    else if (_peers[1].active && portRmt == _peers[1].port && ipRmt == _peers[1].ip) srcIdx = 1;
                    
                    if (srcIdx != -1) {
                        int dstIdx = (srcIdx == 0) ? 1 : 0;
                        if (_peers[dstIdx].active) {
                            _rtpSock.sendTo(pkt, len, &_peers[dstIdx].addrRtp);
                        }
                    } else if (_peers[0].active && !_peers[1].active && srcIdx == 0) {
                        _rtpSock.send(pkt, len); 
                    }
                }
            }
        }
        
        if (len <= 0) break;
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
         {
             PAutoLock lock(_mutex);
             len = _rtcpSock.recv(rtcpBuf, sizeof(rtcpBuf), ipRmt, portRmt);
             if (len > 0) {
                 if (_group) {
                     _group->onRtcpPacket(ipRmt, portRmt, rtcpBuf, len);
                 } else {
                      int srcIdx = -1;
                      if (_peers[0].active && portRmt == _peers[0].port + 1 && ipRmt == _peers[0].ip) srcIdx = 0;
                      else if (_peers[1].active && portRmt == _peers[1].port + 1 && ipRmt == _peers[1].ip) srcIdx = 1;

                      if (srcIdx != -1) {
                          int dstIdx = (srcIdx == 0) ? 1 : 0;
                          if (_peers[dstIdx].active) {
                              _rtcpSock.sendTo(rtcpBuf, len, &_peers[dstIdx].addrRtcp);
                          }
                      } else if (_peers[0].active && !_peers[1].active && srcIdx == 0) {
                           _rtcpSock.send(rtcpBuf, len);
                      }
                 }
             }
         }
         if (len <= 0) break;
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
                      _group->onVideoRtpPacket(ipRmt, portRmt, pkt, len);
                 } else {
                      int srcIdx = -1;
                      if (_peers[0].active && portRmt == _peers[0].videoPort && ipRmt == _peers[0].ip) srcIdx = 0;
                      else if (_peers[1].active && portRmt == _peers[1].videoPort && ipRmt == _peers[1].ip) srcIdx = 1;
                      
                      if (srcIdx != -1) {
                          int dstIdx = (srcIdx == 0) ? 1 : 0;
                          if (_peers[dstIdx].active && _peers[dstIdx].videoPort > 0) {
                              _videoRtpSock.sendTo(pkt, len, &_peers[dstIdx].addrVideoRtp);
                          }
                      } else if (_peers[0].active && !_peers[1].active && srcIdx == 0) {
                          _videoRtpSock.send(pkt, len);
                      }
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
        {
            PAutoLock lock(_mutex);
            len = _videoRtcpSock.recv(rtcpBuf, sizeof(rtcpBuf), ipRmt, portRmt);
            if (len > 0) {
                // Relay Video RTCP
                //printf("RX V-RTCP: len=%d from %s:%d\n", len, ipRmt.c_str(), portRmt);
                int srcIdx = -1;
                // RTCP depends, usually videoPort + 1
                if (_peers[0].active && portRmt == _peers[0].videoPort + 1 && ipRmt == _peers[0].ip) srcIdx = 0;
                else if (_peers[1].active && portRmt == _peers[1].videoPort + 1 && ipRmt == _peers[1].ip) srcIdx = 1;
                
                if (srcIdx != -1) {
                    int dstIdx = (srcIdx == 0) ? 1 : 0;
                    if (_peers[dstIdx].active && _peers[dstIdx].videoPort > 0) {
                        _videoRtcpSock.sendTo(rtcpBuf, len, &_peers[dstIdx].addrVideoRtcp);
                    }
                } else {
                    if (_peers[0].active && !_peers[1].active && srcIdx == 0) {
                        _videoRtcpSock.send(rtcpBuf, len);
                    }
                }
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

void PRtpTrans::reset() {
    PAutoLock lock(_mutex);
    _sessionId = "";
    _group = NULL;
    
    for(int i=0; i<2; ++i) {
        _peers[i].active = false;
        _peers[i].ip = "";
        _peers[i].port = 0;
        _peers[i].videoPort = 0;
        memset(&_peers[i].addrRtp, 0, sizeof(sockaddr_in));
        memset(&_peers[i].addrRtcp, 0, sizeof(sockaddr_in));
        memset(&_peers[i].addrVideoRtp, 0, sizeof(sockaddr_in));
        memset(&_peers[i].addrVideoRtcp, 0, sizeof(sockaddr_in));
    }
}
