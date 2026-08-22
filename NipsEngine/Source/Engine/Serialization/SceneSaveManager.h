#pragma once

#include <string>
#include <filesystem>
#include "Core/CoreMinimal.h"
#include "Core/Paths.h"
#include "GameFramework/WorldContext.h"

// Forward declarations
class UObject;
class UWorld;
class AActor;
class UActorComponent;
class USceneComponent;

namespace json {
	class JSON;
}

struct FPropertyDescriptor;

using std::string;

// Perspective 카메라 상태 — 씬 파일에 저장/복원되는 에디터 전용 데이터
struct FEditorCameraState
{
	FVector  Location = FVector::ZeroVector;
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);  // Pitch, Yaw, Roll (degrees)
	float    FOV      = 60.0f;                     // degrees
	float    NearClip = 0.1f;
	float    FarClip  = 1000.0f;
	bool     bValid   = false;
};

class FSceneSaveManager {
public:
	static constexpr const wchar_t* SceneExtension = L".Scene";
	static constexpr int32          CurrentVersion  = 4;

	static std::wstring GetSceneDirectory() { return FPaths::SceneDir(); }
	static TArray<FString> GetSceneFileList();

	// CameraState == nullptr이면 카메라 섹션 생략 (게임/PIE 호환)
	static void SaveSceneAsJSON(const string& SceneName, FWorldContext& WorldContext,
	                            const FEditorCameraState* CameraState = nullptr);
	static void LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext,
	                              FEditorCameraState* OutCameraState = nullptr);

private:
	// ---- Serialization ----
	static json::JSON SerializeActors(UWorld* World);
	static json::JSON SerializeActor(AActor* Actor);
	static json::JSON SerializeSceneComponentTree(USceneComponent* Comp);
	static json::JSON SerializeProperties(UActorComponent* Comp);
	static json::JSON SerializePropertyValue(const FPropertyDescriptor& Prop);
	static json::JSON SerializeCameraState(const FEditorCameraState* CameraState);

	// ---- Deserialization ----
	static void DeserializeActors(json::JSON& ActorsNode, UWorld* World);
	static void DeserializeActor(json::JSON& ActorNode, UWorld* World);
	static USceneComponent* DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner);
	static void DeserializeNonSceneComponents(json::JSON& Node, AActor* Owner);
	static void ApplyProperties(UActorComponent* Comp, json::JSON& PropsJSON);
	static void ApplyPropertyValue(FPropertyDescriptor& Prop, json::JSON& Value);
	static void DeserializeCameraState(json::JSON& Root, FEditorCameraState* OutCameraState);

	static string GetCurrentTimeStamp();
};
