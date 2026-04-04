#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
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

	void OnDraw(FViewContext& View) override
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

	const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();
	if (!ActiveCamera) return;

	FVector WorldLocation = GetWorldLocation();
	FVector CameraForward = ActiveCamera->GetForwardVector().Normalized();
	FVector Forward = CameraForward * -1;
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f); // 임시 Up축 변경
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

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