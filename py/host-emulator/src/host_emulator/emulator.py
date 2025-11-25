#!/usr/bin/env python

import json
import sys
from threading import Thread

import zmq

from .common import UnhandledMessageError
from .i2c import I2C
from .pin import Pin
from .uart import Uart


class DeviceEmulator:
    def __init__(self):
        print("Creating DeviceEmulator")
        self.emulator_thread = Thread(target=self.run)
        self.running = False
        self.to_device_context = zmq.Context()
        self.to_device_socket = self.to_device_context.socket(zmq.PAIR)
        self.to_device_socket.connect("ipc:///tmp/emulator_device.ipc")
        self.led_1 = Pin(
            "LED 1", Pin.direction.OUT, Pin.state.Low, self.to_device_socket
        )
        self.led_2 = Pin(
            "LED 2", Pin.direction.OUT, Pin.state.Low, self.to_device_socket
        )
        self.button_1 = Pin(
            "Button 1", Pin.direction.IN, Pin.state.Low, self.to_device_socket
        )
        self.pins = [self.led_1, self.led_2, self.button_1]

        self.uart_1 = Uart("UART 1", self.to_device_socket)
        self.uarts = [self.uart_1]

        self.i2c_1 = I2C("I2C 1")
        self.i2cs = [self.i2c_1]

    def user_led1(self):
        return self.led_1

    def user_led2(self):
        return self.led_2

    def user_button1(self):
        return self.button_1

    def uart1(self):
        return self.uart_1

    def i2c1(self):
        return self.i2c_1

    def run(self):
        print("Starting emulator thread")
        try:
            from_device_context = zmq.Context()
            from_device_socket = from_device_context.socket(zmq.PAIR)
            from_device_socket.bind("ipc:///tmp/device_emulator.ipc")
            self.running = True
            while self.running:
                print("Waiting for message...")
                message = from_device_socket.recv()
                # print(f"[Emulator] Received request: {message}")
                if message.startswith(b"{") and message.endswith(b"}"):
                    # JSON message
                    json_message = json.loads(message)
                    if json_message["object"] == "Pin":
                        for pin in self.pins:
                            if response := pin.handle_message(json_message):
                                # print(f"[Emulator] Sending response: {response}")
                                from_device_socket.send_string(response)
                                # print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - Pin not found")
                    elif json_message["object"] == "Uart":
                        for uart in self.uarts:
                            if response := uart.handle_message(json_message):
                                # print(f"[Emulator] Sending response: {response}")
                                from_device_socket.send_string(response)
                                # print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - Uart not found")
                    elif json_message["object"] == "I2C":
                        for i2c in self.i2cs:
                            if response := i2c.handle_message(json_message):
                                # print(f"[Emulator] Sending response: {response}")
                                from_device_socket.send_string(response)
                                # print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - I2C not found")
                    else:
                        raise UnhandledMessageError(
                            message, f" - unknown object type: {json_message['object']}"
                        )
                else:
                    raise UnhandledMessageError(message, " - not JSON")
        finally:
            from_device_socket.close()
            from_device_context.term()

    def start(self):
        self.emulator_thread.start()

    def stop(self):
        self.running = False
        self.emulator_thread.join()

    def uart_initialized(self, name):
        """Check if a UART with the given name exists."""
        return any(uart.name == name for uart in self.uarts)

    def get_uart_tx_data(self, name):
        """Get data that was transmitted (sent) from the device to the emulator."""
        for uart in self.uarts:
            if uart.name == name:
                if len(uart.rx_buffer) > 0:
                    data = list(uart.rx_buffer)
                    return data
                return None
        return None

    def clear_uart_tx_data(self, name):
        """Clear the TX buffer (data received from device)."""
        for uart in self.uarts:
            if uart.name == name:
                uart.rx_buffer.clear()
                return True
        return False

    def uart_send_to_device(self, name, data):
        """Send data from emulator to device (simulating external UART input)."""
        for uart in self.uarts:
            if uart.name == name:
                return uart.send_data(data)
        return None

    def get_pin_state(self, name):
        """Get the current state of a pin."""
        for pin in self.pins:
            if pin.name == name:
                return pin.state
        return None


def main():
    emulator = DeviceEmulator()
    try:
        emulator.start()
        print("Sending Hello")
        emulator.to_device_socket.send_string("Hello")
        print("Waiting for reply")
        reply = emulator.to_device_socket.recv()
        print(f"Received reply: {reply}")
        reply = emulator.user_button1().get_state()
        print(f"Received reply: {reply}")
        while emulator.running:
            emulator.emulator_thread.join(0.5)
    except (KeyboardInterrupt, SystemExit):
        print("main Received keyboard interrupt")
        emulator.stop()
        emulator.to_device_socket.close()
        emulator.to_device_context.term()
        sys.exit(0)


if __name__ == "__main__":
    main()
