#include "Editor/Selection/SelectionManager.h"
#include "Object/Object.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/Scene.h"
#include "GameFramework/AActor.h"
#include "Render/Pipeline/WorldRenderProxy.h"

void FSelectionManager::Init(UWorld* InWorld)
{
	Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	Gizmo->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	Gizmo->Deactivate();

	if (InWorld)
	{
		Gizmo->SetExplicitWorld(InWorld);
		// 명시적으로 OnRegister를 호출하여 Proxy를 생성
		Gizmo->OnRegister();

		// World의 PersistentScene에 Gizmo의 Proxy를 수동 등록
		if (UScene* PersistentScene = InWorld->GetPersistentScene())
		{
			PersistentScene->GetRenderProxy().AddProxy(Gizmo->GetProxy());
		}
	}
}

void FSelectionManager::Shutdown()
{
	ClearSelection();

	if (Gizmo)
	{
		UObjectManager::Get().DestroyObject(Gizmo);
		Gizmo = nullptr;
	}
}

void FSelectionManager::SetWorld(UWorld* InWorld)
{
	if (!Gizmo)
	{
		return;
	}

	SelectedActors.clear();
	Gizmo->Deactivate();
	Gizmo->SetExplicitWorld(InWorld);

	if (!InWorld)
	{
		return;
	}

	if (!Gizmo->GetProxy())
	{
		Gizmo->OnRegister();
	}

	if (UScene* PersistentScene = InWorld->GetPersistentScene())
	{
		PersistentScene->GetRenderProxy().AddProxy(Gizmo->GetProxy());
	}
}

void FSelectionManager::Select(AActor* Actor)
{
	SelectedActors.clear();
	if (Actor)
	{
		SelectedActors.push_back(Actor);
	}
	SyncGizmo();
}

void FSelectionManager::SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList)
{
	if (!ClickedActor) return;

	// Find index of clicked actor
	int32 ClickedIdx = -1;
	for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
	{
		if (ActorList[i] == ClickedActor) { ClickedIdx = i; break; }
	}
	if (ClickedIdx == -1) return;

	// Find nearest already-selected actor's index in ActorList
	int32 AnchorIdx = ClickedIdx;
	int32 MinDist = INT_MAX;
	for (AActor* Sel : SelectedActors)
	{
		for (int32 i = 0; i < static_cast<int32>(ActorList.size()); ++i)
		{
			if (ActorList[i] == Sel)
			{
				int32 Dist = std::abs(i - ClickedIdx);
				if (Dist < MinDist)
				{
					MinDist = Dist;
					AnchorIdx = i;
				}
				break;
			}
		}
	}

	// Replace selection with range [min, max]
	int32 Lo = std::min(AnchorIdx, ClickedIdx);
	int32 Hi = std::max(AnchorIdx, ClickedIdx);

	SelectedActors.clear();
	for (int32 i = Lo; i <= Hi; ++i)
	{
		if (ActorList[i])
		{
			SelectedActors.push_back(ActorList[i]);
		}
	}
	SyncGizmo();
}

void FSelectionManager::ToggleSelect(AActor* Actor)
{
	if (!Actor) return;

	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
	if (It != SelectedActors.end())
	{
		SelectedActors.erase(It);
	}
	else
	{
		SelectedActors.push_back(Actor);
	}
	SyncGizmo();
}

void FSelectionManager::Deselect(AActor* Actor)
{
	auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
	if (It != SelectedActors.end())
	{
		SelectedActors.erase(It);
	}
	SyncGizmo();
}

void FSelectionManager::ClearSelection()
{
	SelectedActors.clear();
	SyncGizmo();
}

void FSelectionManager::SyncGizmo()
{
	if (!Gizmo) return;

	// Scene Load/World 전환 이후에도 Gizmo 프록시가 현재 월드에 확실히 연결되도록 보장한다.
	if (UWorld* World = Gizmo->GetWorld())
	{
		if (!Gizmo->GetProxy())
		{
			Gizmo->OnRegister();
		}
		if (UScene* PersistentScene = World->GetPersistentScene())
		{
			PersistentScene->GetRenderProxy().AddProxy(Gizmo->GetProxy());
		}
	}

	AActor* Primary = GetPrimarySelection();
	if (Primary)
	{
		Gizmo->SetTarget(Primary);
		Gizmo->SetSelectedActors(&SelectedActors);
	}
	else
	{
		Gizmo->SetSelectedActors(nullptr);
		Gizmo->Deactivate();
	}
}
