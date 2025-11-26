#!/usr/bin/env python

import json
import logging
import sys
import time
from threading import Thread

import zmq

from .common import UnhandledMessageError
from .i2c import I2C
from .pin import Pin
from .uart import Uart

# Configure logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)  # Default to INFO, can be changed by users

# Add console handler if not already configured
if not logger.handlers:
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.DEBUG)
    formatter = logging.Formatter("[%(levelname)s] %(name)s: %(message)s")
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)


class DeviceEmulator:
    # Default endpoints for IPC communication
    DEFAULT_FROM_DEVICE_ENDPOINT = "ipc:///tmp/device_emulator.ipc"
    DEFAULT_TO_DEVICE_ENDPOINT = "ipc:///tmp/emulator_device.ipc"

    def __init__(
        self,
        from_device_endpoint=None,
        to_device_endpoint=None,
    ):
        """Initialize the device emulator.

        Args:
            from_device_endpoint: ZMQ endpoint to bind for receiving from device.
                                 Default: "ipc:///tmp/device_emulator.ipc"
            to_device_endpoint: ZMQ endpoint to connect for sending to device.
                               Default: "ipc:///tmp/emulator_device.ipc"
        """
        self.from_device_endpoint = (
            from_device_endpoint or self.DEFAULT_FROM_DEVICE_ENDPOINT
        )
        self.to_device_endpoint = to_device_endpoint or self.DEFAULT_TO_DEVICE_ENDPOINT

        logger.info("Creating DeviceEmulator")
        logger.debug(f"  from_device: {self.from_device_endpoint}")
        logger.debug(f"  to_device: {self.to_device_endpoint}")

        self.running = False

        # Create single context for the entire emulator
        self.context = zmq.Context()

        # Create sockets but DON'T connect/bind yet
        self.to_device_socket = self.context.socket(zmq.PAIR)
        self.from_device_socket = self.context.socket(zmq.PAIR)

        # Set socket options for robust operation
        self.to_device_socket.setsockopt(zmq.LINGER, 0)  # Discard on close
        self.to_device_socket.setsockopt(zmq.SNDTIMEO, 1000)  # 1s send timeout
        self.from_device_socket.setsockopt(zmq.LINGER, 0)
        self.from_device_socket.setsockopt(zmq.RCVTIMEO, 500)  # 500ms recv timeout

        # Hardware components (use socket but don't send yet)
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

        self.emulator_thread = Thread(target=self.run)
        self._ready = False

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
        """Main emulator thread - BIND first, then signal ready."""
        logger.debug("Starting emulator thread")
        try:
            # Clean up any stale socket files from previous runs (IPC only)
            if self.from_device_endpoint.startswith("ipc://"):
                import os

                socket_path = self.from_device_endpoint.replace("ipc://", "")
                try:
                    os.unlink(socket_path)
                    logger.debug(f"Removed stale socket file: {socket_path}")
                except FileNotFoundError:
                    pass  # No stale file, that's fine

            # BIND in the thread (before anyone tries to connect)
            self.from_device_socket.bind(self.from_device_endpoint)
            logger.debug(f"Bound to {self.from_device_endpoint}")

            self.running = True
            self._ready = True  # Signal that we're ready

            while self.running:
                try:
                    # Use recv with timeout (from socket options)
                    message = self.from_device_socket.recv()

                    if message.startswith(b"{") and message.endswith(b"}"):
                        json_message = json.loads(message)
                    if json_message["object"] == "Pin":
                        for pin in self.pins:
                            if response := pin.handle_message(json_message):
                                # print(f"[Emulator] Sending response: {response}")
                                self.from_device_socket.send_string(response)
                                # print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - Pin not found")
                    elif json_message["object"] == "Uart":
                        for uart in self.uarts:
                            if response := uart.handle_message(json_message):
                                # print(f"[Emulator] Sending response: {response}")
                                self.from_device_socket.send_string(response)
                                # print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - Uart not found")
                    elif json_message["object"] == "I2C":
                        for i2c in self.i2cs:
                            if response := i2c.handle_message(json_message):
                                # print(f"[Emulator] Sending response: {response}")
                                self.from_device_socket.send_string(response)
                                # print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - I2C not found")
                    else:
                        raise UnhandledMessageError(
                            message, f" - unknown object type: {json_message['object']}"
                        )
                except zmq.Again:
                    # Timeout - check if we should stop
                    if not self.running:
                        break
                    continue

        except Exception as e:
            logger.error(f"Emulator thread error: {e}", exc_info=True)
        finally:
            self.from_device_socket.close()
            logger.debug("Emulator thread exiting")

    def start(self):
        """Start emulator and wait until ready."""
        self.emulator_thread.start()

        # Wait for emulator to be ready (with timeout)
        timeout = 5.0  # seconds
        start_time = time.time()
        while not self._ready:
            if time.time() - start_time > timeout:
                raise RuntimeError("Emulator failed to start within timeout")
            time.sleep(0.01)

        # NOW connect the to_device socket (emulator is bound and ready)
        self.to_device_socket.connect(self.to_device_endpoint)
        logger.debug(f"Connected to {self.to_device_endpoint}")

        # Give connection a moment to establish
        time.sleep(0.05)

    def stop(self):
        """Stop emulator and clean up resources."""
        logger.info("Stopping emulator")
        self.running = False

        # Wait for thread to exit (recv timeout will let it check running flag)
        self.emulator_thread.join(timeout=2.0)

        # Clean up sockets and context
        self.to_device_socket.close()
        # from_device_socket closed in thread
        self.context.term()
        logger.info("Emulator stopped")

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
        logger.info("Sending Hello")
        emulator.to_device_socket.send_string("Hello")
        logger.info("Waiting for reply")
        reply = emulator.to_device_socket.recv()
        logger.info(f"Received reply: {reply}")
        reply = emulator.user_button1().get_state()
        logger.info(f"Received reply: {reply}")
        while emulator.running:
            emulator.emulator_thread.join(0.5)
    except (KeyboardInterrupt, SystemExit):
        logger.info("main Received keyboard interrupt")
        emulator.stop()
        sys.exit(0)


if __name__ == "__main__":
    main()
