#include "Editor/Input/EditorViewportCommandTool.h"

#include "Editor/Input/EditorViewportController.h"
#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/Input/EditorNavigationTool.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

FEditorViewportCommandTool::FEditorViewportCommandTool(FEditorViewportClient* InOwner, FEditorViewportController* InController)
	: Owner(InOwner), Controller(InController)
{
}

bool FEditorViewportCommandTool::HandleInput(float DeltaTime)
{
	(void)DeltaTime;
	if (!Owner || !Controller)
	{
		return false;
	}

	if (Owner->InputContext.bImGuiCapturedKeyboard)
	{
		return false;
	}

	static const TArray<int32> ViewportGlobalActionCandidates =
	{
		static_cast<int32>(EditorViewportInputMapping::EEditorViewportAction::CycleMode),
		static_cast<int32>(EditorViewportInputMapping::EEditorViewportAction::CycleGizmoMode),
		static_cast<int32>(EditorViewportInputMapping::EEditorViewportAction::ToggleGizmoCoordinateSpace),
		static_cast<int32>(EditorViewportInputMapping::EEditorViewportAction::FocusSelection),
		static_cast<int32>(EditorViewportInputMapping::EEditorViewportAction::DeleteSelection),
		static_cast<int32>(EditorViewportInputMapping::EEditorViewportAction::SelectAll)
	};

	int32 TriggeredCommandActionId = 0;
	const bool bHasCommandAction = InputBindingUtils::TryGetHighestPriorityTriggeredAction(
		Owner->InputContext,
		EditorViewportInputMapping::GetBindings(),
		ViewportGlobalActionCandidates,
		TriggeredCommandActionId);

	if (!bHasCommandAction)
	{
		return false;
	}

	switch (static_cast<EditorViewportInputMapping::EEditorViewportAction>(TriggeredCommandActionId))
	{
	case EditorViewportInputMapping::EEditorViewportAction::CycleMode:
		return Controller->CycleMode();
	case EditorViewportInputMapping::EEditorViewportAction::CycleGizmoMode:
		return Owner->TryCycleGizmoMode();
	case EditorViewportInputMapping::EEditorViewportAction::ToggleGizmoCoordinateSpace:
		return ToggleGizmoCoordinateSpace();
	case EditorViewportInputMapping::EEditorViewportAction::FocusSelection:
		return FocusPrimarySelection();
	case EditorViewportInputMapping::EEditorViewportAction::DeleteSelection:
		return DeleteSelectedActors();
	case EditorViewportInputMapping::EEditorViewportAction::SelectAll:
		return SelectAllActors();
	default:
		return false;
	}
}

bool FEditorViewportCommandTool::FocusPrimarySelection()
{
	if (!Owner || !Owner->Camera || !Owner->SelectionManager)
	{
		return false;
	}

	AActor* PrimarySelection = Owner->SelectionManager->GetPrimarySelection();
	if (!PrimarySelection)
	{
		return false;
	}

	const FVector Target = PrimarySelection->GetActorLocation();
	IEditorViewportTool* NavigationTool = Controller->GetNavigationTool();
	if (!NavigationTool)
	{
		return false;
	}

	FEditorNavigationTool* NavTool = static_cast<FEditorNavigationTool*>(NavigationTool);
	NavTool->FocusOnTarget(Target);
	return true;
}

bool FEditorViewportCommandTool::DeleteSelectedActors()
{
	if (!Owner || !Owner->SelectionManager)
	{
		return false;
	}

	const TArray<AActor*> SelectedActors = Owner->SelectionManager->GetSelectedActors();
	if (SelectedActors.empty())
	{
		return false;
	}

	for (AActor* Actor : SelectedActors)
	{
		if (!Actor)
		{
			continue;
		}

		if (UWorld* ActorWorld = Actor->GetWorld())
		{
			ActorWorld->DestroyActor(Actor);
		}
	}

	Owner->SelectionManager->ClearSelection();
	return true;
}

bool FEditorViewportCommandTool::ToggleGizmoCoordinateSpace()
{
	if (!Owner || !Owner->Gizmo)
	{
		return false;
	}

	Owner->Gizmo->ToggleCoordinateSpace();
	return true;
}

bool FEditorViewportCommandTool::SelectAllActors()
{
	if (!Owner || !Owner->World || !Owner->SelectionManager)
	{
		return false;
	}

	const TArray<AActor*>& Actors = Owner->World->GetActors();
	if (Actors.empty())
	{
		return false;
	}

	Owner->SelectionManager->ClearSelection();

	bool bSelectedAny = false;
	for (AActor* Actor : Actors)
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		Owner->SelectionManager->AddSelect(Actor);
		bSelectedAny = true;
	}

	return bSelectedAny;
}
