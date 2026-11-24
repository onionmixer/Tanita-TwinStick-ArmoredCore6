# How to use the TANITA Twin Stick Bridge

## Play AC6 with ViGEmBus

1. Connect the TANITA VCD-18-c by USB and open `release\Tanita.exe`. The input panel should show `USB GAMESTICK / 1F4F:9001`.
2. Check ViGEmBus status at the bottom of the window. If the driver is missing, select **Open official ViGEmBus downloads** and install it using the official installer. Return to the app after installation. [Official ViGEmBus release](https://github.com/nefarius/ViGEmBus/releases/latest)
3. Select **ViGEmBus (Xbox 360)** and click **Start link**. Wait for **Output connected**. Close any other instance that is creating a virtual pad, including the legacy bridge.
4. In Steam, open **ARMORED CORE VI → Properties → Controller** and enable **Steam Input** for this game. Launch AC6 and use the **Type A** controller layout. [Official AC6 controls](https://www.fromsoftware.jp/manual/armoredcore6/win/operation.html)
5. In the garage, select **Operation → AC Test**. A new save needs to reach the garage first. For offline testing, set the game's startup mode to offline in its network settings before loading the save. [Official garage guide](https://www.fromsoftware.jp/manual/armoredcore6/win/menu1.html)
6. Verify that the AC is stationary with both levers released. Push both forward to move, spread them to jump, and test each weapon. Click **Stop output** or close the app when finished.

## Controls

Lever combinations are listed as left lever / right lever.

| Twin Stick input | AC6 Type A action |
| --- | --- |
| Both levers in the same direction, including diagonals | Move in that direction |
| One lever alone or a mismatched direction outside the combinations below | No movement |
| Back / forward, or forward / back | Rotate the camera left or right |
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

AC6's Type A layout has no independent turn-in-place control for the AC. The opposed fore-and-aft lever gesture sends horizontal right-stick input to rotate the camera. [Official AC6 controller layout](https://www.fromsoftware.jp/manual/armoredcore6/win/operation.html)

## Verify Windows input

Open **Mapping or Test** to see the Twin Stick diagram and all Xbox outputs and AC6 Type A actions at once. Click a numbered marker or a row to highlight the corresponding control and draw a line between them. Green indicates live physical input. Both grip diagrams are visible. **Left grip** and **Right grip** select the corresponding trigger. The panel below the image displays the current mapped output. With output stopped, it is a preview.

Select **Start input test**, release all controls, and follow the prompts for eight lever directions and 19 physical buttons. After each detected input, release it to continue. **Skip input** records a skipped step. The test checks the current bindings without editing them. Leave gamepad output stopped to run the test without sending inputs to the game.

### Change a button assignment

With output and the guided test stopped, choose an action under **Button Assignment (AC6)** and click **Capture input**. Release the Twin Stick controls and press one assignable physical button within four seconds. The modal shows the countdown, live lever and button values, and the detected button. If the deadline passes or you select **Cancel**, the modal displays **Input assign canceled** for one second and closes without changing the mapping. After a button is detected, select **Save mapping** to apply the assignment or **Retry** for another capture.

The list and output preview reflect the saved binding, and Windows remembers it for your user account. Choose **Unassigned** and capture a button to disable it. **Restore defaults** restores the full factory mapping after confirmation.

Both gray buttons on the same grip share a physical signal and therefore share one assignment. Lever gestures and the single- and dual-TURBO behavior are fixed. The editor changes the native app's mapping for both output drivers.

Open `joy.cpl` from Windows Run. In the virtual gamepad's **Properties**, press the panel × button and push both levers to the right. The button and movement axis should register. Select **Xbox 360 Controller** for ViGEmBus, or the **VHF** gamepad for the custom driver. The physical **USB GAMESTICK** entry shows raw input from the Twin Stick.

If the app sees the physical controller but the game does not respond, check the virtual device first. If Windows sees input and AC6 does not, verify the game's Steam Input override and controller layout.

## Custom HID driver and AC6 DSE error

The locally test-signed custom driver was tested in Windows Game Controllers. On this PC, the app needed to be **Run as administrator** to open the custom driver. Select **Custom HID driver**, then **Start link**. A successful connection is shown as **Output connected**. The unsigned package produced by a local build needs an appropriate driver signature before it can be loaded.

AC6 showed an Easy Anti-Cheat **Driver Signature Enforcement (DSE)** error while Windows test signing was enabled. To restore normal boot for AC6, open **Command Prompt as administrator**, run the following command, and **restart Windows**. [Microsoft TESTSIGNING instructions](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option)

```cmd
bcdedit /set {current} testsigning off
```

After reboot, use **ViGEmBus (Xbox 360)** with Steam Input enabled. The local test-signed driver cannot load in normal boot. The custom output's AC6 compatibility remains unverified.

## Legacy Python prototype

From `legacy\`, run `install.bat` to install the Python dependencies and `run.bat` to start the Xbox 360 output. The calibrated profile is in `legacy\config.json`. Use `diagnose.bat` to inspect physical input and `test_dry_run.bat` to check mapped output in the console.
