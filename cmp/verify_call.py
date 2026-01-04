import socket
import sys
import time
import struct
import threading
import argparse

IP = "127.0.0.1"
CMP_PORT = 9000

# Session 1
LOC_IP = "0.0.0.0"
LOC_AUDIO_PORT = 50000
LOC_VIDEO_PORT = 50002

# Simulating the remote side (Client)
RMT_IP = "127.0.0.1"
RMT_AUDIO_PORT = 60000
RMT_VIDEO_PORT = 60002

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2)

def send_cmd(cmd):
    print(f"Sending CMD: {cmd}")
    sock.sendto(cmd.encode(), (IP, CMP_PORT))
    try:
        data, addr = sock.recvfrom(1024)
        print(f"Received CMD: {data.decode()}")
        return data.decode()
    except socket.timeout:
        print("Timeout waiting for response")
        return None

def verify_echo(port, label, pt, payload, server_ip, server_port):
    # Setup socket to receive echo
    recv_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    recv_sock.bind((RMT_IP, port))
    recv_sock.settimeout(2.0)

    # Setup socket to send to server
    print(f"[{label}] Sending RTP (PT={pt}) to Server {server_ip}:{server_port}...")
    
    # RTP Header
    seq = 100
    ts = 123456
    ssrc = 999
    # V=2, P=0, X=0, CC=0, M=0, PT=..., Seq=..., TS=..., SSRC=...
    header = struct.pack("!BBHII", 0x80, pt, seq, ts, ssrc)
    pkt = header + payload
    
    send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    send_sock.sendto(pkt, (server_ip, server_port))
    send_sock.close()
    
    # Receive Echo
    try:
        data, _ = recv_sock.recvfrom(2048)
        recv_sock.close()
        
        # Parse minimal header
        if len(data) < 12:
            print(f"[{label}] FAILED: Too short")
            return False
            
        v_p_x_cc, r_pt, r_seq, r_ts, r_ssrc = struct.unpack("!BBHII", data[:12])
        r_pt = r_pt & 0x7F
        
        if r_pt == pt and data[12:] == payload:
            print(f"[{label}] SUCCESS: Received valid echo")
            return True
        else:
            print(f"[{label}] FAILED: Content mismatch (PT={r_pt})")
            return False
            
    except socket.timeout:
        print(f"[{label}] FAILED: Timeout (No echo)")
        recv_sock.close()
        return False


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("port", type=int, nargs="?", default=9000)
    args = parser.parse_args()
    CMP_PORT = args.port
    
    print("--- Verify Call (Audio & Video Echo) ---")

    # 1. ALIVE
    if send_cmd("ALIVE") != "OK": sys.exit(1)

    # 2. Add Session with Video Ports (Dynamic Allocation)
    # ADD <id> <rmt_ip> <rmt_audio> <rmt_video>
    # Response: OK <loc_ip> <loc_audio> <loc_video>
    cmd = f"ADD session1 {RMT_IP} {RMT_AUDIO_PORT} {RMT_VIDEO_PORT}"
    resp = send_cmd(cmd)
    
    if not resp or not resp.startswith("OK"):
        print(f"FAILED: ADD command failed: {resp}")
        sys.exit(1)
        
    parts = resp.split()
    if len(parts) < 4:
        print(f"FAILED: Invalid ADD response format: {resp}")
        sys.exit(1)
        
    server_ip = parts[1]
    server_audio_port = int(parts[2])
    server_video_port = int(parts[3])
    
    print(f"Assigned Server Ports: Audio={server_audio_port}, Video={server_video_port}")

    # 3. Test Audio Echo (AMR-WB PT 97)
    # We send to the assigned server_audio_port
    if not verify_echo(RMT_AUDIO_PORT, "Audio", 97, b"AudioPayload123", server_ip, server_audio_port):
        send_cmd("REMOVE session1")
        sys.exit(1)

    # 4. Test Video Echo (H.265 PT 96)
    # We send to the assigned server_video_port
    if not verify_echo(RMT_VIDEO_PORT, "Video", 96, b"VideoPayload456", server_ip, server_video_port):
        send_cmd("REMOVE session1")
        sys.exit(1)
    
    # 5. Remove Session
    if send_cmd("REMOVE session1") != "OK": sys.exit(1)

    print("Call Session Test PASSED")
    sys.exit(0)
