# RAVBA PokeAuto changes

- Internal RetroPad injection only; no ViGEm or operating-system virtual gamepad.
- Physical keyboard/controller input remains enabled and is ORed with automation input.
- Background execution because automation input is generated inside `systemReadJoypad`.
- Embedded FireRed BPRE revision 1 starter sequence recorded at approximately 10x.
- Direct EWRAM PID/OTID read and Generation III shiny calculation.
- Stops automatically and writes `PokeAuto-found.txt` when shiny is found.
- RetroAchievements source integration is left unchanged; automated resets are routed
  through the existing `Reset` command so the normal `RA_OnReset()` path executes.
