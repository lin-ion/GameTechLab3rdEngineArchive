#pragma once

/*
	Constants Buffer에 사용될 구조체와 
	에 담길 RenderCommand 구조체를 정의하고 있습니다.
	RenderCommand는 Renderer에서 Draw Call을 1회 수행하기 위해 필요한 정보를 담고 있습니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Device/D3DDevice.h"
#include "Core/CoreMinimal.h"
#include "Core/ResourceTypes.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"

constexpr uint32 MAX_FIREBALL_COUNT = 32;
constexpr uint32 MAX_FOG_COUNT = 8;

struct ID3D11ShaderResourceView;

enum class ERenderCommandType
{
	Primitive,
	Gizmo,
	SelectionMask,
	Billboard,
	DebugBox,	
	Grid,		// Grid 패스 — LineBatcher 경유
	Font,		// TextRenderComponent — FontBatcher 경유
	SubUV,		// SubUVComponent     — SubUVBatcher 경유
	StaticMesh,	// UStaticMeshComponent — OBJ 메시 퐁셰이딩
	Decal,
};

//PerObject
struct FPerObjectConstants
{
	FMatrix Model;
	FVector4 Color;
};

struct FFrameConstants
{
	FMatrix View;          
	FMatrix Projection;    
	float bIsWireframe = 0.0f;
	FVector WireframeColor;
};

struct FGizmoConstants
{
	FVector4 ColorTint;
	uint32 bIsInnerGizmo;	
	uint32 bClicking;
	uint32 SelectedAxis;
	float HoveredAxisOpacity;
};

struct FEditorConstants
{
	FVector CameraPosition; // xyz 사용, w padding
	uint32 Flag;
};

struct FOutlineConstants
{
	FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
	float OutlineThicknessPixels = 2.0f;
	FVector2 ViewportSize = FVector2(1.0f, 1.0f);
	float Padding0 = 0.0f;
};

struct FAABBConstants
{
	static constexpr int32 MaxVertices = 24;
	FVector Vertices[MaxVertices] = {};
	int32 VertexCount = 0;
	FColor Color;
};

struct FGridConstants
{
	float GridSpacing;
	int32 GridHalfLineCount;
	bool  bOrthographic;
	float Padding0[1];
};

struct FFontConstants
{
	const FString* Text = nullptr;			// 컴포넌트 소유 문자열 참조 (프레임 내 유효)
	const FFontResource* Font = nullptr;
	float Scale = 1.0f;
};

struct FSubUVConstants
{
	const FParticleResource* Particle = nullptr;
	uint32 FrameIndex = 0;
	float Width  = 1.0f;
	float Height = 1.0f;
};
struct FBillboardConstants
{
	ID3D11ShaderResourceView* SRV = nullptr;
	float Width = 1.0f;
	float Height = 1.0f;
};
// StaticMeshBuffer (b6) — ShaderStaticMesh.hlsl 대응
// 완전 Obj전용입니다. 추후 Bump를 Normal로 바꾸면 됩니다.
struct FStaticMeshConstants
{
	// Phong Material
	FVector AmbientColor  = { 0.2f, 0.2f, 0.2f };
	float   _Pad0         = 0.0f;

	FVector DiffuseColor  = { 0.8f, 0.8f, 0.8f };
	float   _Pad1         = 0.0f;

	FVector SpecularColor = { 0.5f, 0.5f, 0.5f };
	float   Shininess     = 32.0f;

	// Camera
	FVector CameraWorldPos = { 0.0f, 0.0f, 0.0f };
	float   _Pad2          = 0.0f;

	// ScrollUV
	float  ScrollX          = 0.f;
	float  ScrollY          = 0.f;
	float  Padding0         = 0.0f;
	uint32 bHasDiffuseMap   = 0;     // cbuffer bytes 76-79  — HLSL uint bHasDiffuseMap 대응
	uint32 bHasSpecularMap  = 0;     // cbuffer bytes 80-83  — HLSL uint bHasSpecularMap 대응
	float  Padding1         = 0.f;   // cbuffer bytes 84-87
	float  Padding2         = 0.f;   // cbuffer bytes 88-91  (16바이트 블록 완성)
};

struct FStaticMeshResources
{
	// Texture SRV (CPU-only)
	ID3D11ShaderResourceView* DiffuseSRV  = { nullptr };
	ID3D11ShaderResourceView* AmbientSRV  = { nullptr };
	ID3D11ShaderResourceView* SpecularSRV = { nullptr };
	ID3D11ShaderResourceView* BumpSRV     = { nullptr };
};

struct FFxaaConstantBuffer
{
    float EdgeThreshold = 1.0f / 8.0f;      // ex: 1.0 / 8.0
    float EdgeThresholdMin = 1.0f / 16.0f; // ex: 1.0 / 16.0
    float Subpix = 0.75f;                  // ex: 0.75
    float Padding0 = 0.0f;
};

struct FDepthSceneConstants
{
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    FVector2 Padding0 = FVector2(0.0f, 0.0f); // 16바이트 정렬 맞춤용 패딩
};

struct FViewportInfoConstants
{
    FVector2 InvFullRenderTargetSize = FVector2(0.0f, 0.0f);
    FVector2 ViewportOriginPixels = FVector2(0.0f, 0.0f);
    FVector2 ViewportSizePixels = FVector2(0.0f, 0.0f);
    FVector2 Padding0 = FVector2(0.0f, 0.0f);
};

struct FDecalConstants
{
	FMatrix DecalViewProjection;
	FVector DecalForward;
	float FadeAlpha;
};

struct FDecalResources
{
	// Decal texture SRV (CPU-only)
	ID3D11ShaderResourceView* DecalTextureSRV  = { nullptr };
};

struct FRenderCommand
{
	//	VB, IB 모두 담고 있는 MB
	FMeshBuffer* MeshBuffer = nullptr;
	uint32		 SectionIndexStart = {};
	uint32		 SectionIndexCount = {};

	FPerObjectConstants PerObjectConstants = {};

	union
	{
		FGizmoConstants Gizmo;
		FEditorConstants Editor;
		FOutlineConstants Outline;
		FAABBConstants AABB;
		FGridConstants Grid;
		FFontConstants Font;
		FSubUVConstants SubUV;
		FBillboardConstants Billboard;
		FStaticMeshConstants StaticMesh;
		FDecalConstants Decal;
	} Constants;

	struct
	{
		FStaticMeshResources StaticMesh;
		FDecalResources Decal;
	} Resources;

	EDepthStencilState DepthStencilState = static_cast<EDepthStencilState>(-1);
	EBlendState BlendState = static_cast<EBlendState>(-1);
	ERenderCommandType Type = ERenderCommandType::Primitive;
};


struct FFireBallInfo
{
	FFireBallInfo() = default;

	FFireBallInfo(const FVector4& InWorldLocation, const FColor& InLinearColor,
		float InIntensity, float InRadius, float InRadiusFalloff)
		: WorldLocation(InWorldLocation)
		, LinearColor(InLinearColor)
		, Intensity(InIntensity)
		, Radius(InRadius)
		, RadiusFalloff(InRadiusFalloff)
	{
	}

	// Setters
	void SetWorldLocation(const FVector4& InWorldLocation) { WorldLocation = InWorldLocation; }
	void SetLinearColor(const FColor& InLinearColor) { LinearColor = InLinearColor; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; }
	void SetRadius(float InRadius) { Radius = InRadius; }
	void SetRadiusFalloff(float InRadiusFalloff) { RadiusFalloff = InRadiusFalloff; }

	// Getters
	const FVector4& GetWorldLocation() const { return WorldLocation; }
	const FColor& GetLinearColor()   const { return LinearColor; }
	float           GetIntensity()     const { return Intensity; }
	float           GetRadius()        const { return Radius; }
	float           GetRadiusFalloff() const { return RadiusFalloff; }

private:
	FVector4 WorldLocation;
	FColor   LinearColor;
	float    Intensity = 0.f;
	float    Radius = 0.f;
	float    RadiusFalloff = 0.f;
};



struct FFireBallConstants
{
	FVector4 WorldLocation;
	FVector4 LinearColor;
	float Intensity;
	float Radius;
	float RadiusFalloff;
	float _padding;

};
__declspec(align(16)) struct FFireBallCBuffer
{
	FMatrix       InvViewProj;                        // 64 bytes
	FFireBallConstants FireBalls[32];    // 48 * 32 = 1536 bytes
	int              FireBallCount;                      //  4 bytes
	float            _cbPadding[3];                      // 12 bytes
};


/**
 * FHeightFogInfo
 *
 * UHeightFogComponent 가 매 프레임 RenderBus 에 밀어넣는 스냅샷 구조체.
 * HLSL FHeightFogCBuffer 의 FogInfos[] 배열 원소와 1:1 매칭됩니다.
 *
 * 패딩 규칙 (cbuffer packing, float4 경계):
 *   float3 WorldPosition    // 12 bytes
 *   float  FogDensity       //  4 bytes  → float4 완성
 *   float  FogHeightFalloff //  4 bytes
 *   float  StartDistance    //  4 bytes
 *   float  FogCutoffDistance//  4 bytes
 *   float  FogMaxOpacity    //  4 bytes  → float4 완성
 *   float3 InscatteringColor// 12 bytes
 *   float  InfluenceRadius  //  4 bytes  → float4 완성
 *   Total: 48 bytes (패딩 없음)
 */
