# 0.5.4
- Stronger persistence
- Reduced reliance on absolute position when centering anchor points
- Fix up the text nodes (with a bit of help from Geode's own [`TextInputNodeFix`](https://github.com/geode-sdk/geode/blob/main/loader/src/ui/nodes/TextInput.cpp))

# 0.5.3
- Better persistence of positional and angular velocity
- Text node improvements
- Refinements

# 0.5.2
- More relaxed (and robust) collection
- Better material properties

# 0.5.1
- Quite a few bug fixes
- Correcting sliders and fonts

# 0.5.0
- More proper state resetting.
- Ability to pause and reset current scene using keybinds.
- Better size resolution

# 0.4.5
- Much more robust types.

# 0.4.4
- 2.2091 Support on Geode 6.0.0-alpha.1
- Better isolation of contexts.

# 0.4.3
- First beta release
- Much more friendly in the editor.

# 0.4.2
- Better state reconstruction to ensure that reused elements don't look weird when programs modify things later on.
- Better understanding of when to cleanup and when not to.

# 0.4.1
- Code structure changes
- Marginally better cleanup in case b2Worlds fail mid-way through.

# 0.4.0
- Better text segmentation
- Refactored to use `AntigravityContext` instead of hooking into *every* `CCNode`

# 0.3.1
- Better viewport containment checks

# 0.3.0
- More robust
- Potentially ready for showcasing.

# 0.2.1
- Slightly better node identification
- Better anchor point corrections
- More oomph to nodes that animate in or out.

# 0.2.0
- Velocities are preserved
- Settings
- Even more robust layer identification.

# 0.1.1
- More robust node selection by not including nodes which have zero size
    - Thanks to @lunarawa on discord for finding this

# 0.1.0
- Migrate to v3.1.0 of box2d
- More robust layer resolution
- More robust guardrails in case box2d fails to make world for any reason
    - This only happens if there's a ton of worlds made (for some reason).

# 0.0.2
- Primitively works (to some degree)

# 0.0.1
- Doesn't even work yet lmao
