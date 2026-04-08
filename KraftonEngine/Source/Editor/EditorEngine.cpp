#include "Editor/EditorEngine.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/LevelSaveManager.h"
#include "Engine/Viewport/GameViewportClient.h"
#include "GameFramework/World.h"
#include "GameFramework/Level.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Profiling/Stats.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Viewport/Viewport.h"

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

	ResetViewport();
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
	InputTargetHosts.clear();

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
	PruneInputTargetHosts();

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

		if (FViewportClient* HostClient = ResolveInputTargetClient(VP, VC))
		{
			VP->SetClient(HostClient);
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

	SelectionManager.SetWorld(WorldList.back().World);

	// AActor::BeginPlay()
	PIEWorldContext.World->InitWorld();
	PIEWorldContext.World->BeginPlay();
	
	bPIEEnabled = true;

	// ViewportClient 전환
	SetActiveViewportSubClientForWorldType(EWorldType::PIE);
}

void UEditorEngine::EndPIE()
{
	UWorld* PIEWorld = GetWorld();
	// Get PIE world context
	FWorldContext* PIEContext = GetWorldContextFromWorld(PIEWorld);
	if (PIEContext && PIEContext->WorldType == EWorldType::PIE)
	{
		SelectionManager.ClearSelection();

		// 1. WorldContext를 에디터로 복구
		FWorldContext* EditorContext = GetEditorWorldContext();
		if (EditorContext)
		{
			SetActiveWorld(EditorContext->ContextHandle);
			// SelectionManager의 선택 대상을 에디터 월드로 복구
			SelectionManager.SetWorld(GetWorld());
		}

		// 2. ViewportClient 및 레이어 원상 복구
		SetActiveViewportSubClientForWorldType(EWorldType::Editor);

		// 3. PIE 월드 정리
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

FViewportClient* UEditorEngine::ResolveInputTargetClient(FViewport* InViewport, FViewportClient* InClient) const
{
	if (!InViewport || !InClient)
	{
		return nullptr;
	}

	auto Found = InputTargetHosts.find(InViewport);
	if (Found == InputTargetHosts.end())
	{
		Found = InputTargetHosts.emplace(InViewport, FViewportHostClient()).first;
	}

	FViewportHostClient& Host = Found->second;
	if (!Host.GetActiveSubClient())
	{
		Host.SetActiveSubClient(InClient);
	}
	return &Host;
}

void UEditorEngine::PruneInputTargetHosts()
{
	TSet<FViewport*> LiveViewports;
	for (FLevelEditorViewportClient* VC : ViewportLayout.GetLevelViewportClients())
	{
		if (VC && VC->GetViewport())
		{
			LiveViewports.insert(VC->GetViewport());
		}
	}

	for (auto It = InputTargetHosts.begin(); It != InputTargetHosts.end();)
	{
		if (LiveViewports.find(It->first) == LiveViewports.end())
		{
			It = InputTargetHosts.erase(It);
		}
		else
		{
			++It;
		}
	}
}

FLevelEditorViewportClient* UEditorEngine::FindLevelViewportClientByViewport(FViewport* InViewport) const
{
	if (!InViewport)
	{
		return nullptr;
	}

	for (FLevelEditorViewportClient* VC : ViewportLayout.GetLevelViewportClients())
	{
		if (VC && VC->GetViewport() == InViewport)
		{
			return VC;
		}
	}

	return nullptr;
}

bool UEditorEngine::SetViewportSubClient(FViewport* InViewport, FViewportClient* InSubClient)
{
	if (!InViewport || !InSubClient)
	{
		return false;
	}

	FViewportHostClient& Host = InputTargetHosts[InViewport];
	Host.SetActiveSubClient(InSubClient);
	InViewport->SetClient(&Host);
	return true;
}

bool UEditorEngine::ResetViewportSubClient(FViewport* InViewport)
{
	if (!InViewport)
	{
		return false;
	}

	FLevelEditorViewportClient* DefaultClient = FindLevelViewportClientByViewport(InViewport);
	if (!DefaultClient)
	{
		return false;
	}

	// 레이어 제거 및 월드 포인터 복구
	// NOTE: 이 시점에서 World가 이미 Reset된 상태혀야 함.
	auto Found = InputTargetHosts.find(InViewport);
	if (Found != InputTargetHosts.end())
	{
		Found->second.RemoveLayerClient(DefaultClient);
	}
	DefaultClient->SetWorld(GetWorld());

	return SetViewportSubClient(InViewport, DefaultClient);
}

bool UEditorEngine::SetViewportSubClientForWorldType(FViewport* InViewport, EWorldType InWorldType)
{
	if (!InViewport)
	{
		return false;
	}

	switch (InWorldType)
	{
	case EWorldType::Editor:
		return ResetViewportSubClient(InViewport);
	case EWorldType::PIE:
	{
		if (!PIEViewportClient)
		{
			PIEViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
		}
		PIEViewportClient->SetViewport(InViewport);

		// 기존 레벨 에디터 클라이언트를 ViewportHost의 레이어로 추가
		// => PIE 모드에서도 에디터 기능(기즈모, 선택 등) 유지
		FLevelEditorViewportClient* EditorVC = FindLevelViewportClientByViewport(InViewport);
		if (EditorVC)
		{
			EditorVC->SetWorld(GetWorld());
			
			FViewportHostClient& Host = InputTargetHosts[InViewport];
			Host.SetActiveSubClient(PIEViewportClient);
			Host.AddLayerClient(EditorVC);
			InViewport->SetClient(&Host);
			return true;
		}

		return SetViewportSubClient(InViewport, PIEViewportClient);
	}
	default:
		// Game specific client is not wired yet.
		return false;
	}
}

FViewportClient* UEditorEngine::GetViewportSubClient(FViewport* InViewport) const
{
	if (!InViewport)
	{
		return nullptr;
	}

	auto Found = InputTargetHosts.find(InViewport);
	if (Found != InputTargetHosts.end())
	{
		return Found->second.GetActiveSubClient();
	}

	if (FLevelEditorViewportClient* DefaultClient = FindLevelViewportClientByViewport(InViewport))
	{
		return DefaultClient;
	}

	return nullptr;
}

bool UEditorEngine::SetActiveViewportSubClient(FViewportClient* InSubClient)
{
	FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport();
	if (!ActiveVC)
	{
		return false;
	}

	return SetViewportSubClient(ActiveVC->GetViewport(), InSubClient);
}

bool UEditorEngine::ResetActiveViewportSubClient()
{
	FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport();
	if (!ActiveVC)
	{
		return false;
	}

	return ResetViewportSubClient(ActiveVC->GetViewport());
}

bool UEditorEngine::SetActiveViewportSubClientForWorldType(EWorldType InWorldType)
{
	FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport();
	if (!ActiveVC)
	{
		return false;
	}

	return SetViewportSubClientForWorldType(ActiveVC->GetViewport(), InWorldType);
}

FViewportClient* UEditorEngine::GetActiveViewportSubClient() const
{
	FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport();
	if (!ActiveVC)
	{
		return nullptr;
	}

	return GetViewportSubClient(ActiveVC->GetViewport());
}
