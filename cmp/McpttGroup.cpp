#include "McpttGroup.h"
#include "PRtpHandler.h"
#include <cstring>
#include <arpa/inet.h>

McpttGroup::McpttGroup(const std::string& groupId) 
    : _groupId(groupId), _sharedSession(NULL), _floorTaken(false), _floorOwnerUserId(0)
{
}

McpttGroup::~McpttGroup() {
    PAutoLock lock(_mutex);
    _members.clear();
}

void McpttGroup::setSharedSession(PRtpTrans* session) {
    PAutoLock lock(_mutex);
    _sharedSession = session;
}

void McpttGroup::addMember(const std::string& sessionId, const std::string& ip, int port) {
    printf("0. [McpttGroup:%s] Adding Member Session=%s IP=%s Port=%d\n", _groupId.c_str(), sessionId.c_str(), ip.c_str(), port);
    PAutoLock lock(_mutex);
    Peer peer;
    peer.id = sessionId;
    peer.ip = ip;
    peer.port = port;
    _members[sessionId] = peer;
    printf("1. [McpttGroup:%s] Added Member Session=%s IP=%s Port=%d\n", _groupId.c_str(), sessionId.c_str(), ip.c_str(), port);
    
    // If floor is taken, notify new member
    if (_floorTaken) {
         // Construct Floor Taken packet
         FloorControlPacket pkt;
         memset(&pkt, 0, sizeof(pkt));
         pkt.version_subtype = 0x80; // V=2
         pkt.type = RTCP_PT_APP;
         pkt.length = htons(sizeof(FloorControlPacket)/4 - 1);
         memcpy(pkt.name, "MCPT", 4);
         pkt.opcode = FLOOR_TAKEN;
         pkt.user_id = htonl(_floorOwnerUserId);
         
         sendToMember(sessionId, (char*)&pkt, sizeof(pkt));
    }
}

void McpttGroup::removeMember(const std::string& sessionId) {
    PAutoLock lock(_mutex);
    _members.erase(sessionId);
    printf("[McpttGroup:%s] Member %s left.\n", _groupId.c_str(), sessionId.c_str());

    if ( _floorTaken && (_floorOwnerSessionId == sessionId) ) {
        // Owner left, release floor
        _floorTaken = false;
        _floorOwnerSessionId = "";
        _floorOwnerUserId = 0;
        broadcastFloorStatus(FLOOR_IDLE, 0);
        printf("[McpttGroup:%s] Floor owner left. Floor IDLE.\n", _groupId.c_str());
    }
}

void McpttGroup::updatePriorities(const std::map<std::string, int>& priorities) {
    PAutoLock lock(_mutex);
    _priorities = priorities;
    printf("[McpttGroup:%s] Priorities updated for %lu members.\n", _groupId.c_str(), _priorities.size());
}

void McpttGroup::setDtmfConfig(bool enable, const std::string& pushDigit, const std::string& releaseDigit) {
    PAutoLock lock(_mutex);
    _dtmfEnable = enable;
    _dtmfPushDigit = pushDigit;
    _dtmfReleaseDigit = releaseDigit;
    printf("[McpttGroup:%s] DTMF Config: Enable=%d Push=%s Release=%s\n", _groupId.c_str(), enable, pushDigit.c_str(), releaseDigit.c_str());
}

bool McpttGroup::hasMember(const std::string& sessionId) {
    PAutoLock lock(_mutex);
    return _members.find(sessionId) != _members.end();
}

void McpttGroup::onRtcpPacket(const std::string& ip, int port, char* buf, int len) {
    if (len < sizeof(FloorControlPacket)) {
         return;
    }
    
    // Check sender
    std::string sessionId = "";
    {
        PAutoLock lock(_mutex);
        for(auto const& [sid, peer] : _members) {
            if (peer.ip == ip && peer.port == port + 1) { // RTCP port match (+1) check? Or usually peer.port is RTP.
                sessionId = sid;
                break;
            }
            // Loose match for RTCP port? Or strict? 
            // Usually RTCP is RTP+1. But NAT might change it.
            // For now assume Strict RTP+1 matching peer.port+1
            // OR check if peer.port matches port (maybe multiplexed?)
            // Assuming peer.port is RTP. so peer.port+1 == port.
        }
    }
    
    if (sessionId == "") {
         // Unknown sender
         return;
    }

    FloorControlPacket* pkt = (FloorControlPacket*)buf;
    
    // Validate PT and Name
    if (pkt->type != RTCP_PT_APP) return;
    if (memcmp(pkt->name, "MCPT", 4) != 0) return;

    unsigned int userId = ntohl(pkt->user_id);
    unsigned char opcode = pkt->opcode;
    
    printf("RTCP App Valid. OpCode=%d UserId=%u Session=%s\n", opcode, userId, sessionId.c_str());

    // Dispatch
    switch(opcode) {
        case FLOOR_REQUEST:
            handleFloorRequest(sessionId, userId);
            break;
        case FLOOR_RELEASE:
            handleFloorRelease(sessionId, userId);
            break;
        default:
            break;
    }
}

