#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Math/Vector.h"

class FEditorControlWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;
private:
	FVector CurSpawnPoint = { 0.f, 0.f, 0.f };
};
