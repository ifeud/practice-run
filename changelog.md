# 1.4.0

- The mod no longer hooks the game's scheduler. Each effect keeps track of its
  own half-rate timing instead, which removes the last bit of per-frame overhead
  the mod added and one possible source of conflicts with other mods.
- Node ids are prefixed with the mod id, so the mod's nodes can no longer be
  confused with another mod's.
- A source code and Discord link on the mod page.

# 1.3.1

- "Recommended" and "Reset all" buttons at the bottom of the performance popup:
  one click for a known-good set of options, one click to undo it.
- The mod's particle bookkeeping is thrown away when a new level starts.
- A freshly loaded level gets the effects your settings ask for on its first
  frame instead of a moment later.

# 1.3.0

- The pause menu button is a proper switch now. Pressing it while a run is going
  on switches the mod off and puts the level back into normal mode without
  restarting, instead of starting yet another run.
- New setting: "Enable by default".
- Six new performance options: Auto performance (watches the framerate and
  reacts to it), Half-rate particles, Freeze off-screen, Fewer effects,
  Half-size bursts and No object glow.
- "No particles" now stops the particle simulation instead of only hiding the
  sprites, so it saves processor time and not just drawing.

# 1.2.1

- Options that are switched off are dimmed in the performance popup, so the ones
  that are on are obvious at a glance.
- Fixed a build error: CCMenuItemToggler is not in the cocos2d namespace.

# 1.2.0

- The pause menu button shows the game's red "no entry" mark while a run is
  active, so you can see at a glance whether it is tracking.
- New: performance settings. A gear button in the corner of the pause menu opens
  a popup with ten framerate options for slower PCs.

# 1.1.0

- The button is a purple round icon like the game's own pause menu buttons, and
  it sits in the button row right after the practice button. Before this it was
  a text button in the corner of the pause menu.
- The mod icon is purple with a transparent background.

# 1.0.0

- Purple practice button in the pause menu.
- Pressing it starts a practice run from 0% and counts your deaths.
- A run finished with 0 deaths is saved as a normal completion; a run with one
  or more deaths stays a practice completion.
