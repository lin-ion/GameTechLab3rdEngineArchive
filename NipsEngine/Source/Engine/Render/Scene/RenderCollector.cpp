#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/ActorIterator.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/FireBallComponent.h"
#include "Component/HeightFogComponent.h"
#include "Core/ResourceManager.h"
#include "Component/DecalComponent.h"
#include "Engine/Geometry/Frustum.h"
#include "Engine/Geometry/OBB.h"
#include "Engine/Asset/StaticMesh.h"
#include "Render/Resource/Material.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include <unordered_set>

namespace
{
	struct FScopedMsAccumulator
	{
		FScopedMsAccumulator(const LARGE_INTEGER& InFrequency, double& InAccumulatedMs)
			: Frequency(InFrequency), AccumulatedMs(InAccumulatedMs)
		{
			QueryPerformanceCounter(&StartTime);
		}

		~FScopedMsAccumulator()
		{
			LARGE_INTEGER EndTime{};
			QueryPerformanceCounter(&EndTime);

			AccumulatedMs += static_cast<double>(EndTime.QuadPart - StartTime.QuadPart) * 1000.0 /
				static_cast<double>(Frequency.QuadPart);
		}

	private:
		LARGE_INTEGER StartTime{};
		const LARGE_INTEGER& Frequency;
		double& AccumulatedMs;
	};

	FColor MakeBVHInternalNodeColor(int32 PathIndexFromLeaf, int32 PathLength)
	{
		if (PathLength <= 1)
		{
			return FColor::Yellow();
		}

		const float T = static_cast<float>(PathIndexFromLeaf) / static_cast<float>(PathLength - 1);
		return FColor::Lerp(FColor::Cyan(), FColor::Yellow(), T);
	}

	bool UsesCameraDependentRenderBounds(const UPrimitiveComponent* PrimitiveComponent)
	{
		if (PrimitiveComponent == nullptr)
		{
			return false;
		}

		switch (PrimitiveComponent->GetPrimitiveType())
		{
		case EPrimitiveType::EPT_Billboard:
		case EPrimitiveType::EPT_Text:
		case EPrimitiveType::EPT_SubUV:
		// HACK: Decal을 Frustum Query에서 제외했기 때문에 땜빵으로 추가 ㅠ
		case EPrimitiveType::EPT_Decal:
			return true;
		default:
			return false;
		}
	}

	FMatrix MakeViewBillboardMatrix(const UPrimitiveComponent* Primitive, const FRenderBus& RenderBus)
	{
		FVector BillboardScale = Primitive->GetWorldScale();
		if (Primitive->GetPrimitiveType() == EPrimitiveType::EPT_Billboard)
		{
			const UBillboardComponent* BillboardComp = static_cast<const UBillboardComponent*>(Primitive);
			BillboardScale.Y *= BillboardComp->GetWidth();
			BillboardScale.Z *= BillboardComp->GetHeight();
		}

		const FMatrix WorldMatrix = Primitive->GetWorldMatrix();
		return UBillboardComponent::MakeBillboardWorldMatrix(
			WorldMatrix.GetOrigin(),
			BillboardScale,
			RenderBus.GetCameraForward(),
			RenderBus.GetCameraRight(),
			RenderBus.GetCameraUp());
	}

	FMatrix MakeViewSubUVSelectionMatrix(const USubUVComponent* SubUVComp, const FRenderBus& RenderBus)
	{
		const FVector WorldScale = SubUVComp->GetWorldScale();
		return USubUVComponent::MakeBillboardWorldMatrix(
			SubUVComp->GetWorldLocation(),
			FVector(
				WorldScale.X > 0.01f ? WorldScale.X : 0.01f,
				SubUVComp->GetWidth() * WorldScale.Y,
				SubUVComp->GetHeight() * WorldScale.Z),
			RenderBus.GetCameraForward(),
			RenderBus.GetCameraRight(),
			RenderBus.GetCameraUp());
	}
	/*
	* BillBoardComponent를 상속받은 text, SubUV가 사용하는 AABB 계산함수(의존성 분리)
	*/
	FAABB BuildQuadAABB(const FMatrix& WorldMatrix)
	{
		static constexpr FVector LocalQuadCorners[4] =
		{
			FVector(0.0f, -0.5f,  0.5f),
			FVector(0.0f,  0.5f,  0.5f),
			FVector(0.0f,  0.5f, -0.5f),
			FVector(0.0f, -0.5f, -0.5f)
		};

		FAABB Box;
		Box.Reset();

		for (const FVector& Corner : LocalQuadCorners)
		{
			Box.Expand(WorldMatrix.TransformPosition(Corner));
		}

		return Box;
	}

