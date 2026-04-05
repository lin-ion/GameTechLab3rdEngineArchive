#pragma once

#include "Core/CoreTypes.h"

class AActor;
class UGizmoComponent;

class FSelectionManager
{
public:
	void Init(class UWorld* InWorld);
	void Shutdown();
	void SetWorld(class UWorld* InWorld);

	void Select(AActor* Actor);
	void SelectRange(AActor* ClickedActor, const TArray<AActor*>& ActorList);
	void ToggleSelect(AActor* Actor);
	void Deselect(AActor* Actor);
	void ClearSelection();

	bool IsSelected(AActor* Actor) const
	{
		return std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
	}

	AActor* GetPrimarySelection() const;

	const TArray<AActor*>& GetSelectedActors() const { return SelectedActors; }
	bool IsEmpty() const { return SelectedActors.empty(); }

	UGizmoComponent* GetGizmo() const { return Gizmo; }

private:
	void SyncGizmo();

	TArray<AActor*> SelectedActors;
	AActor* PrimarySelection = nullptr;
	UGizmoComponent* Gizmo = nullptr;
};
