#pragma once

// Define FViewportCameraTransform first, before it's used in FEditorViewportClient
struct FViewportCameraTransform
{
protected:
	FVector ViewLocation;

	// TODO: Use FRotator instead of FVector
	FVector ViewRotation;

	// TODO: Implement Orbit
	//FVector LookAt;

	// TODO: Implement Orthogonal Zoom
	//float OrthoZoom;

public:
	const FVector& GetLocation() const { return ViewLocation; }
	void SetLocation(const FVector& InLocation) { ViewLocation = InLocation; }
	const FVector& GetRotation() const { return ViewRotation; }
	void SetRotation(const FVector& InRotation) { ViewRotation = InRotation; }
	// TODO: Implement LookAt getters/setters
	// TODO: OrthoZoom getters/setters

	FVector GetRightVector() const;
	FVector GetUpVector() const;
	FVector GetForwardVector() const;
};

class FEditorViewportClient
{
public:
	FEditorViewportClient() = default;

public:
	float AspectRatio = 1.0f; // width / height;
	float FOVAngle = 60.0f; // horizontal field of view

protected:
	// TODO: Implement Orbit
	float FarPlane = 1000.0f;
	float NearPlane = 0.1f;
	float bIsPerspective = true;
	FViewportCameraTransform ViewTransform = { } ;

	POINT PreviousMousePosition = { 0, 0 };

public:
	FMatrix GetViewMatrix() const; // WorldToView
	FMatrix GetProjectionMatrix() const; // ViewToClip

	const FVector& GetViewLocation() const { return ViewTransform.GetLocation(); }
	void SetViewLocation(const FVector& NewLocation) { ViewTransform.SetLocation(NewLocation); }
	const FVector& GetViewRotation() const { return ViewTransform.GetRotation(); }
	void SetViewRotation(const FVector& NewRotation) { ViewTransform.SetRotation(NewRotation); }

	// UE에서는 Orthographic과 Perspective를 구분
	FViewportCameraTransform& GetViewTransform() { return ViewTransform; }
	const FViewportCameraTransform& GetViewTransform() const { return ViewTransform; }

	float GetFOV() const { return FOVAngle; }
	void SetFOV(float InFOV) { FOVAngle = InFOV; }
	float GetAspectRatio() const { return AspectRatio; }
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }
	float GetNearPlane() const { return NearPlane; }
	void SetNearPlane(float InNearPlane) { NearPlane = InNearPlane; }
	float GetFarPlane() const { return FarPlane; }
	void SetFarPlane(float InFarPlane) { FarPlane = InFarPlane; }


	void SetPerspective(bool bInIsPerspective) { bIsPerspective = bInIsPerspective; }
	const bool IsPerspective() const { return bIsPerspective; };
	FVector GetCameraRayDirection();

	// RayPicking 관련 함수로 추정
	//	virtual void ProcessClick ( FSceneView& View,
	//		HHitProxy* HitProxy,
	//			FKey Key,
	//			EInputEvent Event,
	//			uint32 HitX,
	//			uint32 HitY
	//			)
	//

	// void SetCameraSetup ( const FVector& LocationForOrbiting,
	// const FRotator& InOrbitRotation,
	// 	 const FVector& InOrbitZoom,
	// 	 const FVector& InOrbitLookAt,
	// 	 const FVector& InViewLocation,
	// 	 const FRotator& InViewRotation
	// )

	// UE 문서 참고 필요
	// void SetOrthoZoom( float InOrthoZoom )

	void Tick(float DeltaTime);
};
