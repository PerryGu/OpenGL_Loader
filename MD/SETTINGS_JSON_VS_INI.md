# Settings File Format: JSON vs INI

## Current Implementation (INI)

The application currently uses INI format (`app_settings.ini`) for storing settings.

### INI Format Pros:
- ✅ **Simple and human-readable** - Easy to edit manually
- ✅ **No external dependencies** - Simple parsing code
- ✅ **Windows-friendly** - Familiar format for Windows users
- ✅ **Lightweight** - Minimal parsing overhead
- ✅ **Works well for flat structures** - Good for simple key-value pairs

### INI Format Cons:
- ❌ **Limited structure** - Only supports sections, no nesting
- ❌ **Verbose for complex data** - Colors require separate R, G, B keys
- ❌ **Manual parsing** - More error-prone than using a library
- ❌ **No standard** - Different implementations have different rules
- ❌ **Type handling** - All values are strings, need manual conversion

### Current INI Structure:
```ini
[Grid]
Size=20.0
Spacing=1.0
Enabled=true
MajorLineColorR=0.5
MajorLineColorG=0.5
MajorLineColorB=0.5
...

[Environment]
BackgroundColorR=0.45
BackgroundColorG=0.55
BackgroundColorB=0.60
BackgroundColorA=1.0
```

## JSON Format Alternative

### JSON Format Pros:
- ✅ **Standard format** - Widely supported and understood
- ✅ **Nested structures** - Better for complex data
- ✅ **Type support** - Numbers, booleans, arrays, objects
- ✅ **More compact** - Colors as arrays: `[0.5, 0.5, 0.5]`
- ✅ **Library support** - Many robust JSON libraries available
- ✅ **Future-proof** - Easier to extend with new features
- ✅ **Better for arrays** - Recent files as JSON array

### JSON Format Cons:
- ❌ **Requires library** - Need to add dependency (or write parser)
- ❌ **Less human-readable** - Slightly more complex syntax
- ❌ **Slightly more verbose** - More characters for simple values

### Proposed JSON Structure:
```json
{
  "camera": {
    "position": [0.0, 9.0, 30.0],
    "focusPoint": [0.0, 0.0, 0.0],
    "yaw": -90.0,
    "pitch": 0.0,
    "zoom": 45.0,
    "orbitDistance": 30.0
  },
  "window": {
    "posX": -1,
    "posY": -1,
    "width": 1920,
    "height": 1080
  },
  "grid": {
    "size": 20.0,
    "spacing": 1.0,
    "enabled": true,
    "majorLineColor": [0.5, 0.5, 0.5],
    "minorLineColor": [0.3, 0.3, 0.3],
    "centerLineColor": [0.8, 0.2, 0.2]
  },
  "environment": {
    "backgroundColor": [0.45, 0.55, 0.60, 1.0]
  },
  "recentFiles": [
    "path/to/file1.fbx",
    "path/to/file2.fbx"
  ]
}
```

## Recommendation

**I recommend switching to JSON** for the following reasons:

1. **Better structure** - Colors as arrays instead of separate R/G/B keys
2. **Easier to extend** - Adding new settings is simpler
3. **Standard format** - More maintainable long-term
4. **Library support** - Can use a well-tested JSON library
5. **Future features** - Better support for complex settings

## Implementation Options

### Option 1: Use nlohmann/json (Recommended)
- Header-only library (single header file)
- Modern C++ API
- Easy to use
- No build system changes needed

### Option 2: Use rapidjson
- Fast and lightweight
- Header-only option available
- More C-style API

### Option 3: Simple custom parser
- No dependencies
- Limited features
- More maintenance burden

## Migration Plan

If we switch to JSON:

1. **Add JSON library** (e.g., nlohmann/json)
2. **Update AppSettings structure** - No changes needed (same data)
3. **Update loadSettings()** - Parse JSON instead of INI
4. **Update saveSettings()** - Write JSON instead of INI
5. **Backward compatibility** - Can auto-convert INI to JSON on first run
6. **Update file extension** - `app_settings.json` instead of `.ini`

## Current Status

For now, I've added Grid and Environment settings to the **existing INI system**. This works perfectly fine and doesn't require any external dependencies.

**If you want to switch to JSON**, I can:
1. Add a JSON library (nlohmann/json is recommended)
2. Migrate the settings system to JSON
3. Add backward compatibility to convert old INI files
4. Update all documentation

**What would you prefer?**
- Keep INI (current implementation - works great!)
- Switch to JSON (better long-term, requires adding a library)
