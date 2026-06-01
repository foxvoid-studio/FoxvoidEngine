# Roadmap: Foxvoid Engine

## Python Metadata

**Targeting maximum development comfort and data-driven inspector features for your scripts.**

- [ ] Slider Limits (`__ranges__`): Constrain numerical values (min/max) within the ImGui inspector to prevent designers from entering values that could break the game's physics or logic (e.g., negative speed).

- [ ] Help Tooltips (`__tooltips__`): Display descriptive text when hovering over variables in the UI, allowing you to quickly remember what a specific variable does without having to open the source code.

- [ ] Visibility Control (`__hide__` / `__expose__`): Override the default UI visibility rules. This allows you to hide public variables that shouldn't be tweaked manually, or force the display of private variables (starting with _) for debugging purposes.

- [ ] Architecture Safety (`__require_components__`): Automatically verify that the GameObject possesses the vital C++ components (like RigidBody2d or Animator2d) required by the Python script. If missing, display a warning and a one-click button in the Inspector to add them.