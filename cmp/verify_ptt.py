import socket
import sys
import time
import struct
import threading

CMP_IP = "127.0.0.1"
CMP_PORT = 9000

# Constants
FLOOR_REQUEST = 1
FLOOR_GRANT   = 2
FLOOR_REJECT  = 3
FLOOR_RELEASE = 4
FLOOR_IDLE    = 5
FLOOR_TAKEN   = 6
FLOOR_REVOKE  = 7

class SimulatedUser:
    def __init__(self, name, local_rtp_port, remote_rtp_port, user_id):
        self.name = name
        self.sess_id = name 
        # Audio
        self.local_rtp_port = local_rtp_port      # Server Listen Audio
        # self.local_rtcp_port = local_rtp_port + 1
        self.remote_rtp_port = remote_rtp_port    # Client Listen Audio (User receives here)
        self.remote_rtcp_port = remote_rtp_port + 1
        
        # Video
        self.local_video_port = local_rtp_port + 2   # Server Listen Video
        self.remote_video_port = remote_rtp_port + 2 # Client Listen Video (User receives here)
        
        self.user_id = user_id
        
        # Audio RTCP (Client Listen)
        self.rtcp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.rtcp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.rtcp_sock.bind(("127.0.0.1", self.remote_rtcp_port))
        self.rtcp_sock.settimeout(0.5)

        # Audio RTP (Client Listen)
        self.rtp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.rtp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.rtp_sock.bind(("127.0.0.1", self.remote_rtp_port))
        self.rtp_sock.settimeout(0.5)

        # Video RTP (Client Listen)
        self.video_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.video_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.video_sock.bind(("127.0.0.1", self.remote_video_port))
        self.video_sock.settimeout(0.5)
        
        # Send Socket (Any port)
        self.send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        # Will be updated by ADD response
        self.server_ip = CMP_IP 

    def close(self):
        self.rtcp_sock.close()
        self.rtp_sock.close()
        self.video_sock.close()
        self.send_sock.close()

    def send_cmd(self, cmd):
        self.send_sock.sendto(cmd.encode(), (CMP_IP, CMP_PORT))
        
    def send_floor_request(self):
        print(f"[{self.name}] Sending Floor Request")
        pkt = self._build_rtcp(FLOOR_REQUEST)
        # Send to Server Audio RTCP Port (server audio port + 1)
        # Note: We must use the ASSIGNED server audio port for RTCP
        print(f"[{self.name}] Sending Floor Request to {self.server_ip}:{self.local_rtp_port + 1}")
        self.send_sock.sendto(pkt, (self.server_ip, self.local_rtp_port + 1))

    def send_floor_release(self):
        print(f"[{self.name}] Sending Floor Release")
        pkt = self._build_rtcp(FLOOR_RELEASE)
        self.send_sock.sendto(pkt, (self.server_ip, self.local_rtp_port + 1))

    def _build_rtcp(self, opcode):
        version = 0x80
        pt = 204
        length = 5
        ssrc = 12345
        name = b"MCPT"
        reserved = b"\x00\x00\x00"
        return struct.pack("!BBHI4sB3sI", version, pt, length, ssrc, name, opcode, reserved, self.user_id)

    def drain_rtcp(self):
        try:
            while True:
                self.rtcp_sock.recvfrom(2048)
        except socket.timeout:
            pass
        except BlockingIOError:
            pass

    def send_rtp_audio(self, payload):
        # AMR-WB PT 97 ( Dynamic 96-127)
        pkt = struct.pack("!BBHII", 0x80, 97, 0, 0, 0) + payload 
        self.send_sock.sendto(pkt, (self.server_ip, self.local_rtp_port))

    def send_rtp_video(self, payload):
        # H.265 PT 96
        pkt = struct.pack("!BBHII", 0x80, 96, 0, 0, 0) + payload 
        self.send_sock.sendto(pkt, (self.server_ip, self.local_video_port))

    def receive_rtp_audio(self, timeout=1.0):
        try:
            self.rtp_sock.settimeout(timeout)
            data, _ = self.rtp_sock.recvfrom(1024)
            return len(data) > 0
        except:
             return False

    def receive_rtp_video(self, timeout=1.0):
        try:
            self.video_sock.settimeout(timeout)
            data, _ = self.video_sock.recvfrom(1024)
            return len(data) > 0
        except:
             return False

    def wait_for_opcode(self, expected_opcode, timeout=3.0):
        start = time.time()
        while time.time() - start < timeout:
            try:
                data, _ = self.rtcp_sock.recvfrom(2048)
                if len(data) >= 20:
                     version, pt, length, ssrc, name, opcode, reserved, uid = struct.unpack("!BBHI4sB3sI", data[:20])
                     print(f"[{self.name}] RX RTCP OpCode={opcode}")
                     if opcode == expected_opcode:
                         if opcode == FLOOR_GRANT:
                             print(f"[{self.name}] Received GRANT")
                         elif opcode == FLOOR_IDLE:
                             print(f"[{self.name}] Received IDLE")
                         return True
                     if opcode == FLOOR_TAKEN:
                         pass # Ignore Taken messages while waiting for specific
            except socket.timeout:
                continue
        return False