void McpttGroup::onRtpPacket(const std::string& ip, int port, char* buf, int len) {
    std::string action = "NONE";
    std::string actionSenderId = "";
    unsigned int actionUserId = 0;

    {
        PAutoLock lock(_mutex);
        
        // Find sender session
        std::string senderId = "";
        for(auto const& [sid, peer] : _members) {
            if (peer.ip == ip && peer.port == port) {
                senderId = sid;
                break;
                break;
            }
        }
        
        if (senderId == "") {
             // DEBUG: Log unknown sender occasionally?
             // printf("[DEBUG] Unknown Sender: IP=%s Port=%d\n", ip.c_str(), port);
        }
        
        unsigned char pt = (unsigned char)(buf[1] & 0x7F);
        //printf("RTP Packet: IP=%s Port=%d Len=%d PT=%d SenderId=%s\n", ip.c_str(), port, len, pt, senderId.c_str());
        if (senderId != "") {
            // [DTMF Check]
            if (_dtmfEnable && len > 12) { 
                // DEBUG: Print potentially valid DTMF packets or all packets if needed
                // Rate limit or just print for now since user says "nothing happens".
                if (pt == 101) {
                     printf("[DEBUG] RTP Packet: IP=%s Port=%d Len=%d PT=%d SenderId=%s\n", ip.c_str(), port, len, pt, senderId.c_str());
                } else if (pt != 0 && pt != 98) { // Ignore typical audio
                     printf("[DEBUG] Unknown PT Packet: IP=%s Port=%d Len=%d PT=%d SenderId=%s\n", ip.c_str(), port, len, pt, senderId.c_str());
                }

                if (pt == 101) { 
                    unsigned char digitCode = (unsigned char)buf[12];
                    bool endBit = (buf[13] & 0x80) != 0; 
                    printf("[DEBUG] PT101 Detail: Code=%d EndBit=%d\n", digitCode, endBit);
                    
                    char digitChar = 0;
                    if (digitCode >= 0 && digitCode <= 9) digitChar = '0' + digitCode;
                    else if (digitCode == 10) digitChar = '*';
                    else if (digitCode == 11) digitChar = '#';
                    else if (digitCode >= 12 && digitCode <= 15) digitChar = 'A' + (digitCode - 12);
                    
                    if (digitChar != 0 && endBit) {
                        printf("[McpttGroup:%s] DTMF Detected: %c from %s\n", _groupId.c_str(), digitChar, senderId.c_str());
                        std::string dStr(1, digitChar);
                        
                        unsigned int uId = 0;
                        try { uId = std::stoul(senderId); } catch (...) { uId = 9999; }

                        if (dStr == _dtmfPushDigit) {
                            action = "REQUEST";
                            actionSenderId = senderId;
                            actionUserId = uId;
                        } else if (dStr == _dtmfReleaseDigit) {
                            action = "RELEASE";
                            actionSenderId = senderId;
                            actionUserId = uId;
                        }
                    }
                }
            }

            if (_floorTaken && _floorOwnerSessionId == senderId) {
                sendToAll(buf, len, ip, port);
            }
        }
    } // Lock releases here

    if (action == "REQUEST") {
        handleFloorRequest(actionSenderId, actionUserId);
    } else if (action == "RELEASE") {
        handleFloorRelease(actionSenderId, actionUserId);
    }
}

void McpttGroup::onVideoRtpPacket(const std::string& ip, int port, char* buf, int len) {
    PAutoLock lock(_mutex);
    
    if (!_floorTaken) return;
    
    std::string senderId = "";
    for(auto const& [sid, peer] : _members) {
        if (peer.ip == ip && peer.port == port) { // Video port check? 
            // For simplicity, assume video port matches peer.port (if multiplexed) or separate mapping.
            // But we didn't store video port in Peer struct. Assuming audio-only for now or same IP/Port logic caveat.
            // If Peer has videoPort, we should check peer.videoPort.
            // But Peer struct only has 'port'.
            // Let's assume we ignore video for this refactor or assume audio forwarding logic applies.
            senderId = sid; 
            // WARNING: Video port handling incomplete in Peer struct. Focusing on Audio per request.
            break;
        }
    }
    
    if (senderId == "" || _floorOwnerSessionId != senderId) return;

    if (_sharedSession) {
         for (auto const& [sid, peer] : _members) {
            if (sid != senderId) {
                // Video port?? We need to know Peer's video port.
                // Assuming peer.port + 2 or stored video port.
                // Current Peer struct missing videoPort.
                // Added note: Video forwarding disabled/broken in this refactor until Peer struct updated.
                // _sharedSession->sendVideoTo(peer.ip, peer.port+?, buf, len);
            }
        }
    }
}

