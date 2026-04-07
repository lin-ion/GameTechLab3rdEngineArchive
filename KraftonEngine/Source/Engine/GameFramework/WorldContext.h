#pragma once
#include "Core/CoreTypes.h"
#include "Object/FName.h"

class UWorld;

enum class EWorldType : uint32
{
    Editor,			// Editor mode — no BeginPlay
	EditorPreview,	// Actor preview mode - NOT IMPLEMENTED
    PIE,			// Play In Editor
    Game,			// Game mode — BeginPlay/Tick active
};

struct FWorldContext
{
    EWorldType WorldType = EWorldType::Editor;
    UWorld* World = nullptr;
    FString ContextName;
    FName ContextHandle;
};
