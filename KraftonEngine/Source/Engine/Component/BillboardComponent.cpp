#include "BillboardComponent.h"
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
