#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/CoreTypes.h"

class FEditorLevelWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	void RefreshLevelFileList();

	char LevelName[128] = "Default";

	TArray<FString> LevelFiles;
	int32 SelectedLevelIndex = -1;

	float NewLevelNotificationTimer = 0.f;
	float LevelSaveNotificationTimer = 0.f;
	float LevelLoadNotificationTimer = 0.f;
};
