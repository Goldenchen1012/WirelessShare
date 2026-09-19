# WirelessShare

WirelessShare uses one LOLIN S2 Mini on each Windows computer to transfer clipboard text, PNG images,
files, and folders over a direct Wi-Fi connection. The computers communicate with their local device
through USB CDC; the two devices communicate over WPA2 and TCP.

## First-time setup

1. Open `FW/WirelessDevice/WirelessDevice.ino` in Arduino IDE.
2. Select **LOLIN S2 Mini** and enable **USB CDC On Boot**, then flash the same firmware to both devices.
3. Build and run `SW/WirelessShareApp/WirelessShareApp.pro` on both Windows computers.
4. Select the local USB CDC COM port on each computer.
5. Set one computer to **A - Access Point** and the other to **B - Station**.
6. Enter the same 8-63 byte pairing password on both computers and click **Connect**.

When both applications show `Peer connected`, copying supported content on either computer sends it to
the other computer. Received files are verified and then moved to `Downloads/WirelessShare`; their paths
are placed on the Windows clipboard.

## Behavior and limits

- A single file may be up to 1 GiB. Text and PNG clipboard payloads are limited to 64 MiB.
- Files are streamed in 2 KiB chunks; an ESP32 never stores a complete transfer.
- Each transport frame has CRC-32 protection, and a complete transfer is acknowledged only after its
  SHA-256 digest is verified.
- An interrupted receive is deleted. The sender restarts a transfer after reconnecting.
- Symbolic links are skipped, and received relative paths are checked before writing to disk.
- Closing the main window leaves the application running in the Windows system tray.

## Build verification

The desktop application requires Qt 5 with the Widgets and Serial Port modules. A small protocol test is
available at `SW/ProtocolTests/ProtocolTests.pro`. Rebuild `WirelessShareApp.pro` before running; the
pre-existing checked-in `SW/build-WirelessShareApp-...` directory contains generated files from the
original empty project and is not a release package.
