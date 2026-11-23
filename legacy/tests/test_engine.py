import copy
import json
import unittest
from pathlib import Path
from engine import TwinStickEngine, InputState

BASE=json.loads((Path(__file__).resolve().parents[1]/'config.example.json').read_text())

class EngineTest(unittest.TestCase):
    def setUp(self): self.eng=TwinStickEngine(copy.deepcopy(BASE))
    def run_state(self, lx=0,ly=0,rx=0,ry=0,buttons=(),t=0.0):
        return self.eng.step(InputState(lx,ly,rx,ry,set(buttons)),t)
    def test_eight_combos(self):
        cases=[((-1,0,1,0),'jump'),((1,0,-1,0),'cancel_ascend'),((0,-1,0,1),'turn_left'),((0,1,0,-1),'turn_right'),((0,1,0,1),'move'),((0,-1,0,-1),'move'),((-1,0,-1,0),'move'),((1,0,1,0),'move')]
        for vals,gesture in cases:
            with self.subTest(vals=vals): self.assertEqual(self.run_state(*vals).gesture,gesture)
    def test_jump_holds_then_releases(self):
        self.assertIn('A',self.run_state(-1,0,1,0).buttons)
        self.assertNotIn('A',self.run_state(0,0,0,0).buttons)
    def test_jump_overrides_move(self):
        o=self.run_state(-1,0,1,0)
        self.assertEqual((o.move_x,o.move_y),(0,0))
    def test_single_lever_and_diagonal(self):
        for o in (self.run_state(lx=1,ly=1), self.run_state(rx=1,ry=1)):
            self.assertEqual((o.move_x,o.move_y,o.gesture),(0,0,'idle'))
        o=self.run_state(lx=1,ly=1,rx=1,ry=1)
        self.assertEqual((o.move_x,o.move_y),(1,1))
    def test_turn_direction(self):
        self.assertLess(self.run_state(ly=-1,ry=1).camera_x,0)
        self.assertGreater(self.run_state(ly=1,ry=-1).camera_x,0)
    def test_move_requires_matching_directions(self):
        for lx, ly, rx, ry in ((0,1,1,0), (0,1,1,1), (0,1,0,.3)):
            with self.subTest(levers=(lx,ly,rx,ry)):
                o=self.run_state(lx,ly,rx,ry)
                self.assertEqual((o.move_x,o.move_y,o.gesture),(0,0,'idle'))
        o=self.run_state(.2,1,.3,1)
        self.assertEqual((o.move_x,o.move_y),(0,1))
    def test_weapon_buttons(self):
        o=self.run_state(buttons=('left_trigger','right_trigger','left_aux'))
        self.assertTrue({'LT','RT','LB'} <= o.buttons)
    def test_dual_turbo_camera_mode(self):
        both = ('left_turbo', 'right_turbo')
        camera = self.run_state(ly=1, ry=1, buttons=both)
        self.assertEqual((camera.mode, camera.move_y, camera.camera_y), ('camera', 0, 1))
        self.assertFalse(camera.buttons)
        combat = self.run_state(ly=1, ry=1, buttons=('left_turbo',), t=.1)
        self.assertEqual((combat.mode, combat.move_y, combat.camera_y), ('combat', 1, 0))
        self.assertFalse(combat.buttons)
        self.assertFalse(self.run_state(buttons=('left_turbo',), t=.25).buttons)
        self.assertFalse(self.run_state(t=.3).buttons)
        self.assertEqual(self.run_state(buttons=('left_aux', 'right_aux'), t=.4).buttons, {'LB', 'RB'})

    def test_turbo_single_and_chord(self):
        self.assertFalse(self.run_state(buttons=('left_turbo',), t=0).buttons)
        self.assertEqual(self.run_state(buttons=('left_turbo',), t=.10).buttons, {'LS'})
        self.assertFalse(self.run_state(buttons=('left_turbo', 'right_turbo'), t=.11).buttons)
        self.assertFalse(self.run_state(buttons=('left_turbo',), t=.12).buttons)
        self.run_state(t=.13)
        self.run_state(buttons=('right_turbo',), t=.14)
        self.assertEqual(self.run_state(buttons=('right_turbo',), t=.24).buttons, {'LS'})
        self.assertFalse(self.run_state(buttons=('right_turbo',), t=.34).buttons)

    def test_short_turbo_tap(self):
        self.run_state(buttons=('left_turbo',), t=0)
        self.assertEqual(self.run_state(t=.04).buttons, {'LS'})
        self.assertEqual(self.run_state(t=.06).buttons, {'LS'})
        self.assertFalse(self.run_state(t=.14).buttons)
    def test_deadzone_direction(self):
        self.assertEqual(self.run_state(lx=-.5,rx=.5).gesture,'idle')
    def test_assist_button(self):
        self.assertIn('RS', self.run_state(buttons=('target_assist',)).buttons)

if __name__=='__main__': unittest.main()