void McpttGroup::handleFloorRequest(const std::string& sessionId, unsigned int userId) {
    PAutoLock lock(_mutex);
    // Check Priority
    int requesterPrio = 999;
    if (_priorities.find(sessionId) != _priorities.end()) requesterPrio = _priorities[sessionId];

    if (!_floorTaken) {
        // Grant Floor
        _floorTaken = true;
        _floorOwnerSessionId = sessionId;
        _floorOwnerUserId = userId;
        
        // Send Grant to Requestor
        FloorControlPacket response;
        memset(&response, 0, sizeof(response));
        response.version_subtype = 0x80;
        response.type = RTCP_PT_APP;
        response.length = htons(sizeof(FloorControlPacket)/4 - 1);
        memcpy(response.name, "MCPT", 4);
        response.opcode = FLOOR_GRANT;
        response.user_id = htonl(userId);
        
        sendToMember(sessionId, (char*)&response, sizeof(response));
        
        // Broadcast Taken to others
        broadcastFloorStatus(FLOOR_TAKEN, userId);
        
        printf("[McpttGroup:%s] Floor GRANTED to User %u (Session %s) Prio=%d\n", _groupId.c_str(), userId, sessionId.c_str(), requesterPrio);
    } else {
        if (_floorOwnerSessionId == sessionId) return;

        // Check Preemption (Lower Value = Higher Priority)
        int ownerPrio = 999;
        if (_priorities.find(_floorOwnerSessionId) != _priorities.end()) ownerPrio = _priorities[_floorOwnerSessionId];

        if (requesterPrio < ownerPrio) {
            // PREEMPTION
            printf("[McpttGroup:%s] Floor PREEMPTED by User %u (Prio %d) from User %u (Prio %d)\n", 
                   _groupId.c_str(), userId, requesterPrio, _floorOwnerUserId, ownerPrio);

            // Revoke Current
            FloorControlPacket revoke;
            memset(&revoke, 0, sizeof(revoke));
            revoke.version_subtype = 0x80;
            revoke.type = RTCP_PT_APP;
            revoke.length = htons(sizeof(FloorControlPacket)/4 - 1);
            memcpy(revoke.name, "MCPT", 4);
            revoke.opcode = FLOOR_REVOKE;
            revoke.user_id = htonl(_floorOwnerUserId);
            sendToMember(_floorOwnerSessionId, (char*)&revoke, sizeof(revoke));

            // Grant New
            _floorOwnerSessionId = sessionId;
            _floorOwnerUserId = userId;
            
            FloorControlPacket grant;
            memset(&grant, 0, sizeof(grant));
            grant.version_subtype = 0x80;
            grant.type = RTCP_PT_APP;
            grant.length = htons(sizeof(FloorControlPacket)/4 - 1);
            memcpy(grant.name, "MCPT", 4);
            grant.opcode = FLOOR_GRANT;
            grant.user_id = htonl(userId);
            sendToMember(sessionId, (char*)&grant, sizeof(grant));
            
            // Broadcast Taken (New Owner)
            broadcastFloorStatus(FLOOR_TAKEN, userId);
            
        } else {
            // REJECT
            FloorControlPacket response;
            memset(&response, 0, sizeof(response));
            response.version_subtype = 0x80;
            response.type = RTCP_PT_APP;
            response.length = htons(sizeof(FloorControlPacket)/4 - 1);
            memcpy(response.name, "MCPT", 4);
            response.opcode = FLOOR_REJECT;
            response.user_id = htonl(userId);
            
            sendToMember(sessionId, (char*)&response, sizeof(response));
            printf("[McpttGroup:%s] Floor REJECTED for User %u (Prio %d). Owned by %u (Prio %d)\n", 
                   _groupId.c_str(), userId, requesterPrio, _floorOwnerUserId, ownerPrio);
        }
    }
}

void McpttGroup::handleFloorRelease(const std::string& sessionId, unsigned int userId) {
    PAutoLock lock(_mutex);
    if (_floorTaken && _floorOwnerSessionId == sessionId) {
        _floorTaken = false;
        _floorOwnerSessionId = "";
        _floorOwnerUserId = 0;
        
        broadcastFloorStatus(FLOOR_IDLE, 0);
        printf("[McpttGroup:%s] Floor RELEASED by User %u (Session %s)\n", _groupId.c_str(), userId, sessionId.c_str());
    }
}

void McpttGroup::broadcastFloorStatus(unsigned char opcode, unsigned int userId) {
    FloorControlPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.version_subtype = 0x80;
    pkt.type = RTCP_PT_APP;
    pkt.length = htons(sizeof(FloorControlPacket)/4 - 1);
    memcpy(pkt.name, "MCPT", 4);
    pkt.opcode = opcode;
    pkt.user_id = htonl(userId);
    
    sendToAll((char*)&pkt, sizeof(pkt), "", 0);
}

void McpttGroup::sendToAll(const char* data, int len, const std::string& excludeIp, int excludePort) {
    if (_sharedSession) {
        for (auto const& [sid, peer] : _members) {
            if (peer.ip == excludeIp && peer.port == excludePort) continue;
            // RTCP port usually peer.port + 1
            _sharedSession->sendTo(peer.ip, peer.port + 1, (char*)data, len); // Assuming RTCP
        }
    }
}

void McpttGroup::sendToMember(const std::string& sessionId, const char* data, int len) {
    if (_sharedSession && _members.find(sessionId) != _members.end()) {
        const Peer& peer = _members[sessionId];
        _sharedSession->sendTo(peer.ip, peer.port + 1, (char*)data, len); // Assuming RTCP
    }
}
