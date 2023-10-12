#!/usr/bin/env python

import zmq


class DeviceEmulator:
    def __init__(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PAIR)
        self.socket.bind("ipc:///tmp/device_emulator.ipc")

    def run(self):
        while True:
            message = self.socket.recv()
            print(f"Received request: {message}")
            self.socket.send_string("World")

def main():
    emulator = DeviceEmulator()
    emulator.run() 

if __name__ == "__main__":
    main()