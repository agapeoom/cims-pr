#ifndef __MCPTT_GROUP_H__
#define __MCPTT_GROUP_H__

#include <string>
#include <map>
#include <vector>
#include <set>
#include "pbase.h"

// Forward declaration
class PRtpTrans;

// RTCP APP Packet for Floor Control (Simplified)
// 3GPP TS 24.379 uses specific RTCP APP packets. 
// We will use a simplified structure for this implementation.

#define RTCP_PT_APP 204

enum FloorOpCode {
    FLOOR_REQUEST = 1,
    FLOOR_GRANT   = 2,
    FLOOR_REJECT  = 3,
    FLOOR_RELEASE = 4,
    FLOOR_IDLE    = 5,
    FLOOR_TAKEN   = 6,
    FLOOR_REVOKE  = 7
};

struct FloorControlPacket {
    unsigned char version_subtype; // V=2, P=0, Subtype=... 
    unsigned char type;            // PT=204 (APP)
    unsigned short length; 
    unsigned int ssrc;             // SSRC of sender
    char name[4];                  // "MCPT"
    unsigned char opcode;          // FloorOpCode
    unsigned char reserved[3];     // Padding/Reserved
    unsigned int user_id;          // User ID requesting/holding floor (simplified)
};

class McpttGroup {
public:
    McpttGroup(const std::string& groupId);
    virtual ~McpttGroup();

    void addMember(const std::string& sessionId, PRtpTrans* session);
    void removeMember(const std::string& sessionId);
    bool hasMember(const std::string& sessionId);

    // Floor Control Logic
    void handleFloorRequest(const std::string& sessionId, unsigned int userId);
    void handleFloorRelease(const std::string& sessionId, unsigned int userId);

    // Called by PRtpTrans when an RTCP packet is received
    void onRtcpPacket(const std::string& sessionId, char* buf, int len);

    // Called by PRtpTrans when an RTP packet is received
    void onRtpPacket(const std::string& sessionId, char* buf, int len);

    // Called by PRtpTrans when a Video RTP packet is received
    void onVideoRtpPacket(const std::string& sessionId, char* buf, int len);

private:
    void sendToAll(const char* data, int len);
    void sendToMember(const std::string& sessionId, const char* data, int len);
    void broadcastFloorStatus(unsigned char opcode, unsigned int userId);

    std::string _groupId;
    std::map<std::string, PRtpTrans*> _members; // SessionID -> Session
    
    // Floor State
    bool _floorTaken;
    std::string _floorOwnerSessionId;
    unsigned int _floorOwnerUserId;

    PMutex _mutex;
};

#endif // __MCPTT_GROUP_H__
