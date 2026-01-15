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

    // Helper to fill sockaddr
    auto fillAddr = [](struct sockaddr_in& addr, const std::string& ip, int port) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
    };

    fillAddr(_peers[idx].addrRtp, ipRmt, portRmt);
    fillAddr(_peers[idx].addrRtcp, ipRmt, portRmt + 1);
    
    if (videoPortRmt > 0) {
        fillAddr(_peers[idx].addrVideoRtp, ipRmt, videoPortRmt);
        fillAddr(_peers[idx].addrVideoRtcp, ipRmt, videoPortRmt + 1);
    }

    // Default 'send' destination for legacy/single-peer logic?
    // We can just set it to Peer 0 if we assume Peer 0 is primary.
    // Or we leave valid socket dest as last updated?
    // Let's set it to Peer 0 always if active, or Peer 1 if 0 inactive?
    // Original code had one _addrRmt.
    // _rtpSock.setRmt updates _addrRmt inside CRtpSocket.
    // We can call it for this peer just to update CRtpSocket internal state if needed,
    // but our relay logic uses sendTo and ignores internal state for relaying.
    // However, if we utilize internal state for anything else...
    // Let's just update if idx==0 to be consistent with "Primary".
    
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
                if (len >= 12) {
                    unsigned char* p = (unsigned char*)pkt;
                    int v = (p[0] >> 6) & 0x03;
                    int pad = (p[0] >> 5) & 0x01;
                    int x = (p[0] >> 4) & 0x01;
                    int cc = p[0] & 0x0F;
                    int m = (p[1] >> 7) & 0x01;
                    int pt = p[1] & 0x7F;
                    unsigned short seq = (p[2] << 8) | p[3];
                    unsigned int ts = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
                    unsigned int ssrc = (p[8] << 24) | (p[9] << 16) | (p[10] << 8) | p[11];
                    printf("[RTP] len=%d %s:%d | V=%d P=%d X=%d CC=%d M=%d PT=%d Seq=%d TS=%u SSRC=0x%X\n", 
                           len, ipRmt.c_str(), portRmt, v, pad, x, cc, m, pt, seq, ts, ssrc);
                } else {
                    printf("RX A-RTP: len=%d from %s:%d (Too Short)\n", len, ipRmt.c_str(), portRmt);
                }
                if (_group) {
                    _group->onRtpPacket(_sessionId, pkt, len);
                } else {
                    // Relay Logic
                    // Identify Source
                    int srcIdx = -1;
                    if (_peers[0].active && portRmt == _peers[0].port && ipRmt == _peers[0].ip) srcIdx = 0;
                    else if (_peers[1].active && portRmt == _peers[1].port && ipRmt == _peers[1].ip) srcIdx = 1;
                    
                    // Also support latching for unknown/changed port?
                    // If ip matches but port dif?
                    // For now strict match.
                    
                    if (srcIdx != -1) {
                        // Relay to other
                        int dstIdx = (srcIdx == 0) ? 1 : 0;
                        if (_peers[dstIdx].active) {
                            _rtpSock.sendTo(pkt, len, &_peers[dstIdx].addrRtp);
                        }
                    } else {
                        // Unknown source or maybe one of the peers sending from new port?
                        // Fallback or just echo? Echo is bad for loop.
                        // If we have only 1 peer active, echo? (Echo test behavior)
                        if (_peers[0].active && !_peers[1].active && srcIdx == 0) {
                            // Echo back
                            _rtpSock.send(pkt, len); 
                        }
                    }
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
         {
             PAutoLock lock(_mutex);
             len = _rtcpSock.recv(rtcpBuf, sizeof(rtcpBuf), ipRmt, portRmt);
             if (len > 0) {
                 // Relay RTCP
                //printf("RX A-RTCP: len=%d from %s:%d\n", len, ipRmt.c_str(), portRmt);

                 if (!_group) {
                      int srcIdx = -1;
                      // Note: RTCP port usually +1
                      if (_peers[0].active && portRmt == _peers[0].port + 1 && ipRmt == _peers[0].ip) srcIdx = 0;
                      else if (_peers[1].active && portRmt == _peers[1].port + 1 && ipRmt == _peers[1].ip) srcIdx = 1; // Loose check

                      if (srcIdx != -1) {
                          int dstIdx = (srcIdx == 0) ? 1 : 0;
                          if (_peers[dstIdx].active) {
                              _rtcpSock.sendTo(rtcpBuf, len, &_peers[dstIdx].addrRtcp);
                          }
                      } else {
                          // If only 1 peer, echo?
                          if (_peers[0].active && !_peers[1].active && srcIdx == 0) {
                               // Echo back
                               _rtcpSock.send(rtcpBuf, len);
                          }
                      }
                 }
                 
                 // printf("RX RTCP: len=%d from %s:%d\n", len, ipRmt.c_str(), portRmt);
                 // fflush(stdout);
             }
         }
         
         if (len > 0 && _group) {
             _group->onRtcpPacket(_sessionId, rtcpBuf, len);
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
                //printf("RX V-RTP: len=%d from %s:%d\n", len, ipRmt.c_str(), portRmt);

                 if (_group) {
                      _group->onVideoRtpPacket(_sessionId, pkt, len);
                 } else {
                      // Relay Video
                      int srcIdx = -1;
                      if (_peers[0].active && portRmt == _peers[0].videoPort && ipRmt == _peers[0].ip) srcIdx = 0;
                      else if (_peers[1].active && portRmt == _peers[1].videoPort && ipRmt == _peers[1].ip) srcIdx = 1;
                      
                      if (srcIdx != -1) {
                          int dstIdx = (srcIdx == 0) ? 1 : 0;
                          if (_peers[dstIdx].active && _peers[dstIdx].videoPort > 0) {
                              _videoRtpSock.sendTo(pkt, len, &_peers[dstIdx].addrVideoRtp);
                          }
                      } else {
                          if (_peers[0].active && !_peers[1].active && srcIdx == 0) {
                              _videoRtpSock.send(pkt, len);
                          }
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
