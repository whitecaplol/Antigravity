# Antigravity
(Not to be confused by that "agentic development platform")

*Note: this mod is in beta. Things have been somewhat tested, but there may still be issues.*

Makes any UI have gravity (how ironic)
- Doesn't matter what it is or what source it came from.

## Features
Adds physics (why not?)
- Agnostic to any UI layer. In other words: It should work regardless of if it's RobTop's menus or some mod's strange UI.
    - Currently works, but all anchor points are adjusted. This may cause issues if some UI element really relies on it.
- Tunable (soon)

## Dependencies
[`box2d`](https://box2d.org/) (For physics)

## Limitations
(or a roadmap)

Currently, consists of some of the following:
- Not ready for prod
- Doesn't disable itself in-game (which is, well, important).
- Bounding boxes are not adjusted when the node is resized
- Out of frame objects may still do physics (notable example includes the main level selector page).
