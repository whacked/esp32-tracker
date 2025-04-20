Import("env")
import os
import socket

header_file = "include/build_metadata.h"


def get_local_ip_address():
    # print ip adddress of my wifi network
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("10.0.0.1", 1))  # connect() for UDP doesn't send packets
    local_ip_address = s.getsockname()[0]
    return local_ip_address


def get_build_number():
    build_number = 0
    if os.path.exists(header_file):
        with open(header_file, "r") as f:
            for line in f.readlines():
                if not line.startswith("#define BUILD_NUMBER"):
                    continue
                build_number = int(line.split()[-1].strip())
    return build_number


# Increment and write
current = get_build_number()
local_ip_address = get_local_ip_address()
next_build = current + 1

print("=== SETTING BUILD METADATA ===")

with open(header_file, "w") as f:
    f.write("#pragma once\n")
    f.write(f"#define BUILD_NUMBER {next_build}\n")
    print(f"Build #{next_build}")

    if local_ip_address.startswith("192.168.1."):
        f.write("#define HOME_SET 1\n")
        print("Home set")
    else:
        print("non-home set")
