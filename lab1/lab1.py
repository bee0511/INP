import subprocess
import time
import socket
import dpkt
import os
import argparse


# Server address and port
# server_host = "inp.zoolab.org"
server_port = 10495

# Your user ID
# user_id = "110550164"  

def connect_to_server_and_capture(user_id, times, server_host):
    try:
        # Start capturing UDP packets using tcpdump with a filter
        pcap_file = f"{user_id}_challenge.pcap"
        tcpdump_command = [
            "tcpdump", "-ni", "any", "-w", pcap_file, "-Xxnv", "udp", "and", "port", str(server_port)
        ]
        tcpdump_process = subprocess.Popen(tcpdump_command)

        # Sleep for a moment to allow tcpdump to start capturing
        time.sleep(1)

        # Create a UDP socket to connect to the server
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        client_socket.connect((server_host, server_port))

        # Request a challenge ID from the server
        challenge_request = f"hello {user_id}"
        client_socket.send(challenge_request.encode())
        challenge_id_response = client_socket.recv(1024).decode().strip()
        print(challenge_id_response)

        # Request the challenge using the obtained challenge ID
        challenge_request = f"chals {challenge_id_response.split()[-1]}"
        
        for cnt in range(times):
            print("Sending", cnt+1, "th request")
            client_socket.send(challenge_request.encode())
            time.sleep(2)

        # Close the UDP socket and tcpdump process
        # client_socket.close()
        tcpdump_process.terminate()
        tcpdump_process.wait()

        print(f"Challenge packets saved to {pcap_file}")

        return pcap_file, client_socket

    except Exception as e:
        print(f"Error: {e}")

def extract_flag_from_pcap(pcap_file):
    try:
        # Open the pcap file for reading
        with open(pcap_file, 'rb') as f:
            pcap = dpkt.pcap.Reader(f)
            flag = ""
            dictionary = {}
            begin_flag = -1
            end_flag = -1
            for _, buf in pcap:
                # Decode the packet payload
                ip = dpkt.sll2.SLL2(buf).data
                payload = ip.data.data
                if "BEGIN FLAG" in payload.decode():
                    begin_flag = int(payload[4:9].decode())
                    continue
                if "END FLAG" in payload.decode():
                    end_flag = int(payload[4:9].decode())
                    continue
                if (payload[0:3].decode() == 'SEQ'):
                    dictionary[int(payload[4:9].decode())] = chr(4 * (ip.hl) - 20 + len(payload))
                # print(ip.hl, payload)
                    # Calculate the x-header-length by summing the lengths of IP options and UDP payload
            if end_flag == -1:
                print("no end flag")
            if begin_flag == -1:
                print("no begin flag")
            for seq_number in range(begin_flag + 1, end_flag):
                flag += dictionary[seq_number]

            print("Decoded Flag:", flag)
        return flag

    except Exception as e:
        print(f"Error extracting flag from pcap: {e}")

def send_flag(flag, client_socket):
    if flag is None:
        raise NotImplementedError
    if flag[0:5] != "FLAG{" or flag[-1] != "}":
        print("Not a FLAG")
        return
    
    # client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # client_socket.connect((server_host, server_port))
    
    start_index = flag.find("{") + 1
    end_index = flag.find("}")
    # flag = flag[start_index:end_index]
    pcap_file = "verfy.pcap"
    tcpdump_command = [
        "tcpdump", "-ni", "any", "-w", pcap_file, "-Xxnv", "udp", "and", "port", str(server_port)
    ]
    tcpdump_process = subprocess.Popen(tcpdump_command)
    time.sleep(1)
    print("Sending flag:", flag)
    verfy_request = f"verfy {flag}"
    
    client_socket.send(verfy_request.encode())
    time.sleep(1)
    
    tcpdump_process.terminate()
    tcpdump_process.wait()
    # server_response = client_socket.recv(1024).decode().strip()
    
    with open(pcap_file, 'rb') as f:
        pcap = dpkt.pcap.Reader(f)
        for _, buf in pcap:
            payload = dpkt.sll2.SLL2(buf).data.data.data
            if "GOOD JOB!" in payload.decode():
                print("GOOD JOB!")
            if "NO NO NO..." in payload.decode():
                print("NO NO NO...")
            
    # print("Server Response:", server_response)
    
    client_socket.close()
    

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Send packets from a pcap file.")
    parser.add_argument("--t", type=int, default=1, help="Number of times to send the packets.")
    parser.add_argument("--s", type=str, default="inp.zoolab.org", help="Destination server")
    parser.add_argument("--id", type=str, default="110550164", help="ID")
    args = parser.parse_args()
    pcap_file, client_socket = connect_to_server_and_capture(args.id, args.t, args.s)
    # pcap_file = "test.pcap"
    flag = extract_flag_from_pcap(pcap_file)
    send_flag(flag, client_socket)
    

