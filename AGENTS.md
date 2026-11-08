# CastPad Project Rules

## Versioning (always required)

Every time you make **any** change to the project (firmware code, web assets, config, etc.),
increment the firmware version number as part of that same change — no exceptions.

- Use semantic versioning `MAJOR.MINOR.PATCH`:
  - **PATCH** (e.g. 1.2.0 → 1.2.1): bug fixes, small tweaks, UI/CSS fixes. This is the default.
  - **MINOR** (e.g. 1.2.1 → 1.3.0): new features or new settings/options.
  - **MAJOR**: only if the user asks for it or for breaking changes (e.g. config format changes that wipe saved settings).
- The version currently appears in **both** of these places and they must always match:
  - `src/main.cpp` — `doc["version"] = "X.Y.Z";` in `sendSettings()`
  - `include/WebAssets.h` — the default `vX.Y.Z` text in the `fwVersion` span on the Settings tab
- Search for the old version string across `src/` and `include/` to make sure no copy is missed.
- Bump the version once per user request (not once per individual file edit).
- Mention the new version number in your summary to the user.