	FAABB BuildRenderAABB(const UPrimitiveComponent* PrimitiveComponent, const FRenderBus& RenderBus)
	{
		switch (PrimitiveComponent->GetPrimitiveType())
		{
		case EPrimitiveType::EPT_Billboard:
			return BuildQuadAABB(MakeViewBillboardMatrix(PrimitiveComponent, RenderBus));
		case EPrimitiveType::EPT_Text:
		{
			const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(PrimitiveComponent);
			return BuildQuadAABB(TextComp->GetTextMatrix());
		}
		case EPrimitiveType::EPT_SubUV:
		{
			const USubUVComponent* SubUVComp = static_cast<const USubUVComponent*>(PrimitiveComponent);
			return BuildQuadAABB(MakeViewSubUVSelectionMatrix(SubUVComp, RenderBus));
		}

		default:
			return PrimitiveComponent->GetWorldAABB();
		}
	}

	int32 SelectLODLevel(const FVector& CameraPos, const FAABB& Bounds, const FMatrix& ProjMatrix, int32 ValidLODCount)
	{
		bool IsOrthoGraphic = (std::abs(ProjMatrix.M[3][3] - 1.0f) < 1e-4f);
		if (ValidLODCount <= 1 || IsOrthoGraphic) return 0;

		// 1. 바운딩 박스를 통해 바운딩 스피어 반지름 및 카메라와의 거리 계산
		const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
		const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
		const float SphereRadius = std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z);

		const FVector Diff = Center - CameraPos;
		const float Dist = std::sqrt(Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z);

		if (Dist <= 1e-4f) return 0;

		const float ProjectedRadius = (SphereRadius / Dist) * ProjMatrix.M[2][1];
		const float ScreenCoverage = ProjectedRadius; 

		static constexpr float Thresholds[] = { 0.05f, 0.03f, 0.01f, 0.008f };
		static constexpr int32 ThresholdCount = static_cast<int32>(sizeof(Thresholds) / sizeof(Thresholds[0]));

		const int32 MaxLOD = ValidLODCount - 1;
		for (int32 LOD = 0; LOD < MaxLOD; ++LOD)
		{
			float Threshold = (LOD < ThresholdCount) ? Thresholds[LOD] : 0.0f;
			if (ScreenCoverage >= Threshold)
				return LOD;
		}

		// 화면에 차지하는 비율이 가장 낮을 경우 최하위 LOD 반환
		return MaxLOD;
	}

}

void FRenderCollector::CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
                                    const FFrustum* ViewFrustum)
{
	ResetCullingStats();
	ResetDecalStats();

	if (!World) return;

	if (ViewFrustum != nullptr)
	{
		CollectWorldWithFrustum(World, *ViewFrustum, ShowFlags, ViewMode, RenderBus);
		return;
	}

	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (!Actor || !Actor->IsVisible()) continue;

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (Primitive != nullptr && Primitive->IsVisible())
			{
				++LastCullingStats.TotalVisiblePrimitiveCount;
			}
		}

		CollectFromActor(Actor, ShowFlags, ViewMode, RenderBus);
	}
}

void FRenderCollector::ResetCullingStats()
{
	LastCullingStats = {};
}

void FRenderCollector::ResetDecalStats()
{
	LastDecalStats = {};
}

void FRenderCollector::CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	VisiblePrimitiveScratch.clear();
	World->GetSpatialIndex().FrustumQueryPrimitives(ViewFrustum, VisiblePrimitiveScratch, FrustumQueryScratch);

	for (UPrimitiveComponent* Primitive : VisiblePrimitiveScratch)
	{
		if (Primitive == nullptr || UsesCameraDependentRenderBounds(Primitive))
		{
			continue;
		}

		++LastCullingStats.BVHPassedPrimitiveCount;
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus);
	}

	std::unordered_set<UPrimitiveComponent*> CollectedCameraDependentPrimitives;
	CollectedCameraDependentPrimitives.reserve(32);

	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (Actor == nullptr || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (Primitive == nullptr || !Primitive->IsVisible())
			{
				continue;
			}

			++LastCullingStats.TotalVisiblePrimitiveCount;

			if (!UsesCameraDependentRenderBounds(Primitive))
			{
				continue;
			}

			if (!CollectedCameraDependentPrimitives.insert(Primitive).second)
			{
				continue;
			}

			if (ViewFrustum.Intersects(BuildRenderAABB(Primitive, RenderBus)) == FFrustum::EFrustumIntersectResult::Outside)
			{
				continue;
			}

			++LastCullingStats.FallbackPassedPrimitiveCount;
			CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus);
		}
	}
}

void FRenderCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
                                        FOutlinePostProcessData& OutOutlineData)
{
	OutOutlineData = {};

	bool bHasSelectionMask = false;
	for (AActor* Actor : SelectedActors)
	{
		bHasSelectionMask |= CollectFromSelectedActor(Actor, ShowFlags, ViewMode, RenderBus);
	}

	if (bHasSelectionMask)
	{
		OutOutlineData.bEnabled = true;
		OutOutlineData.Constants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
		OutOutlineData.Constants.OutlineThicknessPixels = 5.0f;
	}
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic)
{
	FRenderCommand Cmd = {};
	Cmd.Type = ERenderCommandType::Grid;
	Cmd.Constants.Grid.GridSpacing = GridSpacing;
	Cmd.Constants.Grid.GridHalfLineCount = GridHalfLineCount;
	Cmd.Constants.Grid.bOrthographic = bOrthographic;
	RenderBus.AddCommand(ERenderPass::Grid, Cmd);
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation)
{
	if (ShowFlags.bGizmo == false) return;
	if (!Gizmo || !Gizmo->IsVisible()) return;

	FMeshBuffer* GizmoMesh = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
	FMatrix WorldMatrix = Gizmo->GetWorldMatrix();
	bool bHolding = Gizmo->IsHolding();
	int32 SelectedAxis = Gizmo->GetSelectedAxis();

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Gizmo;
		Cmd.MeshBuffer = GizmoMesh;

		Cmd.SectionIndexStart = 0;
		Cmd.SectionIndexCount = GizmoMesh->GetIndexBuffer().GetIndexCount();

		Cmd.PerObjectConstants = FPerObjectConstants{ WorldMatrix };

		if (bInner)
		{
			Cmd.DepthStencilState = EDepthStencilState::GizmoInside;
			Cmd.BlendState = EBlendState::Opaque;
		}
		else
		{
			Cmd.DepthStencilState = EDepthStencilState::GizmoOutside;
			Cmd.BlendState = EBlendState::Opaque;
		}
		Cmd.Constants.Gizmo.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Cmd.Constants.Gizmo.bIsInnerGizmo = bInner ? 1 : 0;
		Cmd.Constants.Gizmo.bClicking = bHolding ? 1 : 0;
		Cmd.Constants.Gizmo.SelectedAxis = (SelectedAxis >= 0 && bIsActiveOperation) ? (uint32)SelectedAxis : 0xffffffffu;
		Cmd.Constants.Gizmo.HoveredAxisOpacity = 1.0f;
		return Cmd;
		};

	RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(false));

	if (!bHolding)
	{
		RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(true));
	}
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!Actor->IsVisible()) return;

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus);
	}
}

