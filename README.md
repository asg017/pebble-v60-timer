# v60-timer

A Pebble watchapp that runs my V60 pour-over recipe and warns me when to pour next with vibrations.

## The recipe

My current V60 pourover recipe with 18-20g of coffee is:

- `0:00` -> pour 60g, bloom
- `0:45` -> pour 30g, to 90g
- `1:30` -> pour 90g, to 180g
- `2:15` -> pour 60g, to 240g
- `3:00` - pour 60g, to 300g

This app, on start, guides you through the above recipe. A warning vibration occurs at the very start (`0:00`), then 5 seconds before each pour is supposed to start (`0:40`, `1:25`, etc.).


This gives me ~20 seconds to run around and do something during my morning routine: grab a mug, open a window, fill my water bottle. 

## Building

Requires `pebble-tool` and an installed SDK (`uv tool install pebble-tool`, then
`pebble sdk install latest`). Developed against pebble-tool 5.0.40 / SDK 4.33.1.

```bash
pebble build            # -> build/v60-timer.pbw, all 7 platforms
```

## Installing

### On the watch (Dev Connect)

1. Phone app → Devices → ⋮ on your watch → **Enable Dev Connect**, sign in with your
   Pebble account. Keep the phone app in the foreground.
2. On the laptop, once: `pebble login` (same Pebble account).
3. `pebble install --cloudpebble`

If it hangs at "waiting for phone to connect", the phone hasn't joined the relay: check Dev
Connect is on, the app is foregrounded, the watch is connected over Bluetooth, and both
sides use the same account.

### On the watch (legacy developer connection)

Phone app → Settings → Developer Mode → Developer Connection → note the IP, then
`pebble install --phone <ip>` from the same network (a Tailscale IP works too).

### In the emulator

```bash
pebble install --emulator basalt              # or flint / emery / gabbro …
pebble emu-button --emulator basalt click select   # press SELECT to start
pebble logs --emulator basalt                 # cue log lines: "t=45 step 2 (pour 90g)"
pebble screenshot --emulator basalt shot.png
pebble kill
```

## Changing the recipe

Edit the `STEPS` table at the top of `src/c/v60-timer.c` and rebuild.
