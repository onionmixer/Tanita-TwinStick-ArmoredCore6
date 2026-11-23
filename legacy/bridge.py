"""Windows HID/SDL joystick -> Virtual-On gesture mapper -> ViGEm X360 pad."""
import argparse
import json
import os
import sys
import time
from pathlib import Path
from engine import InputState, TwinStickEngine, clamp

DEFAULT_CONFIG = Path(__file__).with_name('config.json')

def read_config(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))

def select_joystick(pygame, conf):
    pygame.joystick.init()
    choices = [pygame.joystick.Joystick(i) for i in range(pygame.joystick.get_count())]
    for i, dev in enumerate(choices):
        print(f'[{i}] {dev.get_name()} axes={dev.get_numaxes()} buttons={dev.get_numbuttons()} hats={dev.get_numhats()}')
    desired = conf['device'].get('name_contains', '').lower()
    idx = int(conf['device'].get('joystick_index', 0))
    candidates = [d for d in choices if desired and desired in d.get_name().lower()]
    if desired and not candidates:
        raise RuntimeError(f'No joystick matched name_contains={desired!r}')
    if candidates: return candidates[0]
    if not 0 <= idx < len(choices):
        raise RuntimeError('No valid joystick found. Run diagnose.py first.')
    return choices[idx]

def read_input(dev, cfg):
    mapping = cfg['device']
    def axis(k):
        i = mapping['axes'].get(k)
        if i is None: return 0.0
        if isinstance(i, str) and i.startswith('hat'):
            # E.g. "hat0x" or "hat1y", if hardware reports directions via POV hats.
            import re
            match = re.fullmatch(r'hat(\d+)([xy])', i)
            if not match: raise ValueError(f'Invalid hat mapping: {i!r}')
            hat_index, dim = int(match.group(1)), match.group(2)
            if hat_index >= dev.get_numhats(): raise ValueError(f'Invalid hat index {hat_index}')
            v = float(dev.get_hat(hat_index)[0 if dim == 'x' else 1])
            return -v if mapping.get('invert', {}).get(k, False) else v
        i = int(i)
        if i < 0 or i >= dev.get_numaxes():
            raise ValueError(f'Axis {k}={i} outside device axis count {dev.get_numaxes()}')
        v = float(dev.get_axis(i)) * (-1 if mapping.get('invert', {}).get(k, False) else 1)
        dz = float(cfg['controls']['deadzone'])
        return 0.0 if abs(v) < dz else clamp((abs(v)-dz)/(1-dz) * (1 if v >= 0 else -1))
    buttons = set()
    for name, idx in mapping['button_indices'].items():
        if idx is not None and int(idx) < dev.get_numbuttons() and dev.get_button(int(idx)):
            buttons.add(name)
    for name, (index, expected_x, expected_y) in mapping.get('hat_buttons', {}).items():
        if not 0 <= index < dev.get_numhats():
            raise ValueError(f'Hat button {name}={index} outside device hat count {dev.get_numhats()}')
        x, y = dev.get_hat(index)
        if (expected_x or expected_y) and (not expected_x or x == expected_x) and (not expected_y or y == expected_y):
            buttons.add(name)
    return InputState(axis('left_x'),axis('left_y'),axis('right_x'),axis('right_y'),buttons)

def send(vpad, vgamepad, out):
    """Set entire report every frame to guarantee all released commands are neutral."""
    vpad.reset()
    xb = vgamepad.XUSB_BUTTON
    keys = {
      'A': xb.XUSB_GAMEPAD_A, 'B': xb.XUSB_GAMEPAD_B, 'X': xb.XUSB_GAMEPAD_X,
      'Y': xb.XUSB_GAMEPAD_Y, 'LB': xb.XUSB_GAMEPAD_LEFT_SHOULDER,
      'RB': xb.XUSB_GAMEPAD_RIGHT_SHOULDER, 'LS': xb.XUSB_GAMEPAD_LEFT_THUMB,
      'RS': xb.XUSB_GAMEPAD_RIGHT_THUMB, 'UP': xb.XUSB_GAMEPAD_DPAD_UP,
      'DOWN': xb.XUSB_GAMEPAD_DPAD_DOWN, 'LEFT': xb.XUSB_GAMEPAD_DPAD_LEFT,
      'RIGHT': xb.XUSB_GAMEPAD_DPAD_RIGHT, 'START': xb.XUSB_GAMEPAD_START,
      'BACK': xb.XUSB_GAMEPAD_BACK
    }
    for name in out.buttons:
        if name in keys: vpad.press_button(button=keys[name])
    vpad.left_trigger_float(float(out.buttons.__contains__('LT')))
    vpad.right_trigger_float(float(out.buttons.__contains__('RT')))
    vpad.left_joystick_float(x_value_float=out.move_x,y_value_float=out.move_y)
    vpad.right_joystick_float(x_value_float=out.camera_x,y_value_float=out.camera_y)
    vpad.update()

def main():
    p = argparse.ArgumentParser(description='TANITA VCD-18-c Virtual-On to AC6 bridge')
    p.add_argument('--config', default=str(DEFAULT_CONFIG))
    p.add_argument('--dry-run', action='store_true', help='Print mapping instead of creating XInput device')
    args = p.parse_args()
    # The game owns foreground focus while this bridge reads the physical pad.
    os.environ['SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS'] = '1'
    import pygame
    cfg = read_config(args.config)
    pygame.init()
    pygame.display.init()
    pygame.display.set_mode((300, 90), flags=pygame.HIDDEN)
    pad = select_joystick(pygame, cfg)
    pad.init()
    print('INPUT:', pad.get_name(), '| Escape / Ctrl+C exits. Test offline first.')
    engine = TwinStickEngine(cfg)
    vg = None
    virtual = None
    if not args.dry_run:
        import vgamepad as vg
        virtual = vg.VX360Gamepad()
        print('Virtual Xbox 360 controller created')
    hz = max(20, min(250, float(cfg['controls'].get('poll_hz', 125))))
    period = 1.0/hz
    next_tick = time.perf_counter()
    last_report = None
    try:
        running = True
        while running:
            pygame.event.pump()
            for event in pygame.event.get():
                if event.type == pygame.QUIT or (event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE):
                    running = False
                if event.type == pygame.JOYDEVICEREMOVED and event.instance_id == pad.get_instance_id():
                    raise RuntimeError('TANITA disconnected; stopped to release virtual controls')
            raw = read_input(pad, cfg)
            out = engine.step(raw, time.perf_counter())
            if virtual:
                send(virtual, vg, out)
            else:
                state = (out.mode, out.gesture, round(out.move_x,2),round(out.move_y,2),round(out.camera_x,2),round(out.camera_y,2),tuple(sorted(out.buttons)))
                if state != last_report:
                    print(state)
                    last_report = state
            next_tick += period
            nap = next_tick - time.perf_counter()
            if nap > 0: time.sleep(nap)
            else: next_tick = time.perf_counter()
    finally:
        if virtual:
            virtual.reset()
            virtual.update()
        pygame.quit()

if __name__ == '__main__':
    try: main()
    except KeyboardInterrupt: print('\nInterrupted')
    except Exception as exc:
        print(f'ERROR: {exc}',file=sys.stderr)
        sys.exit(1)
