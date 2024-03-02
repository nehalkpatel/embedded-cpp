from threading import Thread
from host_emulator.emulator import DeviceEmulator
from time import sleep
import pathlib
import subprocess


def test_blinky_executable():
    # Create an instance of the BlinkyEmulator class
    emulator = DeviceEmulator()

    # Start the blinky executable
    print(f"{pathlib.Path.cwd()=}")
    blinky_executable = pathlib.Path("blinky").resolve()
    print(f"{blinky_executable=}")

    if not blinky_executable.exists():
        raise FileNotFoundError("The blinky executable does not exist")

    # Start the emulator in a separate thread
    emulator.emulator_thread.start()

    blinky = subprocess.Popen(
        [str(blinky_executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    sleep(1)
    emulator.stop()

    # Perform your test assertions here

    # Stop the emulator
    # emulator.emulator_thread.stop()

    # Wait for the emulator thread to finish
    emulator.emulator_thread.join()
    blinky.terminate()
