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
                print(f"\n< {len(payload_string)}")
                print("< " + indent(yaml_string, "+ ").lstrip("+ "))
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


if __name__ == "__main__":
    asyncio.run(main())
