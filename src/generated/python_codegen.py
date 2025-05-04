# AUTO-GENERATED FILE. DO NOT EDIT.

from typing import Any

from enum import Enum

class Command(Enum):
    GET_VERSION = "getVersion"
    SET_TIME = "setTime"
    CLEAR_BUFFER = "clearBuffer"
    READ_BUFFER = "readBuffer"
    START_LOGGING = "startLogging"
    STOP_LOGGING = "stopLogging"
    GET_NOW = "getNow"
    GET_STATUS = "getStatus"
    SET_SAMPLING_RATE = "setSamplingRate"
    CALIBRATE = "calibrate"
    RESET = "reset"
    SET_LOG_LEVEL = "setLogLevel"
    DROP_RECORDS = "dropRecords"
    UNKNOWN = "unknown"

class BaseCommandHandler:
    def getVersion(self):
        """Get firmware version (string)"""
        pass

    def setTime(self, epoch):
        """Set device time to given epoch

Args:
    epoch (int): Unix epoch seconds
"""
        pass

    def clearBuffer(self):
        """Clear the data buffer"""
        pass

    def readBuffer(self, offset, length):
        """Read paginated buffer

Args:
    offset (int): Start index
    length (int): Number of records
"""
        pass

    def startLogging(self):
        """Enable logging"""
        pass

    def stopLogging(self):
        """Disable logging"""
        pass

    def getNow(self):
        """Get current device time"""
        pass

    def getStatus(self):
        """Get device status"""
        pass

    def setSamplingRate(self, rate):
        """Set sampling rate

Args:
    rate (int): Sampling rate in Hz
"""
        pass

    def calibrate(self, low, high, weight):
        """Calibrate the scale

Args:
    low (int): No-load reading
    high (int): Loaded reading
    weight (int): Actual weight in grams
"""
        pass

    def reset(self):
        """Reset the device"""
        pass

    def setLogLevel(self, printer, level):
        """Set log level for a printer

Args:
    printer (str): Printer name (raw/event/status)
    level (int): Log level
"""
        pass

    def dropRecords(self, offset, length):
        """Drop records from buffer

Args:
    offset (int): Start index
    length (int): Number of records
"""
        pass

    def unknown(self):
        """Unknown command (error)"""
        pass


COMMAND_SPECS = {
    "getVersion": {
        "args": [
        ],
        "doc": "Get firmware version (string)"
    },
    "setTime": {
        "args": [
            {"name": "epoch", "type": int, "doc": "Unix epoch seconds"},
        ],
        "doc": "Set device time to given epoch"
    },
    "clearBuffer": {
        "args": [
        ],
        "doc": "Clear the data buffer"
    },
    "readBuffer": {
        "args": [
            {"name": "offset", "type": int, "doc": "Start index"},
            {"name": "length", "type": int, "doc": "Number of records"},
        ],
        "doc": "Read paginated buffer"
    },
    "startLogging": {
        "args": [
        ],
        "doc": "Enable logging"
    },
    "stopLogging": {
        "args": [
        ],
        "doc": "Disable logging"
    },
    "getNow": {
        "args": [
        ],
        "doc": "Get current device time"
    },
    "getStatus": {
        "args": [
        ],
        "doc": "Get device status"
    },
    "setSamplingRate": {
        "args": [
            {"name": "rate", "type": int, "doc": "Sampling rate in Hz"},
        ],
        "doc": "Set sampling rate"
    },
    "calibrate": {
        "args": [
            {"name": "low", "type": int, "doc": "No-load reading"},
            {"name": "high", "type": int, "doc": "Loaded reading"},
            {"name": "weight", "type": int, "doc": "Actual weight in grams"},
        ],
        "doc": "Calibrate the scale"
    },
    "reset": {
        "args": [
        ],
        "doc": "Reset the device"
    },
    "setLogLevel": {
        "args": [
            {"name": "printer", "type": str, "doc": "Printer name (raw/event/status)"},
            {"name": "level", "type": int, "doc": "Log level"},
        ],
        "doc": "Set log level for a printer"
    },
    "dropRecords": {
        "args": [
            {"name": "offset", "type": int, "doc": "Start index"},
            {"name": "length", "type": int, "doc": "Number of records"},
        ],
        "doc": "Drop records from buffer"
    },
    "unknown": {
        "args": [
        ],
        "doc": "Unknown command (error)"
    },
}