bool FRenderCollector::CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	(void)ViewMode;
	if (!Actor->IsVisible()) return false;

	bool bHasSelectionMask = false;
	std::unordered_set<int32> SeenBVHNodeIndices;

	for (UPrimitiveComponent* primitiveComponent : Actor->GetPrimitiveComponents())
	{

		if (!primitiveComponent->IsVisible()) continue;

		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Decal)
		{
			CollectAABBCommand(primitiveComponent, ShowFlags, RenderBus);
			CollectBVHInternalNodeAABBs(primitiveComponent, ShowFlags, RenderBus, SeenBVHNodeIndices);
			continue;
		}

		FMeshBuffer* MeshBuffer = nullptr;
		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_StaticMesh)
		{
			auto* StaticMeshComp = static_cast<UStaticMeshComponent*>(primitiveComponent);
			MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMeshComp->GetStaticMesh());
		}
		else
		{
			MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
		}

		if (!MeshBuffer)
		{
			continue;
		}

		FRenderCommand BaseCmd{};
		BaseCmd.MeshBuffer = MeshBuffer;
		BaseCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };
		BaseCmd.SectionIndexStart = 0;
		BaseCmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();

		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
		{
			UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(primitiveComponent);
			const FFontResource* Font = TextComp->GetFont();
			if (!Font || !Font->IsLoaded()) continue;
			const FString& Text = TextComp->GetText();
			if (Text.empty()) continue;

			FMatrix WorldMatrix = TextComp->GetTextMatrix();

			FRenderCommand TextCmd = BaseCmd;
			BaseCmd.PerObjectConstants.Model = WorldMatrix;
			TextCmd.PerObjectConstants = FPerObjectConstants{TextComp->GetWorldMatrix()};
			TextCmd.Type = ERenderCommandType::Font;
			TextCmd.PerObjectConstants.Color = TextComp->GetColor();
			TextCmd.Constants.Font.Text = &Text;
			TextCmd.Constants.Font.Font = Font;
			TextCmd.Constants.Font.Scale = TextComp->GetFontSize();
			TextCmd.BlendState = EBlendState::AlphaBlend;
			TextCmd.DepthStencilState = EDepthStencilState::Default;
			RenderBus.AddCommand(ERenderPass::Font, TextCmd);
		}
		else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SubUV)
		{
			BaseCmd.PerObjectConstants.Model = MakeViewSubUVSelectionMatrix(
				static_cast<USubUVComponent*>(primitiveComponent),
				RenderBus);
		}

		else if(primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Billboard)
		{
			BaseCmd.PerObjectConstants.Model = MakeViewBillboardMatrix(primitiveComponent, RenderBus);
		}

		if (!primitiveComponent->SupportsOutline()) continue;

		// Selection Mask
		FRenderCommand MaskCmd = BaseCmd;
		MaskCmd.Type = ERenderCommandType::SelectionMask;
		RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
		bHasSelectionMask = true;
		CollectAABBCommand(primitiveComponent, ShowFlags, RenderBus);
		CollectBVHInternalNodeAABBs(primitiveComponent, ShowFlags, RenderBus, SeenBVHNodeIndices);
	}

	return bHasSelectionMask;
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!Primitive->IsVisible()) return;

	EPrimitiveType PrimType = Primitive->GetPrimitiveType();

	switch (PrimType)
	{
	case EPrimitiveType::EPT_StaticMesh:
	{
		if (!ShowFlags.bPrimitives) return;

		UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Primitive);
		const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();

		if (!StaticMesh || !StaticMesh->HasValidMeshData()) return;

		// 1. 카메라 정보 및 AABB 가져오기
        FVector CameraPos = RenderBus.GetCameraPosition();
        FMatrix ProjMatrix = RenderBus.GetProj();
        FAABB Bounds = StaticMeshComp->GetWorldAABB();
        const int32 ValidLODCount = StaticMesh->GetValidLODCount();

        // 2. LOD 레벨 계산
		int32 SelectedLOD = 0; // 기본값은 항상 원본(최고 화질)
        if (ShowFlags.bEnableLOD)
        {
            SelectedLOD = SelectLODLevel(CameraPos, Bounds, ProjMatrix, ValidLODCount);
        }

		FMeshBuffer* MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMesh, SelectedLOD);
        if (!MeshBuffer) return;

        const FStaticMesh* MeshData = StaticMesh->GetMeshData(SelectedLOD);
        const TArray<FStaticMeshSection>& Sections = MeshData->Sections;

		for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
		{
			const FStaticMeshSection& Section = Sections[SectionIdx];

			FRenderCommand Cmd = {};
			Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
			Cmd.Type = ERenderCommandType::StaticMesh;
			Cmd.MeshBuffer = MeshBuffer;
			Cmd.DepthStencilState = EDepthStencilState::Default;
			Cmd.BlendState = EBlendState::Opaque;

			Cmd.SectionIndexStart = Section.StartIndex;
			Cmd.SectionIndexCount = Section.IndexCount;

			Cmd.Constants.StaticMesh.CameraWorldPos = RenderBus.GetCameraPosition();

			// 메테리얼 정보가 없을 시 디폴트 메테리얼을 사용합니다.
			static const FMaterial EngineDefaultMaterial{};

			const FMaterial* MtlData = StaticMeshComp->GetMaterial(SectionIdx);

			if (!MtlData) MtlData = &EngineDefaultMaterial;
	
			Cmd.Constants.StaticMesh.AmbientColor = MtlData->AmbientColor;
			Cmd.Constants.StaticMesh.DiffuseColor = MtlData->DiffuseColor;
			Cmd.Constants.StaticMesh.SpecularColor = MtlData->SpecularColor;
			Cmd.Constants.StaticMesh.Shininess = MtlData->Shininess;

			Cmd.Constants.StaticMesh.ScrollX = StaticMeshComp->GetScroll().first;
			Cmd.Constants.StaticMesh.ScrollY = StaticMeshComp->GetScroll().second;

			ID3D11ShaderResourceView* DefaultSRV = FResourceManager::Get().GetDefaultWhiteSRV();

			auto ResolveSRV = [&](const FString& Path) -> ID3D11ShaderResourceView*
			{
				FMaterialResource* Res = FResourceManager::Get().FindTexture(Path);
				return (Res && Res->SRV) ? Res->SRV.Get() : DefaultSRV;
			};

			// 와이어 프레임이 있는 경우 텍스쳐를 사용하지 않는 메테리얼에게 기본 텍스쳐를 강제 주입
			if (ViewMode == EViewMode::Wireframe)
			{
				Cmd.Constants.StaticMesh.bHasDiffuseMap =  1u;
				Cmd.Constants.StaticMesh.bHasSpecularMap = 1u;
				Cmd.Resources.StaticMesh.DiffuseSRV = DefaultSRV;
				Cmd.Resources.StaticMesh.AmbientSRV = DefaultSRV;
				Cmd.Resources.StaticMesh.SpecularSRV = DefaultSRV;
				Cmd.Resources.StaticMesh.BumpSRV = DefaultSRV;
			}
			else
			{
				Cmd.Constants.StaticMesh.bHasDiffuseMap = MtlData->bHasDiffuseTexture ? 1u : 0u;
				Cmd.Constants.StaticMesh.bHasSpecularMap = MtlData->bHasSpecularTexture ? 1u : 0u;
				Cmd.Resources.StaticMesh.DiffuseSRV = MtlData->bHasDiffuseTexture ? ResolveSRV(MtlData->DiffuseTexPath) : DefaultSRV;
				Cmd.Resources.StaticMesh.AmbientSRV = MtlData->bHasAmbientTexture ? ResolveSRV(MtlData->AmbientTexPath) : DefaultSRV;
				Cmd.Resources.StaticMesh.SpecularSRV = MtlData->bHasSpecularTexture ? ResolveSRV(MtlData->SpecularTexPath) : DefaultSRV;
				Cmd.Resources.StaticMesh.BumpSRV = MtlData->bHasBumpTexture ? ResolveSRV(MtlData->BumpTexPath) : DefaultSRV;
			}

			RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
		}

		break;
	}

	case EPrimitiveType::EPT_Text:
	{
		if (!ShowFlags.bBillboardText) return;

		UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Primitive);
		const FFontResource* Font = TextComp->GetFont();
		if (!Font || !Font->IsLoaded()) return;

		const FString& Text = TextComp->GetText();
		if (Text.empty()) return;

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Font;
		Cmd.PerObjectConstants = FPerObjectConstants{TextComp->GetWorldMatrix(), TextComp->GetColor()};
		Cmd.Constants.Font.Text = &Text;
		Cmd.Constants.Font.Font = Font;
		Cmd.Constants.Font.Scale = TextComp->GetFontSize();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;

		RenderBus.AddCommand(ERenderPass::Font, Cmd);
		break;
	}

	case EPrimitiveType::EPT_SubUV:
	{
		USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Primitive);
		const FParticleResource* Particle = SubUVComp->GetParticle();
		if (!Particle || !Particle->IsLoaded()) return;

		FRenderCommand Cmd = {};
		Cmd.PerObjectConstants = FPerObjectConstants{
			MakeViewSubUVSelectionMatrix(SubUVComp, RenderBus),
			FColor::White().ToVector4() };
		Cmd.Type = ERenderCommandType::SubUV;
		Cmd.Constants.SubUV.Particle = Particle;
		Cmd.Constants.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
		Cmd.Constants.SubUV.Width = SubUVComp->GetWidth();
		Cmd.Constants.SubUV.Height = SubUVComp->GetHeight();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;

		RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
		break;
	}

	case EPrimitiveType::EPT_Billboard:
	{
		UBillboardComponent* BillboardComp = static_cast<UBillboardComponent*>(Primitive);
		FMaterialResource* Sprite = BillboardComp->GetCachedSprite();
		FMeshBuffer* MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Billboard);

		ID3D11ShaderResourceView* SRV = (Sprite && Sprite->SRV)	? Sprite->SRV.Get() : FResourceManager::Get().GetDefaultWhiteSRV();

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Billboard;
		Cmd.MeshBuffer = MeshBuffer;
		Cmd.SectionIndexStart = 0;
		Cmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
		Cmd.PerObjectConstants = FPerObjectConstants{
			MakeViewBillboardMatrix(Primitive, RenderBus),
			FColor::White().ToVector4() };
		Cmd.Constants.Billboard.SRV = SRV;
		Cmd.Constants.Billboard.Width = BillboardComp->GetWidth();
		Cmd.Constants.Billboard.Height = BillboardComp->GetHeight();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;

		RenderBus.AddCommand(ERenderPass::Billboard, Cmd);
		break;
	}

	case EPrimitiveType::EPT_Decal:
	{
		if (!ShowFlags.bDecal) { return; }

		static LARGE_INTEGER PerfFrequency = []()
		{
			LARGE_INTEGER Frequency{};
			QueryPerformanceFrequency(&Frequency);
			return Frequency;
		}();

		LastDecalStats.ActiveDecals++;

		UDecalComponent* DecalComponent = static_cast<UDecalComponent*>(Primitive);
		FTextureResource* DecalTexture = DecalComponent->GetCachedDecalTexture();
		ID3D11ShaderResourceView* SRV = (DecalTexture && DecalTexture->SRV) ? DecalTexture->SRV.Get() : FResourceManager::Get().GetDefaultWhiteSRV();

		AActor* DecalOwner = DecalComponent->GetOwner();
		UWorld* World = DecalOwner ? DecalOwner->GetWorld() : nullptr;
		if (World == nullptr) { return; }

		const FMatrix DecalViewProjection = DecalComponent->GetDecalViewProjection();
		const FOBB DecalOBB = DecalComponent->GetDecalOBB();
		{
			FScopedMsAccumulator QueryTimer(PerfFrequency, LastDecalStats.QueryTimeMs);
			FScopedMsAccumulator CollectTimer(PerfFrequency, LastDecalStats.CollectTimeMs);

			FFrustum DecalFrustum;
			DecalFrustum.UpdateFromCamera(DecalViewProjection);

			DecalCandidateScratch.clear();
			DecalAffectedMeshScratch.clear();
			World->GetSpatialIndex().FrustumQueryPrimitives(DecalFrustum, DecalCandidateScratch, DecalQueryScratch);
			LastDecalStats.QueryCandidates += static_cast<int32>(DecalCandidateScratch.size());
		}

		{
			FScopedMsAccumulator CollectTimer(PerfFrequency, LastDecalStats.CollectTimeMs);

			for (UPrimitiveComponent* Candidate : DecalCandidateScratch)
			{
				if (Candidate == nullptr || !Candidate->IsVisible()) { continue; }
				if (Candidate->GetPrimitiveType() != EPrimitiveType::EPT_StaticMesh) { continue; }

				LastDecalStats.StaticMeshCandidates++;
				const FAABB& CandidateAABB = Candidate->GetWorldAABB();

				// SAT 축 4~6: AABB 면 법선 (월드 X, Y, Z)
				{
					FScopedMsAccumulator SATAABBTimer(PerfFrequency, LastDecalStats.SATAABBAxesTimeMs);
					if (!DecalOBB.IntersectAABBWithSAT(CandidateAABB, true, false, false))
					{
						LastDecalStats.CulledByAABBAxes++;
						continue;
					}
				}

				// SAT 축 7~15: 외적 9개
				{
					FScopedMsAccumulator SATCrossTimer(PerfFrequency, LastDecalStats.SATCrossAxesTimeMs);
					if (!DecalOBB.IntersectAABBWithSAT(CandidateAABB, false, false, true))
					{
						LastDecalStats.CulledByCrossAxes++;
						continue;
					}
				}

				UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Candidate);
				DecalAffectedMeshScratch.push_back(StaticMeshComponent);
			}
		}

		for (UStaticMeshComponent* StaticMeshComponent : DecalAffectedMeshScratch)
		{
			const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
			if (!StaticMesh || !StaticMesh->HasValidMeshData()) { continue; }

			FMeshBuffer* MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMesh, 0);
			if (!MeshBuffer) { continue; }

			const FStaticMesh* MeshData = StaticMesh->GetMeshData(0);
			if (MeshData == nullptr) { continue; }

			FRenderCommand Cmd = {};
			Cmd.Type = ERenderCommandType::Decal;
			Cmd.DepthStencilState = EDepthStencilState::DepthReadOnly;
			Cmd.BlendState = EBlendState::AlphaBlend;

			// Static Mesh
			Cmd.PerObjectConstants = FPerObjectConstants{ StaticMeshComponent->GetWorldMatrix(), FColor::White().ToVector4() };
			Cmd.MeshBuffer = MeshBuffer;
			Cmd.SectionIndexStart = 0;
			Cmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();

			// Decal Info
			Cmd.Constants.Decal.DecalForward = DecalComponent->GetForwardVector();
			Cmd.Constants.Decal.DecalViewProjection = DecalViewProjection;
			Cmd.Constants.Decal.FadeAlpha = DecalComponent->FadeAlpha;
			Cmd.Resources.Decal.DecalTextureSRV = SRV;
			RenderBus.AddCommand(ERenderPass::Decal, Cmd);
			LastDecalStats.AffectedPrimitives++;
		}

		if (ShowFlags.bBoundingVolume && !RenderBus.bIsPIE)
		{
			FRenderCommand OBBCmd = {};
			OBBCmd.Type = ERenderCommandType::DebugBox;
			OBBCmd.Constants.AABB.Color = FColor(0, 160, 0);

			TStaticArray<FVector, 8> Corners;
			DecalOBB.GetCorners(Corners);

			static constexpr int32 EdgeIndices[24] = {
				0, 1, 1, 3, 3, 2, 2, 0,
				4, 5, 5, 7, 7, 6, 6, 4,
				0, 4, 1, 5, 2, 6, 3, 7
			};

			OBBCmd.Constants.AABB.VertexCount = 24;
			for (int32 EdgeIndex = 0; EdgeIndex < 24; ++EdgeIndex)
			{
				OBBCmd.Constants.AABB.Vertices[EdgeIndex] = Corners[EdgeIndices[EdgeIndex]];
			}
			RenderBus.AddCommand(ERenderPass::Editor, OBBCmd);
		}
		break;
	}

	case EPrimitiveType::EPT_FireBall:
	{
		UFireBallComponent* FireBallComponent = static_cast<UFireBallComponent*>(Primitive);
		FFireBallInfo FireBallInfo = CollectFireBallInfoFromFireBallComponent(FireBallComponent);
		RenderBus.GatherFireBallComponentInfo(FireBallInfo);
		break;

	}
	case EPrimitiveType::EPT_HeightFog:
	{
		UHeightFogComponent* HeightFogCompoent = static_cast<UHeightFogComponent*>(Primitive);
		FHeightFogInfo HeightFogInfo = CollectHeightFogInfoFromFogComponent(HeightFogCompoent);
		RenderBus.GatherHeightFogComponentInfo(HeightFogInfo);
		break;
	}

	default:
		if (PrimType == EPrimitiveType::EPT_TransGizmo || PrimType == EPrimitiveType::EPT_RotGizmo || PrimType == EPrimitiveType::EPT_ScaleGizmo)
		{
			return;
		}
		return;
	}
}