# Main Test Logic
def run_test(args):
    print("--- Verify PTT (Group & Floor Control) ---")

    # Setup global command socket for setup
    cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    cmd_sock.settimeout(2)
    
    def send_global(cmd):
        cmd_sock.sendto(cmd.encode(), (CMP_IP, CMP_PORT))
        try:
            data, _ = cmd_sock.recvfrom(1024)
            return data.decode()
        except:
            return None

    if send_global("ALIVE") != "OK":
        print("Server not alive")
        return

    # Create Groups
    print("Creating Groups...")
    send_global("addGroup grp1")
    send_global("addGroup grp2")

    # Create Users
    # Grp1: u1, u2, u3
    # Grp2: u4, u5, u6
    # Create Users
    # Grp1: u1, u2, u3
    # Grp2: u4, u5, u6
    users = []
    base_client_port = 60000 
    
    for i in range(1, 7):
        # We only know client listening ports here
        client_audio_port = base_client_port
        client_video_port = base_client_port + 2
        
        u = SimulatedUser(f"u{i}", 0, client_audio_port, 1000+i) # local_rtp_port 0 initially
        u.remote_video_port = client_video_port
        # Re-bind video sock because init used old logic?
        # Let's check SimulatedUser init. 
        # It uses remote_rtp_port passed in.
        # And remote_video_port is calculated as remote_rtp_port + 2
        # So we are good if we pass client_audio_port.
        
        # Now ADD to server
        # ADD <id> <rmt_ip> <rmt_audio> <rmt_video>
        resp = send_global(f"ADD {u.sess_id} 127.0.0.1 {client_audio_port} {client_video_port}")
        
        if resp and resp.startswith("OK"):
            parts = resp.split()
            # OK <loc_ip> <loc_audio> <loc_video>
            server_ip = parts[1]
            server_audio_port = int(parts[2])
            server_video_port = int(parts[3])
            
            u.server_ip = server_ip
            u.local_rtp_port = server_audio_port
            u.local_video_port = server_video_port
            print(f"User {u.name} assigned server ports: IP={server_ip} A={server_audio_port} V={server_video_port}")
        else:
            print(f"Failed to ADD user {u.name}: {resp}")
            return

        users.append(u)
        base_client_port += 10
    
    time.sleep(0.5)

    # Join Groups
    print("Joining Groups...")
    for i in range(3):
        send_global(f"joinGroup grp1 {users[i].sess_id}")
    for i in range(3, 6):
        send_global(f"joinGroup grp2 {users[i].sess_id}")

    time.sleep(1)

    print("\n--- Starting Round Testing ---")
    
    # Run test for 2 rounds
    for round_num in range(1, 3):
        print(f"\n=== Round {round_num} ===")
        
        # Test Group 1 Sequence
        print("--- Group 1 Sequence ---")
        for i in range(3):
            active_user = users[i]
            print(f" -> User {active_user.name} requesting floor (Grp1)")
            
            # Drain old messages
            for u in users: u.drain_rtcp()
            
            active_user.send_floor_request()
            
            # Expect Grant
            if not active_user.wait_for_opcode(FLOOR_GRANT):
                print(f"FAILED: User {active_user.name} did not get GRANT")
                return
            
            # Send RTP Voice (AMR-WB PT 97)
            print(f"[{active_user.name}] Sending Audio (AMR-WB)...")
            active_user.send_rtp_audio(b"AudioData")

            # Send RTP Video (H.265 PT 96)
            print(f"[{active_user.name}] Sending Video (H.265)...")
            active_user.send_rtp_video(b"VideoData")
            
            # Verify others received it
            # Group 1 members: u1, u2, u3
            # If u1 is active, u2 and u3 should receive
            if round_num == 1: # Only checking extensively in round 1 for brevity
                grp_members = users[:3] if i < 3 else users[3:]
                for m in grp_members:
                    if m != active_user:
                        # Check Audio
                        if m.receive_rtp_audio(timeout=1.0):
                            print(f"[{m.name}] Received Audio")
                        else:
                            print(f"FAILED: [{m.name}] Did NOT receive Audio")
                            return
                        
                        # Check Video
                        if m.receive_rtp_video(timeout=1.0):
                            print(f"[{m.name}] Received Video")
                        else:
                            print(f"FAILED: [{m.name}] Did NOT receive Video")
                            return

            time.sleep(0.5) # Speak for 0.5s
            
            active_user.send_floor_release()
            
            if not active_user.wait_for_opcode(FLOOR_IDLE):
                print(f"FAILED: User {active_user.name} did not see IDLE")
                return

        # Test Group 2 Sequence
        print("--- Group 2 Sequence ---")
        for i in range(3, 6):
            active_user = users[i]
            print(f" -> User {active_user.name} requesting floor (Grp2)")
            
            for u in users: u.drain_rtcp()
            
            active_user.send_floor_request()
            
            if not active_user.wait_for_opcode(FLOOR_GRANT):
                print(f"FAILED: User {active_user.name} did not get GRANT")
                return
            
            time.sleep(0.5)
            
            active_user.send_floor_release()
            
            if not active_user.wait_for_opcode(FLOOR_IDLE):
                print(f"FAILED: User {active_user.name} did not see IDLE")
                return

    print("\nSUCCESS: All rounds completed.")
    
    # Cleanup
    if not args.no_cleanup:
        send_global("removeGroup grp1")
        send_global("removeGroup grp2")
        for u in users: 
            send_global(f"REMOVE {u.sess_id}")
            u.close()
    else:
        print("Skipping cleanup (--no-cleanup set)")
        for u in users: u.close()

import argparse
import subprocess

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Verify PTT Logic")
    parser.add_argument("port", type=int, nargs="?", default=9000, help="CMP Server Port")
    parser.add_argument("--pcap", type=str, help="PCAP filename to capture traffic")
    parser.add_argument("--no-cleanup", action="store_true", help="Don't remove groups/sessions at end")
    args = parser.parse_args()

    CMP_PORT = args.port
    
    tcpdump_proc = None
    if args.pcap:
        print(f"Starting PCAP capture to {args.pcap} on interface lo...")
        # Capture raw packets on loopback
        # -U: Packet-buffered (write to file immediately)
        tcpdump_proc = subprocess.Popen(
            ["tcpdump", "-i", "lo", "-U", "-w", args.pcap], 
            stdout=subprocess.DEVNULL, 
            stderr=subprocess.DEVNULL
        )
        time.sleep(1) # Give tcpdump a moment to start

    try:
        run_test(args)
    finally:
        if tcpdump_proc:
             print("Stopping PCAP capture...")
             tcpdump_proc.terminate()
             tcpdump_proc.wait()
