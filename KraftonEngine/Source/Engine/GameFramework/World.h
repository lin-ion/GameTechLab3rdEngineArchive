#pragma once

#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Render/Pipeline/WorldRenderProxy.h"
#include <memory>

// === Forward Declaration
class FViewportCamera;
class ULevel;

class UWorld : public UObject {
public:
	DECLARE_CLASS(UWorld, UObject)
	UWorld();
	~UWorld() override;

	// Actor lifecycle
	template<typename T>
	T* SpawnActor();
	void DestroyActor(AActor* Actor);

	const TArray<AActor*>& GetActors() const;

	void InitWorld();      // Set up the world before gameplay begins
	void BeginPlay();      // Triggers BeginPlay on all actors
	void Tick(float DeltaTime);  // Drives the game loop every frame
	void EndPlay();        // Cleanup before world is destroyed

	bool HasBegunPlay() const { return bHasBegunPlay; }

	// Active Camera
	void SetActiveCamera(FViewportCamera* InCamera) { ActiveCamera = InCamera; }
	FViewportCamera* GetActiveCamera() const { return ActiveCamera; }

	ULevel* GetActiveLevel() const { return ActiveLevel; }
	ULevel* GetPersistentLevel() const { return PersistentLevel; }

private:
	ULevel* ActiveLevel = nullptr;
	ULevel* PersistentLevel = nullptr;

	FViewportCamera* ActiveCamera = nullptr;
	bool bHasBegunPlay = false;
};

#include "GameFramework/Level.h"

template<typename T>
inline T* UWorld::SpawnActor()
{
	// create and register an actor
	T* Actor = UObjectManager::Get().CreateObject<T>();
	Actor->SetWorld(this);
	
	// Default to ActiveLevel
	ActiveLevel->AddActor(Actor);

	return Actor;
}
