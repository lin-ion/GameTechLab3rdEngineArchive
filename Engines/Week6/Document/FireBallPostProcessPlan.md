# FireBall Post-Process Implementation Plan

This document outlines the plan for implementing the FireBall effect using the post-processing pipeline in NipsEngine.

## 1. Overview
The FireBall effect is a screen-space post-processing effect that visualizes fireballs based on data stored in `UFireBallComponent`. The effect will be modularly implemented as an `IPostProcess` pass.

## 2. Core Components

### 2.1. Data Structures
- **`FFireBallPostProcessData`** (in `PostProcessTypes.h`):
  Stores world position, radius, falloff, and intensity for a single fireball.
- **`FFireBallConstants`** (in `Render/Resource/RenderResources.h`):
  Constant buffer structure for the FireBall shader (CB Slot 10).

### 2.2. Collection Logic
- **`FRenderBus`**: Add `TArray<FFireBallPostProcessData> FireBallDataArray` to store collected fireballs.
- **`FRenderCollector::CollectFromComponent`**:
  - Add case for `EPrimitiveType::EPT_FireBall`.
  - Extract `Intensity`, `Radius`, `RadiusFallOff`, and `WorldLocation` from `UFireBallComponent`.
  - Add the data to `RenderBus.FireBallDataArray`.

## 3. Post-Process Integration

### 3.1. `FFireBallPostProcess` Class
- Inherits from `IPostProcess`.
- **`IsEnabled`**: Returns `true` if `ViewDesc.FireBalls` is not empty.
- **`Execute`**:
  - Bind `Resources.FireBallShader`.
  - Update `Resources.FireBallConstantBuffer` for each fireball (or as an array).
  - Draw a full-screen triangle or a proxy geometry for each fireball.
  - Ensure ping-pong buffers are swapped correctly.

### 3.2. `FRenderer` Changes
- Implement `ExecutePostProcessStack` (if not already implemented) to iterate through the `PostProcessStack`.
- Populate `FPostProcessViewDesc.FireBalls` from `FRenderBus.FireBallDataArray` before executing the stack.
- Register `FFireBallPostProcess` in `FRenderer::Create`.

## 4. Shader Implementation (`Shaders/FireBall.hlsl`)
- **Input**: Scene Color, Depth Texture, FireBall parameters.
- **Logic**:
  1. Reconstruct world position from depth.
  2. Calculate distance from fireball center.
  3. Apply radial intensity with falloff.
  4. Blend the effect onto the scene color.

## 5. Implementation Steps
1. **Infrastructure Update**:
   - Add `FireBallDataArray` to `FRenderBus`.
   - Add `FireBalls` to `FPostProcessViewDesc`.
2. **Collector Implementation**: Update `FRenderCollector.cpp` to populate `FireBallDataArray`.
3. **Resource Preparation**: Add `FireBallShader` and `FireBallConstantBuffer` to `FRenderResources`.
4. **Effect Logic**: Implement `FFireBallPostProcess.h/cpp`.
5. **Shader Authoring**: Create `FireBall.hlsl`.
6. **Pipeline Integration**: Implement/Update `ExecutePostProcessStack` in `FRenderer.cpp` and `EditorRenderPipeline.cpp`.

## 6. Future Considerations
- Optimization: Use a tiled renderer or compute shader if the number of fireballs becomes large.
- Visuals: Add heat distortion and animated noise textures to the fireball effect.
