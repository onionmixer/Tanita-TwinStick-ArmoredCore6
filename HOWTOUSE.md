# How to use the TANITA Twin Stick Bridge

## Play AC6 with ViGEmBus

1. Connect the TANITA VCD-18-c by USB and open `release\Tanita.exe`. The input panel should show `USB GAMESTICK / 1F4F:9001`.
2. Check ViGEmBus status at the bottom of the window. If the driver is missing, select **Open official ViGEmBus downloads** and install it using the official installer. Return to the app after installation. [Official ViGEmBus release](https://github.com/nefarius/ViGEmBus/releases/latest)
3. Select **ViGEmBus (Xbox 360)** and click **Start output**. Wait for **Output connected**. Close any other instance that is creating a virtual pad, including the legacy bridge.
4. In Steam, open **ARMORED CORE VI → Properties → Controller** and enable **Steam Input** for this game. Launch AC6 and use the **Type A** controller layout. [Official AC6 controls](https://www.fromsoftware.jp/manual/armoredcore6/win/operation.html)
5. In the garage, select **Operation → AC Test**. A new save needs to reach the garage first. For offline testing, set the game's startup mode to offline in its network settings before loading the save. [Official garage guide](https://www.fromsoftware.jp/manual/armoredcore6/win/menu1.html)
6. Verify that the AC is stationary with both levers released. Push both forward to move, spread them to jump, and test each weapon. Click **Stop output** or close the app when finished.

## Controls

Lever combinations are listed as left lever / right lever.

| Twin Stick input | AC6 Type A action |
| --- | --- |
| Both levers in the same direction, including diagonals | Move in that direction |
| One lever alone or a mismatched direction outside the combinations below | No movement |
| Behind / ahead, or ahead / behind | Rotate the camera left or right |
| Outward (← / →) | Jump or ascend while held |
| Inward (→ / ←) | Release the ascent input from the lever gesture |
| Left / right grip trigger | Left / right hand weapon |
| Left / right grip gray button | Left / right shoulder weapon |
| Either TURBO alone | Assault boost (L3) |
| Hold both TURBO buttons and move the levers | Camera control while both are held. Releasing either returns to combat controls. |
| Panel D-pad | Digital D-pad. Up uses a repair kit, down scans, and left/right navigate choices. |
| Panel □ / × / ○ / △ | Quick boost / jump / boost / shift or interact |
| SHARE / OPTION / PS | Photo mode / pause menu / target assist |

The two gray buttons on each grip share the same input. Press both TURBO buttons nearly together for camera control. A single press held for 90 ms sends an 85 ms L3 pulse, so pressing the second button late can trigger assault boost first. A short single tap sends the pulse on release.

## Verify Windows input

Open `joy.cpl` from Windows Run. In the virtual gamepad's **Properties**, press the panel × button and push both levers to the right. The button and movement axis should register. Select **Xbox 360 Controller** for ViGEmBus, or the **VHF** gamepad for the custom driver. The physical **USB GAMESTICK** entry shows raw input from the Twin Stick.

If the app sees the physical controller but the game does not respond, check the virtual device first. If Windows sees input and AC6 does not, verify the game's Steam Input override and controller layout.

## Custom HID driver and AC6 DSE error

The locally test-signed custom driver was tested in Windows Game Controllers. On this PC, the app needed to be **Run as administrator** to open the custom driver. Select **Custom HID driver**, then **Start output**. A successful connection is shown as **Output connected**. The unsigned package produced by a local build needs an appropriate driver signature before it can be loaded.

AC6 showed an Easy Anti-Cheat **Driver Signature Enforcement (DSE)** error while Windows test signing was enabled. To restore normal boot for AC6, open **Command Prompt as administrator**, run the following command, and **restart Windows**. [Microsoft TESTSIGNING instructions](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option)

```cmd
bcdedit /set {current} testsigning off
```

After reboot, use **ViGEmBus (Xbox 360)** with Steam Input enabled. The local test-signed driver cannot load in normal boot. The custom output's AC6 compatibility remains unverified.

## Legacy Python prototype

From `legacy\`, run `install.bat` to install the Python dependencies and `run.bat` to start the Xbox 360 output. The calibrated profile is in `legacy\config.json`. Use `diagnose.bat` to inspect physical input and `test_dry_run.bat` to check mapped output in the console.