struct FHeightFogInfo
{
	// ------------------------------------------------------------------
	//  Data
	// ------------------------------------------------------------------

	FVector WorldPosition = FVector(0.f, 0.f, 0.f);
	float   FogDensity = 0.02f;

	float   FogHeightFalloff = 0.2f;
	float   StartDistance = 0.f;
	float   FogCutoffDistance = 0.f;
	float   FogMaxOpacity = 1.f;
	bool	bIsFog = false;
	FColor InscatteringColor = FColor(0.78f, 0.86f, 1.f, 1.f);  // 연한 하늘색

	// ------------------------------------------------------------------
	//  Constructors
	// ------------------------------------------------------------------

	/** 기본 생성자 — 모든 멤버를 기본값으로 초기화합니다. */
	FHeightFogInfo() = default;

	/** 전체 멤버 직접 지정 생성자 */
	FHeightFogInfo(
		const FVector& InWorldPosition,
		float          InFogDensity,
		float          InFogHeightFalloff,
		float          InStartDistance,
		float          InFogCutoffDistance,
		float          InFogMaxOpacity,
		const FColor& InInscatteringColor,
		bool		   InbIsFog
		)
		: WorldPosition(InWorldPosition)
		, FogDensity(InFogDensity)
		, FogHeightFalloff(InFogHeightFalloff)
		, StartDistance(InStartDistance)
		, FogCutoffDistance(InFogCutoffDistance)
		, FogMaxOpacity(InFogMaxOpacity)
		, InscatteringColor(InInscatteringColor)
		, bIsFog(InbIsFog)
		
