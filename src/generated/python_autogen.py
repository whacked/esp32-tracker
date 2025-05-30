# AUTO-GENERATED FILE. DO NOT EDIT.

from typing import Any, List, TypedDict

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

class SetTimeResponse(TypedDict):
    status: str
    offset: int
    time: str


class ReadBufferResponse(TypedDict):
    records: List[Any]
    length: int


class GetNowResponse(TypedDict):
    epoch: int
    local: str


class GetStatusResponse(TypedDict):
    logging: bool
    bufferSize: int
    rateHz: int


class SetLogLevelResponse(TypedDict):
    status: str
    printer: str
    level: int


class DropRecordsResponse(TypedDict):
    status: str
    offset: int
    length: int

class BaseCommandHandler:
    async def getVersion(self) -> str:
        """Get firmware version (string)

Returns:
    str"""
        pass

    async def setTime(self, epoch) -> SetTimeResponse:
        """Set device time to given epoch

Args:
    epoch (int): Unix epoch seconds


Returns:
    SetTimeResponse"""
        pass

    async def clearBuffer(self) -> None:
        """Clear the data buffer"""
        pass

    async def readBuffer(self, offset, length) -> ReadBufferResponse:
        """Read paginated buffer

Args:
    offset (int): Start index
    length (int): Number of records


Returns:
    ReadBufferResponse"""
        pass

    async def startLogging(self) -> None:
        """Enable logging"""
        pass

    async def stopLogging(self) -> None:
        """Disable logging"""
        pass

    async def getNow(self) -> GetNowResponse:
        """Get current device time

Returns:
    GetNowResponse"""
        pass

    async def getStatus(self) -> GetStatusResponse:
        """Get device status

Returns:
    GetStatusResponse"""
        pass

    async def setSamplingRate(self, rate) -> None:
        """Set sampling rate

Args:
    rate (int): Sampling rate in Hz
"""
        pass

    async def calibrate(self, low, high, weight) -> None:
        """Calibrate the scale

Args:
    low (int): No-load reading
    high (int): Loaded reading
    weight (int): Actual weight in grams
"""
        pass

    async def reset(self) -> None:
        """Reset the device"""
        pass

    async def setLogLevel(self, printer, level) -> SetLogLevelResponse:
        """Set log level for a printer

Args:
    printer (str): Printer name (raw/event/status)
    level (int): Log level


Returns:
    SetLogLevelResponse"""
        pass

    async def dropRecords(self, offset, length) -> DropRecordsResponse:
        """Drop records from buffer

Args:
    offset (int): Start index
    length (int): Number of records


Returns:
    DropRecordsResponse"""
        pass

    async def unknown(self) -> str:
        """Unknown command (error)

Returns:
    str"""
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
