#pragma once

struct FViewportCameraTransform
{
public:
	FViewportCameraTransform(FVector inViewLocation, FVector inViewRotation, float inDistance)
		: ViewLocation(inViewLocation), ViewRotation(inViewRotation), Distance(inDistance), OrthoSize(1000.0f) {
	}

protected:
	FVector ViewLocation;
	// TODO: Use FRotator instead of FVector
	FVector ViewRotation;
	float Distance;
	float OrthoSize;

public:
	const FVector& GetLocation() const { return ViewLocation; }
	void SetLocation(const FVector& InLocation) { ViewLocation = InLocation; }
	const FVector& GetRotation() const { return ViewRotation; }
	void SetRotation(const FVector& InRotation) { ViewRotation = InRotation; }

	const float& GetDistance() const { return Distance; }
	void SetDistance(const float& InDistance) { Distance = InDistance; }
	const FVector& GetPivotLocation() const { return ViewLocation + GetForwardVector() * Distance; }
	const float& GetOrthoSize() const { return OrthoSize; }
	void SetOrthoSize(const float& InOrthoSize) { OrthoSize = InOrthoSize; }

	FVector GetRightVector() const;
	FVector GetUpVector() const;
	FVector GetForwardVector() const;
};

class FEditorViewportClient
{
public:
	FEditorViewportClient(FVector inViewLocation, FVector inViewRotation, float inAspectRatio, float FOVAngle)
		: ViewTransform(inViewLocation, inViewRotation, inViewLocation.Length()), AspectRatio(inAspectRatio), FOVAngle(FOVAngle)
	{
		float InitialDistance = ViewTransform.GetLocation().Length(); // same as InViewLocation.Length()
		float HalfFOVRadian = Math::ToRadians(FOVAngle) / 2.0f;
		float InitialOrthoSize = 2.0f * InitialDistance * tanf(HalfFOVRadian);

		ViewTransform.SetOrthoSize(InitialOrthoSize);
	}

public:
	float AspectRatio = 1.0f; // AspectRatio = width / height
	float FOVAngle = 60.0f; // horizontal field of view

protected:
	float FarPlane = 100.0f;
	float NearPlane = 0.1f;
	float bIsPerspective = true;
	FViewportCameraTransform ViewTransform;

public:
	FMatrix GetViewMatrix() const; // WorldToView
	FMatrix GetProjectionMatrix() const; // ViewToClip

	const FVector& GetViewLocation() const { return ViewTransform.GetLocation(); }
	void SetViewLocation(const FVector& NewLocation) { ViewTransform.SetLocation(NewLocation); }
	const FVector& GetViewRotation() const { return ViewTransform.GetRotation(); }
	void SetViewRotation(const FVector& NewRotation) { ViewTransform.SetRotation(NewRotation); }

	FViewportCameraTransform& GetViewTransform() { return ViewTransform; }
	float GetFOVAngle() const { return FOVAngle; }
	void SetFOVAngle(float InFOVAngle) { FOVAngle = InFOVAngle; }
	float GetAspectRatio() const { return AspectRatio; }
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }
	float GetNearPlane() const { return NearPlane; }
	void SetNearPlane(float InNearPlane) { NearPlane = InNearPlane; }
	float GetFarPlane() const { return FarPlane; }
	void SetFarPlane(float InFarPlane) { FarPlane = InFarPlane; }
	void SetPerspective(bool bInIsPerspective);
	const bool IsPerspective() const { return bIsPerspective; };
	FVector GetCameraRayDirection();
	void Tick(float DeltaTime);

protected:
	void HandleKeyboardMovement(float DeltaTime);
	void HandleMouseRightDrag();
	void HandleMouseWheel();
};
