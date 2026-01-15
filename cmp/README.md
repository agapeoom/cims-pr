# CMP Server

`cmp` (Component Media Provider) is a standalone media processing server that manages RTP sessions via a UDP control protocol.

## Build Instructions

To build the `CmpServer` executable:

1. Go to the build directory (create it if it doesn't exist):
   ```bash
   cd /home/nex/work/cims/build
   ```

2. Run CMake:
   ```bash
   cmake ..
   ```

3. Build the server:
   ```bash
   make cmp
   ```

The binary will be located at `/home/nex/work/cims/bin/cmp`.

## Usage

Start the server by specifying the UDP listening port:

```bash
./bin/cmp
```

Example:
```bash
./bin/cmp 
```

## Control Protocol

The server listens for UDP packets containing text-based commands. Parameters are space-separated.

### 1. ALIVE Check
Check if the server is running.
- **Command:** `ALIVE`
- **Response:** `OK`

### 2. ADD Session
Create a new media resource (RTP session). Optionally supports Video ports.
- **Command:** `ADD <id> <local_ip> <local_audio_port> <remote_ip> <remote_audio_port> [local_video_port remote_video_port]`
- **Response:** `OK` or `ERROR: ...`
- **Example Audio Only:** `ADD session1 0.0.0.0 10000 192.168.1.50 20000`
- **Example Audio+Video:** `ADD session1 0.0.0.0 10000 192.168.1.50 20000 10002 20002`

### 3. MODIFY Session
Update the remote address of an existing session.
- **Command:** `MODIFY <id> <remote_ip> <remote_port>`
- **Response:** `OK` or `ERROR: ...`
- **Example:** `MODIFY session1 192.168.1.51 20002`

### 4. REMOVE Session
Delete a session and release resources.
- **Command:** `REMOVE <id>`
- **Response:** `OK` or `ERROR: ...`
- **Example:** `REMOVE session1`

### 5. ADD_GROUP (MCPTT)
Create a new MCPTT group.
- **Command:** `addGroup <groupId>`
- **Response:** `OK` or `ERROR`
- **Example:** `addGroup group1`

### 6. REMOVE_GROUP (MCPTT)
Delete an MCPTT group.
- **Command:** `removeGroup <groupId>`
- **Response:** `OK` or `ERROR`

### 7. JOIN_GROUP (MCPTT)
Add a session to a group.
- **Command:** `joinGroup <groupId> <sessionId>`
- **Response:** `OK` or `ERROR`

### 8. LEAVE_GROUP (MCPTT)
Remove a session from a group.
- **Command:** `leaveGroup <groupId> <sessionId>`
- **Response:** `OK` or `ERROR`

## Verification

Two Python scripts are provided to verify the server functionality.

### 1. General Call Test (Session Management)
Verifies UDP commands for session creation and modification (`ALIVE`, `ADD`, `MODIFY`, `REMOVE`).
Also verifies **Audio (AMR-WB)** and **Video (H.265)** packet handling (Echo).

```bash
python3 cmp/verify_call.py <port>
```
Example:
```bash
python3 cmp/verify_call.py 9000
```

### 2. PTT Scenario Test (Group & Floor Control)
Verifies MCPTT logic, including Group creation/joining and Floor Control state machine (Request, Grant, Release, Idle).
- Simulates 2 Groups with 3 Participants each.
- Runs 2 Rounds of Floor Control usage for each participant.

```bash
python3 cmp/verify_ptt.py <port> [--pcap <filename.pcap>]
```
Example:
```bash
python3 cmp/verify_ptt.py 9000 --pcap test.pcap
python3 cmp/verify_ptt.py 9000 --pcap test2.pcap --no-cleanup

```
> **Note:** PCAP capture requires `tcpdump` installed and sufficient privileges (e.g., `sudo` or `CAP_NET_RAW`).
