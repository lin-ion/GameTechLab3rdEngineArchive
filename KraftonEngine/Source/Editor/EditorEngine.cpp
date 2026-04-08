#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/LevelSaveManager.h"
#include "GameFramework/World.h"
#include "GameFramework/Level.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Profiling/Stats.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"

IMPLEMENT_CLASS(UEditorEngine, UEngine)

FWorldContext* UEditorEngine::GetEditorWorldContext()
{
	for (auto& Ctx : WorldList)
	{
		if (Ctx.WorldType == EWorldType::Editor)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

void UEditorEngine::Init(FWindowsWindow* InWindow)
{
	// 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
	UEngine::Init(InWindow);
	
	FObjManager::ScanMeshAssets();
	FObjManager::ScanMaterialAssets();

	// 에디터 전용 초기화
	FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());

	MainPanel.Create(Window, Renderer, this);

	// World
	if (WorldList.empty())
	{
		CreateWorldContext(EWorldType::Editor, FName("Default"));
	}
	SetActiveWorld(WorldList[0].ContextHandle);
	GetWorld()->InitWorld();

	// Selection & Gizmo
	SelectionManager.Init(GetWorld());

	// 뷰포트 레이아웃 초기화 + 저장된 설정 복원
	ViewportLayout.Initialize(this, Window, Renderer, &SelectionManager);
	ViewportLayout.LoadFromSettings();

	// Editor render pipeline
	SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));
}

void UEditorEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	ViewportLayout.SaveToSettings();
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
	CloseLevel();
	SelectionManager.Shutdown();
	MainPanel.Release();

	// 뷰포트 레이아웃 해제
	ViewportLayout.Release();

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	// 윈도우 리사이즈 시에는 ImGui 패널이 실제 크기를 결정하므로
	// FViewport RT는 SSplitter 레이아웃에서 지연 리사이즈로 처리됨
}

void UEditorEngine::Tick(float DeltaTime)
{
	MainPanel.Update();
	SetImGuiInputCapture(MainPanel.IsCapturingMouse(), MainPanel.IsCapturingKeyboard());

	ClearInputTargets();
	for (FLevelEditorViewportClient* VC : ViewportLayout.GetLevelViewportClients())
	{
		if (!VC)
		{
			continue;
		}

		FViewport* VP = VC->GetViewport();
		if (!VP)
		{
			continue;
		}

		RegisterInputTarget(
			VP,
			VC,
			EInputRouteDomain::Editor,
			[VC](FRect& OutRect)
			{
				const FRect& R = VC->GetViewportScreenRect();
				if (R.Width <= 0.0f || R.Height <= 0.0f)
				{
					return false;
				}
				OutRect = R;
				return true;
			});
	}

	DispatchInput();

	for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
	{
		VC->Tick(DeltaTime);
	}

	WorldTick(DeltaTime);
	Render(DeltaTime);
}

FViewportCamera* UEditorEngine::GetCamera() const
{
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		return ActiveVC->GetCamera();
	}
	return nullptr;
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	MainPanel.Render(DeltaTime);
}

void UEditorEngine::SetPickingMode(EPickingMode InMode)
{
	if (PickingMode == InMode)
	{
		return;
	}

	PickingMode = InMode;
	for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
	{
		if (VC)
		{
			VC->ResetIdPickingState();
		}
	}
}

// ─── 기존 메서드 ──────────────────────────────────────────

void UEditorEngine::ResetViewport()
{
	SelectionManager.SetWorld(GetWorld());
	ViewportLayout.ResetViewport(GetWorld());
}

void UEditorEngine::CloseLevel()
{
	ClearWorlds();
}

void UEditorEngine::NewLevel()
{
	ClearWorlds();
	FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewLevel"), "New Level");
	SetActiveWorld(Ctx.ContextHandle);

	ResetViewport();
}

void UEditorEngine::StartPIE()
{
	FWorldContext* Context = GetEditorWorldContext();
	FWorldContext PIEWorldContext = Context->Duplicate();
	PIEWorldContext.WorldType = EWorldType::PIE;
	
	WorldList.push_back(PIEWorldContext);
	SetActiveWorld(WorldList.back().ContextHandle);

	SelectionManager.SetWorld(GetWorld());

	// AActor::BeginPlay()
	PIEWorldContext.World->InitWorld();
	PIEWorldContext.World->BeginPlay();
	
	bPIEEnabled = true;
}

void UEditorEngine::EndPIE()
{
	UWorld* PIEWorld = GetWorld();
	// Get PIE world context
	FWorldContext* PIEContext = GetWorldContextFromWorld(PIEWorld);
	if (PIEContext && PIEContext->WorldType == EWorldType::PIE)
	{
		SelectionManager.ClearSelection();

		// Revert ActiveWorldContext to EditorWorldContext FIRST
		FWorldContext* EditorContext = GetEditorWorldContext();
		if (EditorContext)
		{
			SetActiveWorld(EditorContext->ContextHandle);
			// This will trigger Gizmo->SetWorld, which safely unregisters from PIEWorld while it's still alive
			SelectionManager.SetWorld(GetWorld());
		}

		PIEContext->World->EndPlay();
		auto WorldListIter = find_if(WorldList.begin(), WorldList.end(), 
			[PIEContext](const FWorldContext& a) 
			{
				return a.ContextHandle == PIEContext->ContextHandle;
			});
		if (WorldListIter != WorldList.end())
		{
			UObjectManager::Get().DestroyObject(PIEContext->World);
			WorldList.erase(WorldListIter);
		}
	}
	
	bPIEEnabled = false;
}

void UEditorEngine::ClearWorlds()
{
	FStatManager::Get().ResetStats();

	SelectionManager.ClearSelection();
	SelectionManager.SetWorld(nullptr);

	for (FWorldContext& Ctx : WorldList)
	{
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}

	WorldList.clear();
	ActiveWorldHandle = FName::None;

	// Reset rendering bus and name counters to free memory
	ResetRenderPipeline();
	UObjectManager::Get().ClearNameCounters();

	ViewportLayout.DestroyAllCameras();
}
