R07_open_enclosure — Auto-Pigeon regression cases (written by ../generate.py; run by ci/auto-pigeon/regress.py)

R07.open.1: a one-unit slot through a wall to the void: a real opening
  map: maps/r07_open_1.map
  mechanism: none (control)
  the source leaks
  stages: bsp

  The +x wall is two brushes with a full-height slot of 1 units between them, open to the void. The
  map leaks; a compiler that reports it sealed has closed a real opening. P1+P2 must not change that:
  it keeps material, it does not invent it.
