"""Common types and exceptions for the host emulator.

The string values of every enum here are wire vocabulary: they mirror the C++
serialization tables in ``src/libs/mcu/host/emulator_message_json_encoder.hpp``
and the two sides must change together.
"""

from enum import StrEnum


class UnhandledMessageError(Exception):
    """Exception raised when a message cannot be handled."""


class MessageType(StrEnum):
    """Whether a message initiates an exchange or answers one."""

    Request = "Request"
    Response = "Response"


class ObjectType(StrEnum):
    """Which kind of peripheral a message addresses."""

    Pin = "Pin"
    Uart = "Uart"
    I2C = "I2C"


class Operation(StrEnum):
    """What a request asks the receiving side to do."""

    Set = "Set"
    Get = "Get"
    Send = "Send"
    Receive = "Receive"


class Status(StrEnum):
    """Status codes for emulator responses; mirrors C++ ``common::Error``."""

    Ok = "Ok"
    Unknown = "Unknown"
    InvalidArgument = "InvalidArgument"
    InvalidState = "InvalidState"
    InvalidOperation = "InvalidOperation"
    OperationFailed = "OperationFailed"
    Unhandled = "Unhandled"
    ConnectionRefused = "ConnectionRefused"
    ConnectionClosed = "ConnectionClosed"
    Timeout = "Timeout"
    WouldBlock = "WouldBlock"
    MessageTooLarge = "MessageTooLarge"
