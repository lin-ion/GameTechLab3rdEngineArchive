#pragma once

#include <memory>
#include "Editor/Input/EditorViewportModes.h"

class FEditorViewportClient;

class FEditorViewportController
{
public:
	explicit FEditorViewportController(FEditorViewportClient* InOwner);
	bool SetMode(EEditorViewportModeType InModeType);
	EEditorViewportModeType GetMode() const;
	bool CycleMode();

	bool HandleCommandInput(float DeltaTime);
	bool HandleGizmoInput(float DeltaTime);
	bool HandleSelectionInput(float DeltaTime);
	bool HandleNavigationInput(float DeltaTime);
	bool IsNavigationInputActiveNow() const;
	bool HasPendingIdPickRequest() const;
	void GetPendingIdPickCoord(uint32& OutX, uint32& OutY) const;
	bool HasPendingIdPickReadback() const;
	uint32 GetPendingIdPickReadbackRequestId() const;
	void BeginPendingIdPickReadback(uint32 InRequestId);
	void CancelPendingIdPickReadback();
	void SetIdPickResult(uint32 InId);
	void ResetIdPickingState();
	void ResetInputState();

private:
	FEditorViewportClient* Owner = nullptr;
	std::unique_ptr<IEditorViewportMode> ActiveMode;
	std::unique_ptr<IEditorViewportTool> NavigationTool;
};
