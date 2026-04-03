#pragma once

#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Render/Pipeline/WorldRenderProxy.h"
#include <memory>

// === Forward Declaration
class UCameraComponent;
class UScene;

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

	// Active Camera — EditorViewportClient 또는 PlayerController가 세팅
	void SetActiveCamera(UCameraComponent* InCamera) { ActiveCamera = InCamera; }
	UCameraComponent* GetActiveCamera() const { return ActiveCamera; }

	UScene* GetActiveScene() const { return ActiveScene; }
	UScene* GetPersistentScene() const { return PersistentScene; }

private:
	UScene* ActiveScene = nullptr;
	UScene* PersistentScene = nullptr;

	UCameraComponent* ActiveCamera = nullptr;
	bool bHasBegunPlay = false;
};

#include "GameFramework/Scene.h"

template<typename T>
inline T* UWorld::SpawnActor()
{
	// create and register an actor
	T* Actor = UObjectManager::Get().CreateObject<T>();
	Actor->SetWorld(this);
	
	// Default to ActiveScene
	ActiveScene->AddActor(Actor);

	return Actor;
}