void FRenderCollector::CollectBVHInternalNodeAABBs(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags,
                                                   FRenderBus& RenderBus, std::unordered_set<int32>& SeenNodeIndices)
{
	if (!ShowFlags.bBoundingVolume || !ShowFlags.bBVHBoundingVolume || PrimitiveComponent == nullptr)
	{
		return;
	}

	AActor* Owner = PrimitiveComponent->GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	const FWorldSpatialIndex& SpatialIndex = World->GetSpatialIndex();
	const int32 ObjectIndex = SpatialIndex.FindObjectIndex(PrimitiveComponent);
	if (ObjectIndex == FBVH::INDEX_NONE)
	{
		return;
	}

	const FBVH& BVH = SpatialIndex.GetBVH();
	const TArray<int32>& ObjectToLeafNode = BVH.GetObjectToLeafNode();
	if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
	{
		return;
	}

	const int32 LeafNodeIndex = ObjectToLeafNode[ObjectIndex];
	if (LeafNodeIndex == FBVH::INDEX_NONE)
	{
		return;
	}

	const TArray<FBVH::FNode>& Nodes = BVH.GetNodes();
	if (LeafNodeIndex < 0 || LeafNodeIndex >= static_cast<int32>(Nodes.size()))
	{
		return;
	}

	TArray<int32> PathToRoot;
	PathToRoot.reserve(16);

	int32 CurrentNodeIndex = Nodes[LeafNodeIndex].Parent;
	while (CurrentNodeIndex != FBVH::INDEX_NONE)
	{
		if (CurrentNodeIndex < 0 || CurrentNodeIndex >= static_cast<int32>(Nodes.size()))
		{
			break;
		}

		PathToRoot.push_back(CurrentNodeIndex);
		CurrentNodeIndex = Nodes[CurrentNodeIndex].Parent;
	}

	for (int32 PathIndex = 0; PathIndex < static_cast<int32>(PathToRoot.size()); ++PathIndex)
	{
		const int32 NodeIndex = PathToRoot[PathIndex];
		if (!SeenNodeIndices.insert(NodeIndex).second)
		{
			continue;
		}

		const FBVH::FNode& Node = Nodes[NodeIndex];
		if (Node.IsLeaf())
		{
			continue;
		}

		const FColor Color = MakeBVHInternalNodeColor(PathIndex, static_cast<int32>(PathToRoot.size()));
		CollectAABBCommand(Node.Bounds, Color, RenderBus);
	}
}

