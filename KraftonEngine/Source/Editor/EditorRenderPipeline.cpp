#include "EditorRenderPipeline.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Selection/PickingTypes.h"
#include "Editor/Selection/PickingPerf.h"
#include "Render/Pipeline/Renderer.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer)
	: Editor(InEditor)
{
}

FEditorRenderPipeline::~FEditorRenderPipeline()
{
}

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
	FStatManager::Get().TakeSnapshot();
	FGPUProfiler::Get().TakeSnapshot();
	FGPUProfiler::Get().BeginFrame();
#endif

	// 뷰포트별 오프스크린 렌더 (각 VP의 RT에 3D 씬 렌더)
	for (FLevelEditorViewportClient* VC : Editor->GetLevelViewportClients())
	{
		RenderViewport(VC, Renderer);
	}

	// 스왑체인 백버퍼 복귀 → ImGui 합성 → Present
	Renderer.BeginFrame();
	Editor->RenderUI(DeltaTime);

#if STATS
	FGPUProfiler::Get().EndFrame();
#endif
	Renderer.EndFrame();
}

void FEditorRenderPipeline::RenderViewport(FLevelEditorViewportClient* VC, FRenderer& Renderer)
{
	UCameraComponent* Camera = VC->GetCamera();
	if (!Camera) return;

	FViewport* VP = VC->GetViewport();
	if (!VP) return;

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Ctx) return;

	UWorld* World = Editor->GetWorld();
	if (!World) return;

	// 뷰포트별 렌더 옵션 사용
	const FViewportRenderOptions& Opts = VC->GetRenderOptions();
	const FShowFlags& ShowFlags = Opts.ShowFlags;
	EViewMode ViewMode = Opts.ViewMode;

	// 지연 리사이즈 적용 + 오프스크린 RT 바인딩
	if (VP->ApplyPendingResize())
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
		VC->InvalidateIdBufferCache();
	}

	// 렌더 시작 (RT 클리어 + DSV 바인딩)
	VP->BeginRender(Ctx);

	// 1. ViewContext 수집
	ViewContext.Clear();

	ViewContext.SetCameraInfo(Camera);
	ViewContext.SetRenderOptions(Opts);
	ViewContext.SetViewportInfo(VP);

	// 2. RenderCommand(DefaultPass), Entry(Batcher)를 ERenderPass별로 수집
	const TArray<AActor*>& SelectedActors = Editor->GetSelectionManager().GetSelectedActors();

	if (UScene* Scene = World->GetPersistentScene())
	{
		Scene->GetRenderProxy().CollectWorld(ViewContext, SelectedActors, false);
	}
	if (UScene* Scene = World->GetActiveScene())
	{
		Scene->GetRenderProxy().CollectWorld(ViewContext, SelectedActors, true);
	}

	ViewContext.CollectViewElements();

	// 3. Bus에 담긴 커맨드와 엔트리를 기반으로 렌더러에 배치
	Renderer.PrepareBatchers(ViewContext);

	// 4. Bus에 담긴 커맨드와 엔트리를 기반으로 GPU 드로우 콜 실행
	Renderer.Render(ViewContext);

	if (Editor->GetPickingMode() == EPickingMode::IDBuffer && VC->HasPendingIdPickRequest())
	{
		const uint64 IdPickStartCycles = FPickingPlatformTime::Cycles64();
		if (VC->ShouldRenderPendingIdPick())
		{
			Renderer.RenderPicking(ViewContext, VP);
			VC->UpdateIdBufferCacheCameraState();
		}

		uint32 PickX = 0;
		uint32 PickY = 0;
		VC->GetPendingIdPickCoord(PickX, PickY);

		uint32 PickedId = 0;
		if (VP->ReadPickingId(Ctx, PickX, PickY, PickedId))
		{
			VC->SetIdPickResult(PickedId);
		}
		else
		{
			VC->SetIdPickResult(0u);
		}

		const uint64 IdPickEndCycles = FPickingPlatformTime::Cycles64();
		FPickingPerf::Record(EPickingMode::IDBuffer, IdPickEndCycles - IdPickStartCycles);
	}
}

void FEditorRenderPipeline::Reset()
{
	ViewContext.Reset();
}
