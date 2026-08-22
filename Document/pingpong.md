# Implementing Post-Process Ping-Pong Mechanism

This document outlines how to implement a ping-pong rendering structure in NipsEngine to support multi-pass post-processing effects (e.g., Gaussian Blur, Bloom, SSAO).

---

## 1. Current Limitations
In the current `FRenderer`, post-processing (like `DrawPostProcessOutline`) is performed directly on the `SceneColorRTV`. This "in-place" modification is limited because:
- You cannot read from the same texture you are currently writing to (DirectX 11 restriction).
- Complex effects require the output of one pass to be the input of the next.

---

## 2. Proposed Ping-Pong Structure

### A. Resource Modification (`FD3DDevice`)
We need to add a second "Ping-Pong" buffer to `FD3DDevice` that matches the size and format of the `ViewportSceneColorTexture`.

```cpp
// Add to FD3DDevice.h
TComPtr<ID3D11Texture2D> ViewportPingPongTexture;
TComPtr<ID3D11RenderTargetView> ViewportPingPongRTV;
TComPtr<ID3D11ShaderResourceView> ViewportPingPongSRV;

// Add to FRenderTargetSet
ID3D11RenderTargetView* PingPongRTV = nullptr;
ID3D11ShaderResourceView* PingPongSRV = nullptr;
```

### B. The Ping-Pong Logic (`FRenderer`)
Implement a helper class or logic within `FRenderer` to manage the swapping of buffers.

```cpp
struct FPingPongBuffer {
    ID3D11RenderTargetView* RTVs[2];
    ID3D11ShaderResourceView* SRVs[2];
    int32 SourceIndex = 0;

    void Swap() { SourceIndex = 1 - SourceIndex; }
    ID3D11RenderTargetView* GetTarget() const { return RTVs[1 - SourceIndex]; }
    ID3D11ShaderResourceView* GetSource() const { return SRVs[SourceIndex]; }
};
```

---

## 3. Implementation Workflow

### Step 1: Initialize Buffers
In `FD3DDevice::CreateViewportRenderTargets`, create the `ViewportPingPongTexture` with the same dimensions as the scene color texture.

### Step 2: Collection Stage
Add a new `ERenderPass::PostProcessChain` to the `ERenderPass` enum. This pass will contain multiple `FRenderCommand`s for each effect in the chain.

### Step 3: Execution Stage (`FRenderer::RenderPostProcess`)
Replace the single outline draw with a loop that swaps buffers:

```mermaid
graph LR
    Start[Scene Color Buffer] --> Pass1[Pass 1: Blur X]
    Pass1 -->|Write to| PingPong[Ping-Pong Buffer]
    PingPong -->|Read from| Pass2[Pass 2: Blur Y]
    Pass2 -->|Write to| SceneColor[Scene Color Buffer]
    SceneColor --> Final[Final Composition]
```

```cpp
void FRenderer::ExecutePostProcessChain(ID3D11DeviceContext* Context) {
    FPingPongBuffer PP;
    PP.RTVs[0] = CurrentRenderTargets.SceneColorRTV;
    PP.SRVs[0] = CurrentRenderTargets.SceneColorSRV;
    PP.RTVs[1] = CurrentRenderTargets.PingPongRTV;
    PP.SRVs[1] = CurrentRenderTargets.PingPongSRV;

    for (auto& Effect : PostProcessEffects) {
        // 1. Set Target
        ID3D11RenderTargetView* Target = PP.GetTarget();
        Context->OMSetRenderTargets(1, &Target, nullptr);

        // 2. Bind Source as Texture
        ID3D11ShaderResourceView* Source = PP.GetSource();
        Context->PSSetShaderResources(0, 1, &Source);

        // 3. Draw Fullscreen Triangle
        Effect.Shader->Bind(Context);
        Context->Draw(3, 0);

        // 4. Swap for next pass
        PP.Swap();
    }
}
```

---

## 4. Key Considerations
1.  **Fullscreen Triangle**: Use a vertex shader that generates a fullscreen triangle from `SV_VertexID` to avoid needing a vertex buffer.
2.  **Depth/Stencil**: Post-process passes usually disable depth testing (`DepthEnable = FALSE`).
3.  **Sampler State**: Use a `Point` or `Linear` sampler depending on whether you need exact pixel mapping or filtered results.
4.  **Final Output**: Ensure the last pass writes back to the buffer that the UI/Editor expects (usually `SceneColorRTV`).
