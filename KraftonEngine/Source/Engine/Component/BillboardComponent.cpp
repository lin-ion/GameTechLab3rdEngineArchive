#include "BillboardComponent.h"
#include "Math/Matrix.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Pipeline/PrimitiveProxy.h"
#include "Texture/Texture2D.h"

class FBillboardProxy : public FPrimitiveProxy
{
public:
	FBillboardProxy(UBillboardComponent* InOwner) : FPrimitiveProxy(InOwner) {}

	void UpdateProxy() override
	{
	}

	void SubmitRenderCommand(FViewContext& View) override
	{
		UBillboardComponent* Billboard = static_cast<UBillboardComponent*>(Owner);
		if (!Billboard->IsVisible()) return;

		FMeshBuffer* Buffer = Billboard->GetMeshBuffer();
		if (!Buffer || !Buffer->IsValid()) return;

		const FMatrix BillboardMatrix = FBillboardProxy::ComputeBillboardMatrix(
			View.GetCameraForward(),
			Billboard->GetWorldScale(),
			Billboard->GetWorldLocation());

		FRenderCommand Cmd        = {};
		Cmd.MeshBuffer            = Buffer;
		Cmd.Shader                = FShaderManager::Get().GetShader(EShaderType::Billboard);
		Cmd.PerObjectConstants    = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
		Cmd.SpriteSRV             = Billboard->GetSprite() ? Billboard->GetSprite()->GetSRV() : nullptr;
		Cmd.PickingId             = GetId();
		View.AddCommand(ERenderPass::Billboard, Cmd);

		if (bSelected && Billboard->SupportsOutline())
		{
			FRenderCommand MaskCmd    = {};
			MaskCmd.MeshBuffer        = Buffer;
			MaskCmd.Shader            = FShaderManager::Get().GetShader(EShaderType::Primitive);
			MaskCmd.PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
			View.AddCommand(ERenderPass::SelectionMask, MaskCmd);
		}
	}

private:
	static FMatrix ComputeBillboardMatrix(
		const FVector& CameraForward,
		const FVector& WorldScale,
		const FVector& WorldLocation)
	{
		FVector Forward = (CameraForward * -1.0f).Normalized();
		FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);
		if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
			WorldUp = FVector(0.0f, 1.0f, 0.0f);

		const FVector Right = WorldUp.Cross(Forward).Normalized();
		const FVector Up    = Forward.Cross(Right).Normalized();

		FMatrix Rot;
		Rot.SetAxes(Forward, Right, Up);

		return FMatrix::MakeScaleMatrix(WorldScale)
			* Rot
			* FMatrix::MakeTranslationMatrix(WorldLocation);
	}
};

// ============================================================
DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)

FPrimitiveProxy* UBillboardComponent::CreateProxy()
{
	return new FBillboardProxy(this);
}

FMeshBuffer* UBillboardComponent::GetMeshBuffer() const
{
	return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::SpriteQuad);
}

const FMeshData* UBillboardComponent::GetMeshData() const
{
	// SpriteQuad 는 FTextureVertex 기반이므로 FMeshData(FVertex) 접근 불가 — nullptr 반환
	return nullptr;
}

void UBillboardComponent::TickComponent(float DeltaTime)
{
	// if (!GetOwner() || !GetOwner()->GetWorld()) return;
	UpdateWorldAABB();
}

void UBillboardComponent::UpdateWorldAABB() const
{
	const float   NewScale    = std::max({ GetWorldScale().X, GetWorldScale().Y, GetWorldScale().Z });
	const FVector WorldCenter = GetWorldLocation();
	const FVector Extent(NewScale, NewScale, NewScale);

	WorldAABBMinLocation = WorldCenter - Extent;
	WorldAABBMaxLocation = WorldCenter + Extent;
}

// TODO: ComputeBillboardMatrixForRay 랑 LineTraceComponent 는 한번 정리해야함
static FMatrix ComputeBillboardMatrixForRay(
	const FVector& RayDirection,
	const FVector& WorldScale,
	const FVector& WorldLocation)
{
	FVector Forward = (RayDirection * -1.0f).Normalized();
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);
	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
		WorldUp = FVector(0.0f, 1.0f, 0.0f);

	const FVector Right = WorldUp.Cross(Forward).Normalized();
	const FVector Up    = Forward.Cross(Right).Normalized();

	FMatrix Rot;
	Rot.SetAxes(Forward, Right, Up);

	return FMatrix::MakeScaleMatrix(WorldScale) * Rot * FMatrix::MakeTranslationMatrix(WorldLocation);
}

bool UBillboardComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult, float InClosestT)
{
	// Ray 방향으로 빌보드 행렬을 계산
	FMatrix BillboardWorldMatrix = ComputeBillboardMatrixForRay(Ray.Direction, GetWorldScale(), GetWorldLocation());
	FMatrix InvWorldMatrix = BillboardWorldMatrix.GetInverse();

	FRay LocalRay;
	LocalRay.Origin = InvWorldMatrix.TransformPositionWithW(Ray.Origin);
	LocalRay.Direction = InvWorldMatrix.TransformVector(Ray.Direction).Normalized();

	// 빌보드는 X축 평면(YZ 평면)에 위치한다고 가정 (ComputeBillboardMatrix의 axes 설정 기준)
	if (std::abs(LocalRay.Direction.X) < 1e-6f) return false;

	float t = -LocalRay.Origin.X / LocalRay.Direction.X;
	if (t < 0.0f) return false;

	FVector LocalHitPos = LocalRay.Origin + LocalRay.Direction * t;

	// SpriteQuad의 크기가 -0.5 ~ 0.5 라고 가정
	if (LocalHitPos.Y >= -0.5f && LocalHitPos.Y <= 0.5f &&
		LocalHitPos.Z >= -0.5f && LocalHitPos.Z <= 0.5f)
	{
		FVector WorldHitPos = BillboardWorldMatrix.TransformPositionWithW(LocalHitPos);
		OutHitResult.Distance = (WorldHitPos - Ray.Origin).Length();
		OutHitResult.HitComponent = this;
		OutHitResult.bHit = true;
		return true;
	}

	return false;
}
