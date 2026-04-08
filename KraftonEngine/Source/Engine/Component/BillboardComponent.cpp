#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Render/Resource/ShaderManager.h"

#include "Render/Pipeline/PrimitiveProxy.h"
#include "Render/Pipeline/WorldRenderProxy.h"

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
		FMeshBuffer* Buffer = Billboard->GetMeshBuffer();
		if (!Buffer || !Buffer->IsValid()) return;

		FMatrix PerViewMatrix = Billboard->ComputeBillboardMatrix(View.GetCameraForward());

		// Draw Opaque
		FRenderCommand Cmd = {};
		Cmd.PerObjectConstants = FPerObjectConstants::FromWorldMatrix(PerViewMatrix);
		Cmd.Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
		Cmd.MeshBuffer = Buffer;
		View.AddCommand(ERenderPass::Opaque, Cmd);

		if (bSelected)
		{
			if (Owner->SupportsOutline())
			{
				View.AddCommand(ERenderPass::SelectionMask, Cmd);
			}

			if (View.GetShowFlags().bBoundingVolume)
			{
				FAABBEntry Entry = {};
				FBoundingBox Box = Owner->GetWorldBoundingBox();
				Entry.AABB.Min = Box.Min;
				Entry.AABB.Max = Box.Max;
				Entry.AABB.Color = FColor::White();
				View.AddAABBEntry(std::move(Entry));
			}
		}
	}
};

DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)

FPrimitiveProxy* UBillboardComponent::CreateProxy()
{
	return new FBillboardProxy(this);
}

void UBillboardComponent::TickComponent(float DeltaTime)
{
	if (!GetOwner() || !GetOwner()->GetWorld()) return;

	// Billboard 행렬은 Render Proxy에서 View별로 계산되므로,
	// 여기서는 단순히 위치와 스케일만 반영된 행렬을 갱신하거나 AABB만 업데이트합니다.
	// CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * FMatrix::MakeTranslationMatrix(GetWorldLocation());

	UpdateWorldAABB();
}

FMatrix UBillboardComponent::ComputeBillboardMatrix(const FVector& CameraForward) const
{
	// TickComponent와 동일한 로직
	FVector Forward = (CameraForward * -1.0f).Normalized();
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f);
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	return FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(GetWorldLocation());
}
