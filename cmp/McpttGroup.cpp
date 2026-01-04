#include "McpttGroup.h"
#include "PRtpHandler.h"
#include <cstring>
#include <arpa/inet.h>

McpttGroup::McpttGroup(const std::string& groupId) 
    : _groupId(groupId), _floorTaken(false), _floorOwnerUserId(0)
{
}

McpttGroup::~McpttGroup() {
    PAutoLock lock(_mutex);
    _members.clear();
}

void McpttGroup::addMember(const std::string& sessionId, PRtpTrans* session) {
    PAutoLock lock(_mutex);
    _members[sessionId] = session;
    printf("[McpttGroup:%s] Member %s joined.\n", _groupId.c_str(), sessionId.c_str());
    
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

    if (_floorTaken && _floorOwnerSessionId == sessionId) {
        // Owner left, release floor
        _floorTaken = false;
        _floorOwnerSessionId = "";
        _floorOwnerUserId = 0;
        broadcastFloorStatus(FLOOR_IDLE, 0);
        printf("[McpttGroup:%s] Floor owner left. Floor IDLE.\n", _groupId.c_str());
    }
}

bool McpttGroup::hasMember(const std::string& sessionId) {
    PAutoLock lock(_mutex);
    return _members.find(sessionId) != _members.end();
}

void McpttGroup::onRtcpPacket(const std::string& sessionId, char* buf, int len) {
    if (len < sizeof(FloorControlPacket)) {
         printf("RTCP len %d < %ld\n", len, sizeof(FloorControlPacket));
         fflush(stdout);
         return;
    }
    
    FloorControlPacket* pkt = (FloorControlPacket*)buf;
    
    // Validate PT and Name
    if (pkt->type != RTCP_PT_APP) {
         printf("RTCP PT mismatch. Expected %d, Got %d\n", RTCP_PT_APP, pkt->type);
         fflush(stdout);
         return;
    }
    if (memcmp(pkt->name, "MCPT", 4) != 0) {
         printf("RTCP Name mismatch. Got %.4s\n", pkt->name);
         fflush(stdout);
         return;
    }

    unsigned int userId = ntohl(pkt->user_id);
    unsigned char opcode = pkt->opcode;
    
    printf("RTCP App Valid. OpCode=%d UserId=%u\n", opcode, userId);
    fflush(stdout);

    // Dispatch
    switch(opcode) {
        case FLOOR_REQUEST:
            handleFloorRequest(sessionId, userId);
            break;
        case FLOOR_RELEASE:
            handleFloorRelease(sessionId, userId);
            break;
        default:
            printf("Unknown opcode %d\n", opcode);
            fflush(stdout);
            break;
    }
}

void McpttGroup::onRtpPacket(const std::string& sessionId, char* buf, int len) {
    PAutoLock lock(_mutex);
    
    // Only forward if floor is taken and sender is owner
    if (_floorTaken && _floorOwnerSessionId == sessionId) {
        for (auto const& [sid, session] : _members) {
            // Don't echo back to sender
            if (sid != sessionId && session) {
                 session->sendRtp(buf, len);
            }
        }
    }
}

void McpttGroup::onVideoRtpPacket(const std::string& sessionId, char* buf, int len) {
    PAutoLock lock(_mutex);
    
    // Only forward if floor is taken and sender is owner
    if (_floorTaken && _floorOwnerSessionId == sessionId) {
        for (auto const& [sid, session] : _members) {
            // Don't echo back to sender
            if (sid != sessionId && session) {
                 session->sendVideoRtp(buf, len);
            }
        }
    }
}

void McpttGroup::handleFloorRequest(const std::string& sessionId, unsigned int userId) {
    PAutoLock lock(_mutex);
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
        
        printf("[McpttGroup:%s] Floor GRANTED to User %u (Session %s)\n", _groupId.c_str(), userId, sessionId.c_str());
    } else {
        // Reject or Queue (For now, Reject)
        if (_floorOwnerSessionId == sessionId) {
            // Already owner, ignore or re-grant?
             return;
        }

        FloorControlPacket response;
        memset(&response, 0, sizeof(response));
        response.version_subtype = 0x80;
        response.type = RTCP_PT_APP;
        response.length = htons(sizeof(FloorControlPacket)/4 - 1);
        memcpy(response.name, "MCPT", 4);
        response.opcode = FLOOR_REJECT;
        response.user_id = htonl(userId);
        
        sendToMember(sessionId, (char*)&response, sizeof(response));
         printf("[McpttGroup:%s] Floor REJECTED for User %u (Session %s). Owned by %u\n", _groupId.c_str(), userId, sessionId.c_str(), _floorOwnerUserId);
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
    
    sendToAll((char*)&pkt, sizeof(pkt));
}

void McpttGroup::sendToAll(const char* data, int len) {
    for (auto const& [sid, session] : _members) {
        if (session) {
            // session->sendRtcp(data, len); 
            // We need to add sendRtcp to PRtpTrans
             session->sendRtcp((char*)data, len);
        }
    }
}

void McpttGroup::sendToMember(const std::string& sessionId, const char* data, int len) {
    if (_members.find(sessionId) != _members.end()) {
        _members[sessionId]->sendRtcp((char*)data, len);
    }
}
