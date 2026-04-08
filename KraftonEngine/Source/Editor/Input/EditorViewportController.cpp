#include "Editor/Input/EditorViewportController.h"

#include "Editor/Viewport/EditorViewportClient.h"

namespace
{
std::unique_ptr<IEditorViewportMode> CreateMode(EEditorViewportModeType InModeType, FEditorViewportClient* InOwner)
{
	switch (InModeType)
	{
	case EEditorViewportModeType::Select:
	default:
		return std::make_unique<FEditorSelectMode>(InOwner);
	}
}

EEditorViewportModeType GetNextModeType(EEditorViewportModeType InCurrentModeType)
{
	switch (InCurrentModeType)
	{
	case EEditorViewportModeType::Select:
	default:
		return EEditorViewportModeType::Select;
	}
}
}

FEditorViewportController::FEditorViewportController(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
	if (Owner)
	{
		ActiveMode = CreateMode(EEditorViewportModeType::Select, Owner);
		NavigationTool = std::make_unique<FEditorNavigationTool>(Owner);
	}
}

bool FEditorViewportController::SetMode(EEditorViewportModeType InModeType)
{
	if (!Owner)
	{
		return false;
	}

	if (ActiveMode && ActiveMode->GetType() == InModeType)
	{
		return true;
	}

	ActiveMode = CreateMode(InModeType, Owner);
	return ActiveMode != nullptr;
}

EEditorViewportModeType FEditorViewportController::GetMode() const
{
	if (!ActiveMode)
	{
		return EEditorViewportModeType::Select;
	}

	return ActiveMode->GetType();
}

bool FEditorViewportController::CycleMode()
{
	if (!ActiveMode)
	{
		return false;
	}

	const EEditorViewportModeType CurrentModeType = ActiveMode->GetType();
	const EEditorViewportModeType NextModeType = GetNextModeType(CurrentModeType);
	if (NextModeType == CurrentModeType)
	{
		return false;
	}

	return SetMode(NextModeType);
}

bool FEditorViewportController::HandleCommandInput(float DeltaTime)
{
	if (!Owner)
	{
		return false;
	}

	(void)DeltaTime;
	if (Owner->InputContext.bImGuiCapturedKeyboard)
	{
		return false;
	}

	if (Owner->InputContext.Frame.IsReleased(VK_TAB))
	{
		if (CycleMode())
		{
			return true;
		}
	}

	if (Owner->InputContext.Frame.IsReleased(VK_SPACE) && Owner->TryCycleGizmoMode())
	{
		return true;
	}

	return false;
}

bool FEditorViewportController::HandleGizmoInput(float DeltaTime)
{
	if (!Owner || !ActiveMode)
	{
		return false;
	}

	return ActiveMode->HandleGizmoInput(DeltaTime);
}

bool FEditorViewportController::HandleSelectionInput(float DeltaTime)
{
	if (!Owner || !ActiveMode)
	{
		return false;
	}

	return ActiveMode->HandleSelectionInput(DeltaTime);
}

bool FEditorViewportController::HandleNavigationInput(float DeltaTime)
{
	if (!Owner || !NavigationTool)
	{
		return false;
	}

	return NavigationTool->HandleInput(DeltaTime);
}

bool FEditorViewportController::IsNavigationInputActiveNow() const
{
	if (!NavigationTool)
	{
		return false;
	}

	const FEditorNavigationTool* NavTool = static_cast<const FEditorNavigationTool*>(NavigationTool.get());
	return NavTool->IsInputActiveNow();
}

bool FEditorViewportController::HasPendingIdPickRequest() const
{
	return ActiveMode ? ActiveMode->HasPendingIdPickRequest() : false;
}

void FEditorViewportController::GetPendingIdPickCoord(uint32& OutX, uint32& OutY) const
{
	OutX = 0u;
	OutY = 0u;
	if (ActiveMode)
	{
		ActiveMode->GetPendingIdPickCoord(OutX, OutY);
	}
}

bool FEditorViewportController::HasPendingIdPickReadback() const
{
	return ActiveMode ? ActiveMode->HasPendingIdPickReadback() : false;
}

uint32 FEditorViewportController::GetPendingIdPickReadbackRequestId() const
{
	return ActiveMode ? ActiveMode->GetPendingIdPickReadbackRequestId() : 0u;
}

void FEditorViewportController::BeginPendingIdPickReadback(uint32 InRequestId)
{
	if (ActiveMode)
	{
		ActiveMode->BeginPendingIdPickReadback(InRequestId);
	}
}

void FEditorViewportController::CancelPendingIdPickReadback()
{
	if (ActiveMode)
	{
		ActiveMode->CancelPendingIdPickReadback();
	}
}

void FEditorViewportController::SetIdPickResult(uint32 InId)
{
	if (ActiveMode)
	{
		ActiveMode->SetIdPickResult(InId);
	}
}

void FEditorViewportController::ResetIdPickingState()
{
	if (ActiveMode)
	{
		ActiveMode->ResetIdPickingState();
	}
}

void FEditorViewportController::ResetInputState()
{
	if (ActiveMode)
	{
		ActiveMode->ResetInputState();
	}
}
