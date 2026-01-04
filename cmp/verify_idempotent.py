import socket
import sys

IP = "127.0.0.1"
PORT = 9000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2)

def send(cmd):
    print(f"TX: {cmd}")
    sock.sendto(cmd.encode(), (IP, PORT))
    try:
        data, _ = sock.recvfrom(1024)
        print(f"RX: {data.decode()}")
        return data.decode()
    except socket.timeout:
        print("RX: Timeout")
        return None

if __name__ == "__main__":
    if len(sys.argv) > 1: PORT = int(sys.argv[1])
    
    # 1. Add Group
    if send("addgroup grp_idempotent") != "OK":
        print("FAILED: First addgroup failed")
        sys.exit(1)
        
    # 2. Add Same Group (Should be OK)
    resp = send("addgroup grp_idempotent")
    if resp != "OK":
        print(f"FAILED: Second addgroup failed. Expected OK, got {resp}")
        sys.exit(1)
        
    print("Idempotent AddGroup verified")
    sys.exit(0)
