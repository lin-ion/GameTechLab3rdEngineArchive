#pragma once
#include "ActorComponent.h"

class UMesh;

class UPickingComponent : public UActorComponent
{
public:
	UPickingComponent() = default;
	virtual ~UPickingComponent() = default;

public:
	bool IsPicked(const UMesh* MeshData, FMatrix Matrix);

private:
	bool RayIntersectsTriangle(const FVector& CameraPos, const FVector& CameraRay, const FVertexSimple& V0, const FVertexSimple& V1, const FVertexSimple& V2);
	bool RayIntersectsCircle();

};

