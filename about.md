# Antigravity
(Not to be confused by that "agentic development platform")

<img src="logo.png" width="150" alt="Antigravity logo" />

*Note: this mod is in alpha (in other words, don't expect anything to work well)*

Makes any UI have gravity (how ironic)
- Doesn't matter what it is or what source it came from.

## Features
Adds physics (why not?)
- Agnostic to any UI layer. In other words: It should work regardless of if it's RobTop's menus or some mod's strange UI.
    - Currently it doesn't fully work yet, see [limitations](#limitations) for details.
- Tunable (one day)

## Dependencies
[`box2d`](https://box2d.org/) (For physics)

## Limitations
Currently, consists of some of the following:
- Gravity doesn't work on newly spawned nodes
- Not fine grained
- Poor mod integration (if you install this it works with the main UI but ironcially not Geode's own menu yet)
    - Note that the goal isn't to hardcode anything (except eventually disabling the mod in-game which i somehow haven't done yet)
