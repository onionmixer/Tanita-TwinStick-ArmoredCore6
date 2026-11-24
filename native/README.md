# Native implementation notes

The [root README](../README.md#build-from-source) contains build commands and release output paths. For operation and troubleshooting, see [HOWTOUSE.md](../HOWTOUSE.md).

- `App.cpp` owns the Win32 GUI, HID report decoding, device arrival/removal tracking, and output connections. It identifies the physical gamepad using USB ID `1F4F:9001` and the Generic Desktop gamepad or joystick collection.
- `Mapper.h` implements matching eight-way lever movement, special lever gestures, and timed TURBO behavior. The app runs a timer so TURBO timing progresses between input reports.
- `Probe.cpp` lists the HID capabilities of the connected Twin Stick. The tested device reports ID 1 in a 64-byte input report, six 8-bit axis values, one hat with a null state, and 14 button usages. The app decodes HID usages directly.
- `Report.h` defines the fixed-size app-to-driver state report. The Win32 app shares this state between its Xbox 360 and custom HID outputs.
- `driver/Driver.c` provides a KMDF/VHF gamepad for the custom output. It accepts a single feeder, validates the state report, and sends neutral input on feeder cleanup. Generic VHF output has been verified in Windows Game Controllers. AC6 compatibility remains unverified.

## Driver development and deployment

The driver is root-enumerated using the hardware ID `ROOT\TanitaHid`. The build creates an unsigned catalog. Local test deployment uses a test-signed SYS and matching catalog, a locally trusted test certificate, and a reboot into Windows test-signing mode. For regular Windows boot, obtain a Windows-trusted production driver signature. Keep development certificates, private keys, signing tools, and local test packages out of public source and release archives. [Microsoft manual deployment](https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/test-a-driver-package-manual-deployment) · [Microsoft driver signing policy](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later-)

Use one active output at a time. The application works for diagnostics without a gamepad backend. Switching backends requires stopping the current output first. On the current Windows 10 test installation, opening the custom driver requires elevation. The ViGEmBus backend uses the separately installed driver via statically linked ViGEmClient.
