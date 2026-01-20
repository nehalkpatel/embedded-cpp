"""Common types and exceptions for the host emulator."""

from enum import Enum


class UnhandledMessageError(Exception):
    """Exception raised when a message cannot be handled."""


class Status(Enum):
    """Status codes for emulator responses."""

    Ok = "Ok"
    Unknown = "Unknown"
    InvalidArgument = "InvalidArgument"
    InvalidState = "InvalidState"
    InvalidOperation = "InvalidOperation"
