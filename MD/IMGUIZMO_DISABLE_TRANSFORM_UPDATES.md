# ImGuizmo Disable Transform Updates

## Change Made
The gizmo is now **visual-only** - it appears when a model is selected but does **not** modify the model's transforms (Translate, Rotate, Scale) when manipulated.

## Reason
User requested to completely eliminate the gizmo's effect on model transforms. The gizmo should only serve as visual feedback, not functional manipulation.

## Implementation

### Modified File: `src/io/scene.cpp`

**Disabled the code block that updates transforms when gizmo is manipulated:**

```cpp
// DISABLED: Gizmo manipulation no longer affects model transforms
// The gizmo is now visual-only - it appears when a model is selected but does not modify transforms
// Users can still modify transforms manually through the PropertyPanel sliders
/*
if (manipulated) {
    // ... all the code that updates PropertyPanel or model transforms ...
}
*/
```

**What still works:**
- Gizmo appears when a model is selected
- Gizmo can be moved/rotated/scaled visually (for reference)
- Gizmo shows the current transform state
- Keyboard shortcuts (W/E/R for operation, +/- for size) still work

**What no longer works:**
- Gizmo manipulation does NOT update PropertyPanel transforms
- Gizmo manipulation does NOT update model.pos/rotation/size
- Model position is NOT affected by gizmo manipulation

**Alternative ways to modify transforms:**
- Use PropertyPanel sliders (Translate, Rotate, Scale)
- Modify values directly in the Properties panel
- Use "Reset Current Bone" or "Reset All Bones" buttons

## Behavior

1. **When a model is selected:**
   - Gizmo appears at the model's position
   - Gizmo shows current transform state
   - Gizmo can be manipulated visually (moved/rotated/scaled)

2. **When gizmo is manipulated:**
   - Gizmo visual updates (moves/rotates/scales)
   - **Model does NOT move** - transform remains unchanged
   - PropertyPanel values remain unchanged
   - Model stays at its original position

3. **To modify model transforms:**
   - Use PropertyPanel sliders
   - Manually enter values in the Properties panel
   - Use reset buttons to return to default values

## Technical Details

The gizmo rendering code remains intact:
- `ImGuizmo::Manipulate()` is still called
- Gizmo state (`mGizmoUsing`, `mGizmoOver`) is still updated
- Gizmo visual updates correctly
- Only the transform update logic is disabled

The disabled code would have:
1. Decomposed the manipulated matrix
2. Extracted translation, rotation, and scale
3. Updated PropertyPanel or model transforms
4. Caused the model to move/rotate/scale

Now this entire block is commented out, so the gizmo is purely visual.

## Files Modified

- `src/io/scene.cpp` - Disabled gizmo transform update logic (commented out the `if (manipulated)` block)
