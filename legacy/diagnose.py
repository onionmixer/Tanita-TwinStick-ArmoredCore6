"""List and inspect SDL joystick signals without modifying the game."""
import argparse
import time
import pygame

p = argparse.ArgumentParser()
p.add_argument('--index',type=int,default=0)
p.add_argument('--seconds',type=float,default=25)
a = p.parse_args()
pygame.init()
pygame.display.set_mode((240,80),flags=pygame.HIDDEN)
pygame.joystick.init()
print('Devices:',pygame.joystick.get_count())
for i in range(pygame.joystick.get_count()):
    d = pygame.joystick.Joystick(i)
    print(f'[{i}] {d.get_name()} GUID={d.get_guid()} axes={d.get_numaxes()} buttons={d.get_numbuttons()} hats={d.get_numhats()}')
if not 0 <= a.index < pygame.joystick.get_count():
    raise SystemExit('Invalid --index. Check the device list above.')
d=pygame.joystick.Joystick(a.index)
d.init()
print('\nMove one lever / press one button at a time. Only changes are shown.')
last=None
end=time.monotonic()+a.seconds
while time.monotonic()<end:
    pygame.event.pump()
    axes=tuple(round(d.get_axis(j),2) for j in range(d.get_numaxes()))
    buttons=tuple(j for j in range(d.get_numbuttons()) if d.get_button(j))
    hats=tuple(d.get_hat(j) for j in range(d.get_numhats()))
    state=(axes,buttons,hats)
    if state != last:
        print('axes=',axes,' pressed=',buttons,' hats=',hats,flush=True)
        last=state
    time.sleep(.05)
pygame.quit()
