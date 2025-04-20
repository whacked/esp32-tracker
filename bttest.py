import asyncio
import json
import time
from asyncio import Queue as AsyncQueue
from textwrap import indent
from typing import Optional

import yaml
from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice

SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

TARGET_NAME = "ESP32-Scale"

response_queue: AsyncQueue[str] = AsyncQueue()


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


async def main():
    esp_device = await acquire_device()
    if not esp_device:
        print("ESP32 device not found")
        return

    async with BleakClient(esp_device.address) as client:
        print(f"Connected to {esp_device.name} [{esp_device.address}]")

        # toriaezu sync the time
        await client.write_gatt_char(
            RX_UUID, (f"setTime {int(time.time())}\n").encode()
        )

        # Notification handler
        def handle_rx(_, data):
            payload_string = data.decode(errors="ignore")
            try:
                mixed_payload = json.loads(payload_string)
                yaml_string = yaml.safe_dump(mixed_payload, default_flow_style=False)
                print(f"\n< {len(payload_string)}")
                print("< " + indent(yaml_string, "+ ").lstrip("+ "))
                response_queue.put_nowait(payload_string)
            except json.JSONDecodeError:
                print(f"< {payload_string}")

        await client.start_notify(TX_UUID, handle_rx)

        while True:
            # Read command in a separate thread, so event loop remains unblocked
            cmd = await asyncio.to_thread(input, "> ")
            cmd = cmd.strip()
            if not cmd:
                continue
            if cmd.lower() == "exit":
                await client.stop_notify(TX_UUID)
                await client.disconnect()
                break
            await client.write_gatt_char(RX_UUID, (cmd + "\n").encode())


async def demo_data_fetch():
    esp_device = await acquire_device()
    async with BleakClient(esp_device.address) as client:
        print(f"Connected to {esp_device.name} [{esp_device.address}]")

        # Add the notification handler setup
        def handle_rx(_, data):
            payload_string = data.decode(errors="ignore")
            try:
                mixed_payload = json.loads(payload_string)
                yaml_string = yaml.safe_dump(mixed_payload, default_flow_style=False)
                print(f"\n< {len(payload_string)}")
                print("< " + indent(yaml_string, "+ ").lstrip("+ "))
                response_queue.put_nowait(payload_string)
            except json.JSONDecodeError:
                print(f"< {payload_string}")

        await client.start_notify(TX_UUID, handle_rx)

        # Now continue with the rest of the function
        await client.write_gatt_char(RX_UUID, b"getStatus\n")
        status_response = await wait_for_response()
        status = json.loads(status_response)
        total_records = status["bufferSize"]

        all_records = []
        chunk_size = 5
        records_read = 0

        # First phase: Read all records
        while records_read < total_records:
            cmd = f"readBuffer {records_read} {chunk_size}\n"
            await client.write_gatt_char(RX_UUID, cmd.encode())
            chunk_response = await wait_for_response()
            chunk_data = json.loads(chunk_response)

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
            cmd = f"dropRecords 0 {drop_size}\n"  # Always drop from start
            await client.write_gatt_char(RX_UUID, cmd.encode())
            await wait_for_response()
            records_to_drop -= drop_size

        print(f"retrieved {len(all_records)} records")
        for record in all_records:
            print(record)

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
