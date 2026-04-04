#pragma once

#include "GameFramework/AActor.h"

class UStaticMeshComponent;
class UTextRenderComponent;

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() = default;

	void InitDefaultComponents(const FString& UStaticMeshFileName);

private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
#ifndef FOR_COMPETITION
	 UTextRenderComponent* TextRenderComponent = nullptr;
#endif
};