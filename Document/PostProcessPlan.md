# Post-Processing Integration Plan: (Viewport -> PostProcess) -> ImGui -> DX

## Overview
The goal is to move post-processing execution from a global "end-of-frame" phase to a "per-viewport" phase. This ensures that effects like DepthScene or FXAA are applied to each viewport individually before they are composited by ImGui.

## 1. Pipeline Modification
The primary change occurs in `FEditorRenderPipeline::RenderViewport`.

**Current Flow:**
1. `RenderViewport` calls `Renderer.Render(Bus)`.
2. Scene is rendered to `ViewportColorTargets[0]`.

**Proposed Flow:**
1. `RenderViewport` calls `Renderer.Render(Bus)`.
2. **Execute Post-Process Stack**:
    - Iterate `Renderer.PostProcessArray`.
    - If `Effect->IsEnabled(Bus)` is true:
        - `Source = Device.GetPostProcessSourceSRV()`
        - `Dest = Device.GetPostProcessDestRTV()`
        - `Effect->Execute(..., Source, Dest)`
        - `Device.SwapPostProcessTargets()`
3. **Result**: The final processed image for that viewport is stored in `ViewportColorTargets[ActiveIndex]`.

## 2. Preserving PostProcessBase (IPostProcess)
The current `IPostProcess` interface is already sufficient:
```cpp
virtual void Execute(..., const FRenderBus& Bus, ..., SceneColorSRV, OutputRTV)
```
Because the `Bus` is passed into `Execute`, each post-process effect can access viewport-specific data (Near/Far planes, ViewMode, etc.) even when running inside the viewport loop. No changes to the interface are required.

## 3. UI Integration (FEditorMainPanel)
Currently, `FEditorMainPanel::RenderViewportHostWindow` hardcodes the viewport texture:
```cpp
// Existing
ID3D11ShaderResourceView* SceneColorSRV = Device.GetViewportSceneColorSRV(); // Always index 0
```
This must be changed to:
```cpp
// Proposed
ID3D11ShaderResourceView* SceneColorSRV = Device.GetFinalColorSRV(); // Returns ActiveViewportColorIndex
```

## 4. Key Advantages
- **No Enormous Refactoring**: Uses existing `FD3DDevice` ping-pong logic and `Renderer` stack.
- **Independence**: Allows different viewports to have different active post-processes.
- **Performance**: Post-processing is restricted to the viewport's `SubViewport` region if implemented correctly in shaders.

## 5. Implementation Steps
1. In `D3DDevice.cpp`, ensure `ResetPostProcessTargets()` is called at the start of `BeginFrame`.
2. In `EditorRenderPipeline.cpp`, call a new helper function `Renderer.ExecutePostProcessStack(Context, Bus)` at the end of `RenderViewport`.
3. Update `MainPanel.cpp` to use the dynamic `GetFinalColorSRV()`.