	{
	}

	// ------------------------------------------------------------------
	//  Getters
	// ------------------------------------------------------------------

	/** 컴포넌트 월드 위치 (높이 기준점) */
	const FVector& GetWorldPosition()      const { return WorldPosition; }

	/** 전체 밀도 스케일 (≥ 0) */
	float          GetFogDensity()         const { return FogDensity; }

	/** 높이 감쇠 계수 (≥ 0). 클수록 낮은 곳에 안개가 집중됩니다. */
	float          GetFogHeightFalloff()   const { return FogHeightFalloff; }

	/** 카메라로부터 안개가 시작되는 최소 거리 (월드 유닛, ≥ 0) */
	float          GetStartDistance()      const { return StartDistance; }

	/** 안개가 완전히 사라지는 최대 거리 (0 이하 = 무제한) */
	float          GetFogCutoffDistance()  const { return FogCutoffDistance; }

	/** 최대 불투명도 [0, 1] */
	float          GetFogMaxOpacity()      const { return FogMaxOpacity; }

	/** Inscattering 색상 (RGB, 각 채널 0~1) */
	const FColor& GetInscatteringColor()  const { return InscatteringColor; }


	// ------------------------------------------------------------------
	//  Setters  (범위 클램프 포함)
	// ------------------------------------------------------------------

	void SetWorldPosition(const FVector& V) { WorldPosition = V; }
	void SetFogDensity(float V) { FogDensity = V < 0.f ? 0.f : V; }
	void SetFogHeightFalloff(float V) { FogHeightFalloff = V < 0.f ? 0.f : V; }
	void SetStartDistance(float V) { StartDistance = V < 0.f ? 0.f : V; }
	void SetFogCutoffDistance(float V) { FogCutoffDistance = V; }
	void SetFogMaxOpacity(float V) { FogMaxOpacity = V < 0.f ? 0.f : V > 1.f ? 1.f : V; }
	void SetInscatteringColor(const FColor& V) { InscatteringColor = V; }
};

// hlsl 레이아웃과 1:1 매칭
struct alignas(16) FHeightFogCBuffer
{
	FVector WorldPosition;      float FogDensity;
	float   FogHeightFalloff;   float StartDistance;
	float   FogCutoffDistance;  float FogMaxOpacity;
	FVector InscatteringColor;  float NearPlane;
	FVector CameraWorldPos;     float FarPlane;
	FMatrix InvViewProj;
};