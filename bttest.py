import asyncio
import json
import time
from asyncio import Queue as AsyncQueue
from pprint import pprint
from textwrap import indent
from typing import Any, List, Optional, cast

import yaml
from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice
from prompt_toolkit import PromptSession
from prompt_toolkit.completion import WordCompleter
from prompt_toolkit.formatted_text import HTML
from prompt_toolkit.history import FileHistory
from prompt_toolkit.styles import Style

from src.generated.python_autogen import (
    COMMAND_SPECS,
    BaseCommandHandler,
    Command,
    DropRecordsResponse,
    GetStatusResponse,
    ReadBufferResponse,
    SetTimeResponse,
)

SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

TARGET_NAME = "ESP32-Scale"

response_queue: AsyncQueue[str] = AsyncQueue()

# Define available commands and their descriptions
COMMANDS = {
    "help": "Show this help message",
    "exit": "Exit the program",
    **{cmd: spec["doc"] for cmd, spec in COMMAND_SPECS.items()},
}

# Create a style for syntax highlighting
style = Style.from_dict(
    {
        "command": "#ansigreen",
        "error": "#ansired",
        "response": "#ansiblue",
    }
)


async def acquire_device() -> Optional[BLEDevice]:
    print("Scanning for ESP32...")
    devices = await BleakScanner.discover()
    esp_device = next((d for d in devices if d.name and TARGET_NAME in d.name), None)
    if not esp_device:
        return None
    print(f"Found {esp_device.name} [{esp_device.address}]")
    return esp_device


async def wait_for_response():
    try:
        return await asyncio.wait_for(response_queue.get(), timeout=5.0)
    except asyncio.TimeoutError:
        raise Exception("Timeout waiting for response")


class MyCommandHandler(BaseCommandHandler):
    """Subclass for client-specific command implementations."""

    def __init__(self, client: BleakClient) -> None:
        super().__init__()
        self.client = client

    async def _get_command_response(self) -> dict | None:
        response = await wait_for_response()
        return json.loads(response)

    async def sendCommandString(self, command: str) -> Any:
        await self.client.write_gatt_char(RX_UUID, (command + "\n").encode())
        return await self._get_command_response()

    async def setTime(self, epoch: int) -> SetTimeResponse:
        out = await self.sendCommandString(f"{Command.SET_TIME.value} {int(epoch)}")
        return cast(SetTimeResponse, out)

    async def getStatus(self) -> GetStatusResponse:
        out = await self.sendCommandString(Command.GET_STATUS.value)
        return cast(GetStatusResponse, out)

    async def readBuffer(self, offset: int, length: int) -> ReadBufferResponse:
        out = await self.sendCommandString(
            f"{Command.READ_BUFFER.value} {offset} {length}"
        )
        return cast(ReadBufferResponse, out)

    async def dropRecords(self, offset: int, length: int) -> DropRecordsResponse:
        out = await self.sendCommandString(
            f"{Command.DROP_RECORDS.value} {offset} {length}"
        )
        return cast(DropRecordsResponse, out)


def handle_rx(_, data: bytes):
    payload_string = data.decode(errors="ignore")
    try:
        mixed_payload = json.loads(payload_string)
        yaml_string = yaml.safe_dump(mixed_payload, default_flow_style=False)
        print(f"\n< {len(payload_string)}")
        print("< " + indent(yaml_string, "+ ").lstrip("+ "))
        response_queue.put_nowait(payload_string)
    except json.JSONDecodeError:
        print(f"< {payload_string}")
        response_queue.put_nowait("null")


async def main():
    esp_device = await acquire_device()
    if not esp_device:
        print("ESP32 device not found")
        return

    async with BleakClient(esp_device.address) as client:
        print(f"Connected to {esp_device.name} [{esp_device.address}]")
        handler = MyCommandHandler(client)
        await client.start_notify(TX_UUID, handle_rx)
        await handler.setTime(int(time.time()))

        # Create a prompt session with history and autocompletion
        session = PromptSession(
            history=FileHistory(".bttest_history"),
            completer=WordCompleter(list(COMMANDS.keys())),
            style=style,
        )

        def get_formatted_prompt() -> HTML:
            return HTML("<command>> </command>")

        def format_response(text: str) -> HTML:
            return HTML(f"<response>{text}</response>")

        def format_error(text: str) -> HTML:
            return HTML(f"<error>Error: {text}</error>")

        def show_help():
            print("\nAvailable commands:")
            for cmd, desc in COMMANDS.items():
                print(f"  {cmd:<10} - {desc}")

        while True:
            try:
                # Get command with autocompletion
                cmd = await session.prompt_async(get_formatted_prompt())
                cmd = cmd.strip()

                if not cmd:
                    continue

                if cmd.lower() == "exit":
                    try:
                        await client.stop_notify(TX_UUID)
                        await client.disconnect()
                    except Exception as e:
                        print(f"Note: Device already disconnected or unreachable: {e}")
                    break

                if cmd.lower() == "help":
                    show_help()
                    continue

                response = await handler.sendCommandString(cmd)
                print(response)

            except KeyboardInterrupt:
                continue
            except Exception as e:
                print(format_error(str(e)))


async def demo_data_fetch():
    esp_device = await acquire_device()
    async with BleakClient(esp_device.address) as client:
        print(f"Connected to {esp_device.name} [{esp_device.address}]")
        handler = MyCommandHandler(client)
        await client.start_notify(TX_UUID, handle_rx)
        await handler.setTime(int(time.time()))

        status = await handler.getStatus()
        total_records = status["bufferSize"]

        all_records = []
        chunk_size = 5
        records_read = 0

        # First phase: Read all records
        while records_read < total_records:
            chunk_data = await handler.readBuffer(records_read, chunk_size)

            # Store the records
            all_records.extend(chunk_data["records"])
            records_read += chunk_data["length"]

            # If we got fewer records than requested, we're done
            if chunk_data["length"] < chunk_size:
                break

        # Second phase: Drop the records we read (in chunks)
        records_to_drop = records_read
        while records_to_drop > 0:
            drop_size = min(chunk_size, records_to_drop)
            await handler.dropRecords(0, drop_size)  # Always drop from start
            records_to_drop -= drop_size

        print(f"retrieved {len(all_records)} records")
        json_lines = []
        for record in all_records:
            print(json.dumps(record))
            json_lines.append(json.dumps(record))

        if json_lines:
            with open("./_data/records.jsonl", "a") as ofile:
                ofile.write("\n")
                ofile.write("\n".join(json_lines))

        return all_records


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="BLE scale data tools")
    parser.add_argument(
        "command",
        choices=["cli", "fetch"],
        help="Command to run: interactive command line or fetch data",
    )
    args = parser.parse_args()

    if args.command == "cli":
        asyncio.run(main())
    elif args.command == "fetch":
        asyncio.run(demo_data_fetch())
