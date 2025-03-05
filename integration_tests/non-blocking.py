#!/usr/bin/env python3
import socket
import sys
import time
import random
import threading
from typing import List

# ANSI color codes
GREEN = '\033[92m'
RED = '\033[91m'
RESET = '\033[0m'
BOLD = '\033[1m'

class IRCClient:
    def __init__(self, host: str, port: int, nickname=None):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.host = host
        self.port = port
        self.buffer = b""
        self.nickname = nickname or f"user{random.randint(1000, 9999)}"
        self.sock.settimeout(0.5)  # Longer timeout for normal operations
    
    def register(self):
        self.send("PASS 2508")
        self.send(f"NICK {self.nickname}")
        self.send(f"USER {self.nickname} 0 * :Test User")
        
        # Wait for registration confirmation
        registered = False
        start_time = time.time()
        while time.time() - start_time < 3:  # Wait up to 3 seconds
            response = self.receive()
            if response and ("001" in response):  # 001 is RPL_WELCOME
                registered = True
                break
            time.sleep(0.1)
        
        if not registered:
            print(f"Failed to register client {self.nickname}")
            return False
        return True
        
    def join_channel(self, channel):
        self.send(f"JOIN {channel}")
        time.sleep(0.2)  # Wait for join
    
    def send(self, message):
        self.sock.send((message + "\r\n").encode())
    
    def receive(self):
        try:
            data = self.sock.recv(4096)
            if data:
                return data.decode('utf-8', errors='replace')
        except socket.timeout:
            return None
        except Exception as e:
            print(f"Error receiving data: {e}")
            return None
            
    def close(self):
        try:
            self.sock.close()
        except:
            pass

def print_result(passed: bool, message: str):
    status = f"\n{GREEN}PASS{RESET}" if passed else f"\n{RED}FAIL{RESET}"
    print(f"{BOLD}{status}{RESET} {message}")

def test_message_flood(host: str, port: int):
    print("\nTesting message flood handling")
    
    # Create sender and receiver
    sender = IRCClient(host, port, "flood_sender")
    receiver = IRCClient(host, port, "flood_receiver")
    
    # Ensure both clients are registered
    if not sender.register() or not receiver.register():
        print("Failed to register clients, aborting test")
        return False
    
    channel = "#testchannel"
    sender.join_channel(channel)
    receiver.join_channel(channel)
    
    # Flag to terminate the flooding thread
    stop_flooding = False
    
    # Function to send flood messages in a separate thread
    def flood_sender_thread():
        message_count = 0
        while not stop_flooding:
            flood_msg = f"PRIVMSG {channel} :Flood message {message_count} with random data {'X' * random.randint(10, 50)}"
            sender.send(flood_msg)
            message_count += 1
            # Small sleep to prevent network saturation
            time.sleep(0.001)
        print(f"Sent {message_count} messages during flood test")
    
    # Start the flooding in a background thread
    flood_thread = threading.Thread(target=flood_sender_thread)
    flood_thread.start()
    
    # Now test server responsiveness DURING the flood
    print("Testing responsiveness during message flood...")
    
    # Wait briefly to ensure flood has started
    time.sleep(0.5)
    
    # Try to communicate during the flood
    response_received = False
    for i in range(5):
        # Send a test message from receiver
        test_msg = f"PRIVMSG {sender.nickname} :Test message during flood {i}"
        receiver.send(test_msg)
        
        # Check for a response
        start_time = time.time()
        while time.time() - start_time < 2.0:
            response = receiver.receive()
            if response and "PRIVMSG" in response:
                response_received = True
                break
            time.sleep(0.1)
        
        if response_received:
            break
        time.sleep(0.5)
    
    # Stop the flooding
    stop_flooding = True
    flood_thread.join(timeout=2.0)
    
    # Clean up
    sender.close()
    receiver.close()
    
    if response_received:
        print("Server remained responsive DURING message flood")
        return True
    else:
        print("Server unresponsive during message flood")
        return False

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <port>")
        sys.exit(1)
    
    try:
        port = int(sys.argv[1])
        if port < 1 or port > 65535:
            raise ValueError
    except ValueError:
        print("Error: Port must be a number between 1 and 65535")
        sys.exit(1)
    
    host = '127.0.0.1'
    
    try:
        # Only run the message flood test
        result = test_message_flood(host, port)
        
        # Overall results
        print("\n=== Test Summary ===")
        print_result(result, "Message flood test")
        
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nTest error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()