void FRenderCollector::CollectAABBCommand(const FAABB& Box, const FColor& Color, FRenderBus& RenderBus)
{
	static constexpr FVector UnitCorners[8] = {
		FVector(0.0f, 0.0f, 0.0f),
		FVector(0.0f, 0.0f, 1.0f),
		FVector(0.0f, 1.0f, 0.0f),
		FVector(0.0f, 1.0f, 1.0f),
		FVector(1.0f, 0.0f, 0.0f),
		FVector(1.0f, 0.0f, 1.0f),
		FVector(1.0f, 1.0f, 0.0f),
		FVector(1.0f, 1.0f, 1.0f)
	};
	static constexpr int32 EdgeIndices[24] = {
		0, 1, 1, 3, 3, 2, 2, 0,
		4, 5, 5, 7, 7, 6, 6, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	FRenderCommand AABBCmd = {};
	AABBCmd.Type = ERenderCommandType::DebugBox;
	AABBCmd.Constants.AABB.Color = Color;
	AABBCmd.Constants.AABB.VertexCount = 24;

	const FVector Extents = Box.Max - Box.Min;
	for (int32 VertexIndex = 0; VertexIndex < 24; ++VertexIndex)
	{
		const FVector& Corner = UnitCorners[EdgeIndices[VertexIndex]];
		AABBCmd.Constants.AABB.Vertices[VertexIndex] = FVector(
			Box.Min.X + (Corner.X * Extents.X),
			Box.Min.Y + (Corner.Y * Extents.Y),
			Box.Min.Z + (Corner.Z * Extents.Z));
	}
	RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}

void FRenderCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus)
{
	if (!ShowFlags.bBoundingVolume) return;

	const FAABB Box = BuildRenderAABB(PrimitiveComponent, RenderBus);
	CollectAABBCommand(Box, FColor::White(), RenderBus);
}

FFireBallInfo FRenderCollector::CollectFireBallInfoFromFireBallComponent(UFireBallComponent* InFireBallComponent)
{
	FVector WorldLoc = InFireBallComponent->GetWorldLocation();
	return FFireBallInfo(FVector4(WorldLoc.X, WorldLoc.Y, WorldLoc.Z, 1.0f), InFireBallComponent->GetColor(), InFireBallComponent->GetIntensity(), InFireBallComponent->GetRadius(), InFireBallComponent->GetRadiusFallOff());
}

FHeightFogInfo FRenderCollector::CollectHeightFogInfoFromFogComponent(UHeightFogComponent* InHeightFogComponent)
{	
	return FHeightFogInfo(InHeightFogComponent->GetWorldLocation() , InHeightFogComponent->GetFogDensity() ,
					InHeightFogComponent->GetFogHeightFalloff() , InHeightFogComponent->GetStartDistance() ,
					InHeightFogComponent->GetFogCutoffDistance() , InHeightFogComponent->GetFogMaxOpacity() , 
					InHeightFogComponent->GetFogInscatteringColor() , InHeightFogComponent->GetFogExist() );
}


