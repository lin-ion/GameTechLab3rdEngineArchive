#pragma once

#include "Object/Object.h"
#include "Core/PropertyTypes.h"

class AActor;

class UActorComponent : public UObject
{
public:
	DECLARE_CLASS(UActorComponent, UObject)
	
	virtual UActorComponent* Duplicate() override;
	virtual UActorComponent* DuplicateSubObjects() override { return this; }

	virtual void BeginPlay();
	virtual void EndPlay() {};

	virtual void Activate();
	virtual void Deactivate();

	void ExecuteTick(float DeltaTime);
	void SetActive(bool bNewActive);
	inline void SetAutoActivate(bool bNewAutoActivate) { bAutoActivate = bNewAutoActivate; }
	inline void SetComponentTickEnabled(bool bEnabled) { bCanEverTick = bEnabled; }

	inline bool IsActive() const { return bIsActive; }
	inline bool IsComponentTickEnabled() const { return bCanEverTick; }
	inline bool IsEditorOnly() const { return bIsEditorOnly; }
	inline void SetEditorOnly(bool bValue) { bIsEditorOnly = bValue; }

	void SetOwner(AActor* Actor) { Owner = Actor; }
	AActor* GetOwner() const { return Owner; }

	// 에디터에 노출할 프로퍼티 목록 반환. 하위 클래스에서 override하여 속성 추가.
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps);

	// 프로퍼티 값 변경 후 호출. 하위 클래스에서 override하여 부수효과(리소스 재로딩 등) 처리.
	virtual void PostEditProperty(const char* PropertyName) {}

	// bCreatedInEditorInstance Get Set
	bool CanRemoveFromInstance() const { return bCreatedInEditorInstance; }
	void SetCreatedInEditorInstance(bool InFlag) { bCreatedInEditorInstance = InFlag; }

protected:
	virtual void TickComponent(float DeltaTime) {}

protected:
	AActor* Owner = nullptr;
	bool bIsEditorOnly = false;

	// 해당 Component가 Editor에서 생성된건지, Spawn할때 이미 붙어있는건지 판별하는 함수
	// default를 fasle로 해서 EditorProperty에서 Add 할때만 true로 변경
	bool bCreatedInEditorInstance = false;

private:
	bool bIsActive = true;
	bool bAutoActivate = true;
	bool bCanEverTick = true;
};




