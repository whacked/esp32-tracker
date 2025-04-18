import asyncio
import json
import time
from textwrap import indent

import yaml
from bleak import BleakClient, BleakScanner

SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

TARGET_NAME = "ESP32-Scale"


async def main():
    print("Scanning for ESP32...")
    devices = await BleakScanner.discover()
    esp_device = next((d for d in devices if d.name and TARGET_NAME in d.name), None)
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
                print(indent(yaml_string, "+ ").lstrip("+ "))
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
                # this has a problem where after we quit
                # we can no longer reconnect immediately
                # (maybe a certain wait will work, assuming the server drops the client after idle)
                # but simply doint client.disconnect() does not fix it
                break
            await client.write_gatt_char(RX_UUID, (cmd + "\n").encode())

        await client.stop_notify(TX_UUID)


if __name__ == "__main__":
    asyncio.run(main())
