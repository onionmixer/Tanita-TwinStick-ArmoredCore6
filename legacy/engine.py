"""Pure Virtual-On to AC6 input mapper. No hardware dependencies."""
from dataclasses import dataclass, field
from math import hypot
from typing import Dict, Set, Tuple

@dataclass
class InputState:
    lx: float = 0.0
    ly: float = 0.0  # positive = physical lever UP (normalized)
    rx: float = 0.0
    ry: float = 0.0
    buttons: Set[str] = field(default_factory=set)

@dataclass
class OutputState:
    move_x: float = 0.0
    move_y: float = 0.0
    camera_x: float = 0.0
    camera_y: float = 0.0
    buttons: Set[str] = field(default_factory=set)
    mode: str = 'combat'
    gesture: str = 'idle'

class TwinStickEngine:
    """Special combos take priority over movement in matching directions.

    Directions use an eight-way threshold. Matching active axes retain analog output.
    Holding both TURBO buttons redirects the levers to the camera.
    A single TURBO press waits briefly so a chord can take priority.
    """
    def __init__(self, config: dict):
        self.cfg = config
        self._turbo_pending_at = None
        self._turbo_chord = False
        self._prev_turbo = frozenset()
        self._assault_pulse_until = None

    def _dir(self, x: float, y: float) -> Tuple[int, int]:
        threshold = float(self.cfg['controls']['gesture_threshold'])
        return ((1 if x >= threshold else -1 if x <= -threshold else 0),
                (1 if y >= threshold else -1 if y <= -threshold else 0))

    def step(self, raw: InputState, now: float) -> OutputState:
        controls = self.cfg['controls']
        b = set(raw.buttons)
        turbo = frozenset(b & {'left_turbo','right_turbo'})
        output = OutputState(mode='camera' if len(turbo) == 2 else 'combat')
        if 'left_aux' in b: output.buttons.add('LB')
        if 'right_aux' in b: output.buttons.add('RB')
        if 'left_trigger' in b: output.buttons.add('LT')
        if 'right_trigger' in b: output.buttons.add('RT')

        # Physical system buttons available on some device reports.
        for name, xb in controls.get('extra_buttons', {}).items():
            if name in b and xb in {'A','B','X','Y','LB','RB','LS','RS','UP','DOWN','LEFT','RIGHT','START','BACK'}:
                output.buttons.add(xb)

        chord_window = float(controls['turbo_chord_sec'])
        pulse_sec = float(controls.get('assault_boost_pulse_sec', 0.085))
        if len(turbo) == 2:
            self._turbo_chord = True
            self._turbo_pending_at = None
            self._assault_pulse_until = None
        elif self._turbo_chord:
            # Suppress single TURBO until both buttons have been released.
            if not turbo:
                self._turbo_chord = False
                self._turbo_pending_at = None
        elif turbo:
            if not self._prev_turbo:
                self._turbo_pending_at = now
            # Hold past the chord window to send one assault boost input.
            if self._turbo_pending_at is not None and now - self._turbo_pending_at >= chord_window:
                self._assault_pulse_until = now + pulse_sec
                self._turbo_pending_at = None
        else:
            # A very quick tap must not be lost. Send a timed pulse on release.
            if self._prev_turbo and self._turbo_pending_at is not None:
                self._assault_pulse_until = now + pulse_sec
            self._turbo_pending_at = None
        if self._assault_pulse_until is not None:
            if now < self._assault_pulse_until and not self._turbo_chord:
                output.buttons.add('LS')
            else:
                self._assault_pulse_until = None
        self._prev_turbo = turbo

        l = self._dir(raw.lx, raw.ly)
        r = self._dir(raw.rx, raw.ry)
        pure_l = l[1] == 0 and l[0] != 0
        pure_r = r[1] == 0 and r[0] != 0
        updown_l = l[0] == 0 and l[1] != 0
        updown_r = r[0] == 0 and r[1] != 0
        if output.mode == 'camera':
            # Use the average of both levers as the camera vector.
            output.camera_x = clamp((raw.lx + raw.rx) * 0.5)
            output.camera_y = clamp((raw.ly + raw.ry) * 0.5)
            output.gesture = 'camera'
        elif pure_l and pure_r and l[0] == -1 and r[0] == 1:
            output.buttons.add('A')
            output.gesture = 'jump'
        elif pure_l and pure_r and l[0] == 1 and r[0] == -1:
            output.gesture = 'cancel_ascend'
        elif updown_l and updown_r and l[1] != r[1]:
            # L down, R up -> rotate left; reverse -> rotate right.
            # Output on right stick (camera controls AC6 orientation).
            output.camera_x = -float(controls['turn_speed']) if l[1] == -1 else float(controls['turn_speed'])
            output.gesture = 'turn_left' if l[1] == -1 else 'turn_right'
        elif l == r and l != (0, 0):
            # Only axes shared by the matching eight-way directions contribute.
            # Output Y positive means XInput UP; input Y has already been normalized.
            output.move_x = clamp((raw.lx + raw.rx) * 0.5) if l[0] else 0.0
            output.move_y = clamp((raw.ly + raw.ry) * 0.5) if l[1] else 0.0
            output.gesture = 'move' if hypot(output.move_x, output.move_y) > 0.05 else 'idle'
        return output

def clamp(v: float) -> float:
    return max(-1.0, min(1.0, v))
