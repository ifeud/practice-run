# Practice Run

Practice mode with a memory: this mod counts the deaths of your practice run, so
a run you finish without dying still counts as a proper completion.

## The button

The pause menu gets one more purple button, in the same row as the game's own
buttons and right after the practice button.

* **No run going on:** pressing it turns practice mode on and restarts the level
  from 0%. From that point on every death is counted.
* **Finish with 0 deaths:** the completion is saved as a normal completion -
  normal percentage, stars and coins, the usual Level Complete screen.
* **Died at least once:** you can keep respawning from your checkpoints as
  usual, but reaching the end only counts as a practice completion.
* **A run going on:** pressing the button switches the mod off again. Tracking
  stops and the level goes back to normal mode without restarting, so you keep
  your position, your attempt and your progress.

While a run is active the button carries the same red "no entry" mark the game's
own practice button uses, and a small label in the corner shows how many times
you have died. The setting **Enable by default** starts a tracked run on every
level you enter.

## Performance options

A smaller purple button in the bottom-left corner of the pause menu opens a
popup with 16 options that save framerate on slower PCs. They all apply
immediately, even in the middle of a level, and every one of them can be
switched back off. **Recommended** switches on the ten options that save the
most work without hiding anything you need to see, and **Reset all** undoes it.

The interesting one is **Auto performance**: it measures your framerate while
you play, steps effects off by itself when you drop below 50 fps and back on
when you are comfortable again, and never touches the options you ticked
yourself. The others are mostly ordinary "stop drawing that" switches, plus
these five:

| Option | What it does |
| --- | --- |
| Half-rate particles | Effects update every other frame and are advanced two frames' worth of time, so they look the same for half the simulation work. Effects take turns, so the work stays spread evenly |
| Freeze off-screen | Effects that have scrolled out of view stop being updated until they come back |
| Fewer effects | Halves Geometry Dash's own limit on how many effects may exist at once |
| Half-size bursts | Every effect is built with half as many particles |
| No object glow | Hides the glow layers and the glow of every object, which is a lot of additive sprites on a decorated level |

## Settings

All of them are in Geode's own settings list (Geometry Dash -> Geode -> Mods ->
Practice Run):

* **Show pause menu button** - hides both buttons if you don't want them.
* **Show run label** - hides the death counter.
* **Enable by default** - starts a tracked run on every level you enter.
* the 16 performance options above.

## Notes

* Windows only, Geometry Dash 2.2081, built with Geode 5.10.1.
* The mod tracks the most recent run you started with the button. Restarting the
  level or toggling practice mode by hand does not reset the death count - press
  the button again for a fresh run.
* The performance options change what is drawn and simulated, never the physics,
  so they cannot make you pass or fail a level. They do change how a level
  looks though, and hiding portals or the ground makes it harder to read.
* Bugs and suggestions: https://discord.gg/PDUu5ENwQu
* Source code: https://github.com/ifeud/practice-run
