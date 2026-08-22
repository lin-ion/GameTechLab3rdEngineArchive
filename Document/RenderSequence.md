sequenceDiagram
    participant GameLoop as FEngineLoop
    participant Engine as UEditorEngine
    participant Pipeline as FEditorRenderPipeline
    participant Collector as FRenderCollector
    participant Bus as FRenderBus
    participant Renderer as FRenderer
    participant Device as FD3DDevice

    GameLoop->>Engine: Tick(DeltaTime)
    activate Engine

    Engine->>Engine: Update Logic and Physics

    Engine->>Pipeline: Execute(DeltaTime, Renderer)
    activate Pipeline

    Pipeline->>Renderer: BeginFrame()
    Renderer->>Device: Clear BackBuffer and DSV

    Note over Pipeline, Renderer: For each Viewport

    Pipeline->>Bus: Clear()

    Pipeline->>Collector: CollectWorld(World, Bus, Frustum)
    activate Collector
    Collector->>Collector: Culling and BVH Traversal
    Collector->>Bus: AddCommand(Pass, Command)
    deactivate Collector

    Pipeline->>Renderer: PrepareBatchers(Bus)
    Note right of Renderer: Fill CPU buffers

    Pipeline->>Renderer: Render(Bus)
    activate Renderer

    Renderer->>Renderer: Update Frame Constant Buffer

    loop Each Render Pass
        Renderer->>Device: Set States (Depth, Blend, Rasterizer)
        Renderer->>Renderer: Apply Pass Render State

        loop Each Render Command
            Renderer->>Device: Bind Shaders and CBs
            Renderer->>Device: Bind Vertex and Index Buffers
            Renderer->>Device: DrawIndexed()
        end

        opt If Batcher exists
            Renderer->>Renderer: Flush Batcher
            Renderer->>Device: Draw()
        end
    end

    deactivate Renderer

    Note over Pipeline, Renderer: End Viewport Loop

    Pipeline->>Engine: RenderUI(DeltaTime)
    Engine->>Renderer: ImGui Draw Calls

    Pipeline->>Renderer: EndFrame()
    Renderer->>Device: Present()

    deactivate Pipeline
    deactivate Engine