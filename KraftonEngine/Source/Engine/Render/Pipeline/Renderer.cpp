#include "Renderer.h"
#include <algorithm>
#include "RenderStats.h"
#include "OcclusionManager.h"
#include "Resource/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Profiling/Stats.h"
#include "Engine/Runtime/Engine.h"
#include "Profiling/Timer.h"
#include "Viewport/Viewport.h"


void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		OutputDebugStringA("Failed to create D3D Device.\n");
	}

	FShaderManager::Get().Initialize(Device.GetDevice());
	FConstantBufferPool::Get().Initialize(Device.GetDevice());
	Resources.Create(Device.GetDevice());

	EditorLineBatcher.Create(Device.GetDevice());
	GridLineBatcher.Create(Device.GetDevice());
	FontBatcher.Create(Device.GetDevice());
	SubUVBatcher.Create(Device.GetDevice());

	InitializePassRenderStates();
	InitializePassBatchers();

	FOcclusionManager::Get().Initialize(Device.GetDevice());

}

void FRenderer::Release()
{
	FOcclusionManager::Get().Release();

	EditorLineBatcher.Release();
	GridLineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();

	Resources.Release();
	FConstantBufferPool::Get().Release();
	FShaderManager::Get().Release();
	Device.Release();
}

//	ViewContext에서 Batcher 데이터 수집 (CPU). BeginFrame 이전에 호출.
void FRenderer::PrepareBatchers(const FViewContext& ViewContext)
{
	// --- Editor 패스: AABB 디버그 박스 → EditorLineBatcher ---
	EditorLineBatcher.Clear();
	for (const auto& Entry : ViewContext.GetAABBEntries())
	{
		EditorLineBatcher.AddAABB(FBoundingBox{ Entry.AABB.Min, Entry.AABB.Max }, Entry.AABB.Color);
	}

	// --- Grid 패스: 월드 그리드 + 축 → GridLineBatcher ---
	GridLineBatcher.Clear();
	for (const auto& Proxy : ViewContext.GetGridProxies())
	{
		const FVector CameraPos = ViewContext.GetView().GetInverseFast().GetLocation();
		const FVector& CameraFwd = ViewContext.GetCameraForward();

		GridLineBatcher.AddWorldHelpers(
			ViewContext.GetShowFlags(),
			Proxy.Grid.GridSpacing,
			Proxy.Grid.GridHalfLineCount,
			CameraPos, CameraFwd, ViewContext.IsFixedOrtho());
	}

	// --- Font 패스: 월드 공간 텍스트 → FontBatcher ---
	FontBatcher.Clear();
	for (const auto& Entry : ViewContext.GetFontEntries())
	{
		if (!Entry.Font.Text.empty())
		{
			FontBatcher.AddText(
				Entry.Font.Text,
				Entry.PerObject.Model.GetLocation(),
				ViewContext.GetCameraRight(),
				ViewContext.GetCameraUp(),
				Entry.PerObject.Model.GetScale(),
				Entry.Font.Scale
			);
		}
	}

	// --- OverlayFont 패스: 스크린 공간 텍스트 → FontBatcher ---
	FontBatcher.ClearScreen();
	for (const auto& Entry : ViewContext.GetOverlayFontEntries())
	{
		if (!Entry.Font.Text.empty())
		{
			FontBatcher.AddScreenText(
				Entry.Font.Text,
				Entry.Font.ScreenPosition.X,
				Entry.Font.ScreenPosition.Y,
				ViewContext.GetViewportWidth(),
				ViewContext.GetViewportHeight(),
				Entry.Font.Scale
			);
		}
	}

	// --- SubUV 패스: 스프라이트 → SubUVBatcher (Particle SRV 기준 정렬) ---
	SubUVBatcher.Clear();
	{
		const auto& Entries = ViewContext.GetSubUVEntries();
		SortedSubUVBuffer.assign(Entries.begin(), Entries.end());

		if (SortedSubUVBuffer.size() > 1)
		{
			std::sort(SortedSubUVBuffer.begin(), SortedSubUVBuffer.end(),
				[](const FSubUVEntry& A, const FSubUVEntry& B) {
					return A.SubUV.Particle < B.SubUV.Particle;
				});
		}

		for (const auto& Entry : SortedSubUVBuffer)
		{
			if (Entry.SubUV.Particle)
			{
				SubUVBatcher.AddSprite(
					Entry.SubUV.Particle->SRV,
					Entry.PerObject.Model.GetLocation(),
					ViewContext.GetCameraRight(),
					ViewContext.GetCameraUp(),
					Entry.PerObject.Model.GetScale(),
					Entry.SubUV.FrameIndex,
					Entry.SubUV.Particle->Columns,
					Entry.SubUV.Particle->Rows,
					Entry.SubUV.Width,
					Entry.SubUV.Height
				);
			}
		}
	}
}

void FRenderer::RenderPicking(const FViewContext& InRenderBus, FViewport* InViewport)
{
	if (!InViewport) return;

	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	if (!Context) return;

	// ── Fix: 메인 렌더/HZB/OcclusionTest 이후 잔여 GPU 상태 초기화 ──
	//	NOTE : Depth 관련 정보 등을 초기화하지 않으면 픽킹 렌더링이 실패할 수 있음
	LastBoundMeshBuffer = nullptr;
	LastBoundShader = nullptr;
	LastBoundDiffuseSRV = nullptr;
	Device.ResetDepthStencilCache();

	InViewport->BeginPickingRender(Context);
	UpdateFrameBuffer(Context, InRenderBus);

	FShader* PickingShader = FShaderManager::Get().GetShader(EShaderType::Picking);
	if (!PickingShader) return;

	const ERenderPass PickPasses[] = { ERenderPass::Opaque, ERenderPass::GizmoOuter, ERenderPass::GizmoInner };
	Device.SetDepthStencilState(EDepthStencilState::Default);
	Device.SetBlendState(EBlendState::Opaque);
	Device.SetRasterizerState(ERasterizerState::SolidBackCull);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (ERenderPass Pass : PickPasses)
	{
		if (Pass == ERenderPass::GizmoInner)
		{
			Device.SetDepthStencilState(EDepthStencilState::GizmoInside);
		}
		else if (Pass == ERenderPass::GizmoOuter)
		{
			Device.SetDepthStencilState(EDepthStencilState::GizmoOutside);
		}
		else
		{
			Device.SetDepthStencilState(EDepthStencilState::Default);
		}

		const auto& Commands = InRenderBus.GetCommands(Pass);
		for (const FRenderCommand& Cmd : Commands)
		{
			if (!Cmd.MeshBuffer || !Cmd.MeshBuffer->IsValid() || Cmd.PickingId == 0u)
			{
				continue;
			}

			PickingShader->Bind(Context);

			Resources.PerObjectConstantBuffer.Update(Context, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
			{
				ID3D11Buffer* CB = Resources.PerObjectConstantBuffer.GetBuffer();
				Context->VSSetConstantBuffers(ECBSlot::PerObject, 1, &CB);
			}

			FPickingConstants PickingConstants = {};
			PickingConstants.PickingId = Cmd.PickingId;
			FConstantBuffer* PickingCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::Picking, sizeof(FPickingConstants));
			PickingCB->Update(Context, &PickingConstants, sizeof(FPickingConstants));
			{
				ID3D11Buffer* CB = PickingCB->GetBuffer();
				Context->PSSetConstantBuffers(ECBSlot::Picking, 1, &CB);
			}

			DrawCommand(Context, Cmd);
		}
	}
}

//	스왑체인 백버퍼 복귀 — ImGui 합성 직전에 호출
void FRenderer::BeginFrame()
{
	GRenderStatsSnapshot = GRenderStats;
	GRenderStats.Reset();
	LastBoundShader = nullptr;
	LastBoundMeshBuffer = nullptr;
	LastBoundDiffuseSRV = nullptr;

	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	ID3D11RenderTargetView* RTV = Device.GetFrameBufferRTV();
	ID3D11DepthStencilView* DSV = Device.GetDepthStencilView();

	Context->ClearRenderTargetView(RTV, Device.GetClearColor());
	Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

	const D3D11_VIEWPORT& Viewport = Device.GetViewport();
	Context->RSSetViewports(1, &Viewport);
	Context->OMSetRenderTargets(1, &RTV, DSV);
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FViewContext& InRenderBus)
{
	// 정렬을 위해 const 캐스트 (가장 저렴한 위치)
	FViewContext& MutableBus = const_cast<FViewContext&>(InRenderBus);

	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	UpdateFrameBuffer(Context, InRenderBus);

#ifdef FOR_COMPETITION
	// ── Large CB 준비 (Opaque 패스 전) ──
	// 1. Opaque 정렬
	{
		SCOPE_STAT("Render.Sort.Opaque");
		MutableBus.SortPass(ERenderPass::Opaque);
	}

	// 2. PerObject 데이터 수집 + 인덱스 부여
	{
		SCOPE_STAT("Render.CollectPerObjectData");
		CollectPerObjectData(MutableBus);
	}

	// 3. Large CB 용량 확보 + 1회 업로드
	uint32 largeCBCount = static_cast<uint32>(PerObjectDataArray.size());
	if (largeCBCount > 0)
	{
		SCOPE_STAT("Render.UpdateLargeCB");
		if (largeCBCount > Resources.LargeCBCapacity)
		{
			uint32 newCap = largeCBCount + (largeCBCount / 2);  // 1.5배 여유
			Resources.CreatePerObjectLargeCB(Device.GetDevice(), newCap);
		}
		Resources.UpdatePerObjectLargeCB(Context, PerObjectDataArray.data(), largeCBCount);
	}
#endif

	// ── 패스 루프 ──
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);

#ifdef FOR_COMPETITION
		// Opaque는 이미 위에서 정렬했으므로 스킵
		if (CurPass != ERenderPass::Opaque)
		{
			if (CurPass == ERenderPass::SelectionMask)
			{
				SCOPE_STAT("Render.Sort.Translucent");
				MutableBus.SortPass(CurPass);
			}
			if (CurPass == ERenderPass::Translucent)
			{
				SCOPE_STAT("Render.Sort.SelectionMask");
				MutableBus.SortPass(CurPass);
			}
		}
		#else
		if (CurPass == ERenderPass::Opaque || CurPass == ERenderPass::Translucent || CurPass == ERenderPass::SelectionMask)
		{
			MutableBus.SortPass(CurPass);
		}
#endif

		ApplyPassRenderState(CurPass, Context, InRenderBus.GetViewMode());

		if (PassBatchers[i])
		{
			PassBatchers[i].DrawBatch(CurPass, InRenderBus, Context);
		}
		else
		{
			const auto& Commands = InRenderBus.GetCommands(CurPass);
			if (!Commands.empty())
			{
#ifdef FOR_COMPETITION
				// Opaque 패스는 Large CB + Offset 방식 사용
				if (CurPass == ERenderPass::Opaque && largeCBCount > 0)
				{
					SCOPE_STAT("Render.OpaquePass");
					ExecuteOpaquePassWithLargeCB(Commands, InRenderBus, Context);
				}
				else
#endif
				{
					ExecuteDefaultPass(Commands, InRenderBus, Context);
				}
			}
		}

		if (CurPass == ERenderPass::Opaque)
		{
			// DX11 Conflict: Depth buffer cannot be bound as both DSV and SRV.
			// Unbind all RTVs and DSV before building HZB.
			ID3D11RenderTargetView* nullRTV = nullptr;
			Context->OMSetRenderTargets(1, &nullRTV, nullptr);

			FOcclusionManager::Get().BuildHZB(Context, InRenderBus.GetViewportDepthSRV(), static_cast<uint32>(InRenderBus.GetViewportWidth()), static_cast<uint32>(InRenderBus.GetViewportHeight()));

			// Rebind the viewport RTV and DSV for subsequent passes (Translucent, etc.)
			ID3D11RenderTargetView* RTV = InRenderBus.GetViewportRTV();
			ID3D11DepthStencilView* DSV = InRenderBus.GetViewportDSV();
			Context->OMSetRenderTargets(1, &RTV, DSV);
		}
	}
}

#ifdef FOR_COMPETITION
// Large CB: PerObject 데이터 수집
void FRenderer::CollectPerObjectData(FViewContext& ViewContext)
{
	PerObjectDataArray.clear();

	auto& Commands = ViewContext.GetCommandsMutable(ERenderPass::Opaque);
	for (FRenderCommand& Cmd : Commands)
	{
		if (Cmd.SectionDraws.empty())
		{
			Cmd.PerObjectBaseIndex = static_cast<uint32>(PerObjectDataArray.size());

			FPerObjectAligned aligned = {};
			aligned.Model = Cmd.PerObjectConstants.Model;
			aligned.Color = Cmd.PerObjectConstants.Color;
			PerObjectDataArray.push_back(aligned);
		}
		else
		{
			Cmd.PerObjectBaseIndex = static_cast<uint32>(PerObjectDataArray.size());
			for (const FMeshSectionDraw& Section : Cmd.SectionDraws)
			{
				FPerObjectAligned aligned = {};
				aligned.Model = Cmd.PerObjectConstants.Model;
				aligned.Color = Section.DiffuseColor;
				PerObjectDataArray.push_back(aligned);
			}
		}
	}
}

// Large CB: Opaque 패스 실행기 — VSSetConstantBuffers1 오프셋 바인딩
void FRenderer::ExecuteOpaquePassWithLargeCB(
    const TArray<FRenderCommand>& Commands,
    const FViewContext& Bus,
    ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext1* Context1 = Device.GetDeviceContext1();
	if (!Context1)
	{
		// 폴백: 기존 방식
		ExecuteDefaultPass(Commands, Bus, Context);
		return;
	}

	for (const auto& Cmd : Commands)
	{
		BindCommandShaderOnly(Cmd, Context);

		if (!Cmd.SectionDraws.empty())
		{
			DrawStaticMeshSectionsWithLargeCB(Context1, Cmd);
		}
		else
		{
			// 오프셋 바인딩 (Map/Unmap 없음)
			UINT firstConstant = Cmd.PerObjectBaseIndex * 16;  // 256B / 16B = 16
			UINT numConstants  = 16;
			Context1->VSSetConstantBuffers1(ECBSlot::PerObject, 1, &Resources.PerObjectLargeCB, &firstConstant, &numConstants);

			DrawCommand(Context, Cmd);
		}
	}
}

// Large CB: 셰이더 + ExtraCB만 바인딩 (PerObject CB 업데이트 제거)
void FRenderer::BindCommandShaderOnly(const FRenderCommand& InCmd, ID3D11DeviceContext* Context)
{
	if (InCmd.Shader)
	{
		if (InCmd.Shader != LastBoundShader)
		{
			++GRenderStats.ShaderBinds;
			InCmd.Shader->Bind(Context);
			LastBoundShader = InCmd.Shader;
		}
		else
		{
			++GRenderStats.RedundantShaderBinds;
		}
	}

	if (InCmd.ExtraCB.Buffer)
	{
		InCmd.ExtraCB.Buffer->Update(Context, InCmd.ExtraCB.Data, InCmd.ExtraCB.Size);
		ID3D11Buffer* cb = InCmd.ExtraCB.Buffer->GetBuffer();
		Context->VSSetConstantBuffers(InCmd.ExtraCB.Slot, 1, &cb);
		Context->PSSetConstantBuffers(InCmd.ExtraCB.Slot, 1, &cb);
	}
}

// Large CB: 섹션별 드로우 (오프셋 바인딩)
void FRenderer::DrawStaticMeshSectionsWithLargeCB(
    ID3D11DeviceContext1* Context1, const FRenderCommand& Cmd)
{
	ID3D11DeviceContext* Context = Context1;

	if (!Cmd.MeshBuffer || !Cmd.MeshBuffer->IsValid()) return;

	// VB/IB 바인딩 (캐싱 유지)
	if (Cmd.MeshBuffer != LastBoundMeshBuffer)
	{
		++GRenderStats.MeshBinds;
		uint32 offset = 0;
		ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
		if (!vertexBuffer) return;
		uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
		Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

		ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
		if (!indexBuffer) return;
		Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

		LastBoundMeshBuffer = Cmd.MeshBuffer;
	}
	else
	{
		++GRenderStats.RedundantMeshBinds;
	}

	Context->PSSetSamplers(0, 1, &Resources.DefaultSampler);

	UINT numConstants = 16;  // 256B / 16B

	uint32 sectionIdx = 0;
	for (const FMeshSectionDraw& Section : Cmd.SectionDraws)
	{
		if (Section.IndexCount == 0) continue;

		// 섹션별 SRV 바인딩 (캐싱 유지)
		if (Section.DiffuseSRV != LastBoundDiffuseSRV)
		{
			++GRenderStats.SRVChanges;
			ID3D11ShaderResourceView* srv = Section.DiffuseSRV;
			Context->PSSetShaderResources(0, 1, &srv);
			LastBoundDiffuseSRV = Section.DiffuseSRV;
		}
		else
		{
			++GRenderStats.RedundantSRVChanges;
		}

		// 오프셋 바인딩만 — Map/Unmap 없음
		UINT firstConstant = (Cmd.PerObjectBaseIndex + sectionIdx) * 16;
		Context1->VSSetConstantBuffers1(ECBSlot::PerObject, 1, &Resources.PerObjectLargeCB, &firstConstant, &numConstants);

		Context->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
		++sectionIdx;
		++GRenderStats.DrawCalls;
		++GRenderStats.DrawIndexedCalls;
		GRenderStats.TrianglesRendered += Section.IndexCount / 3;
	}
}
#endif // FOR_COMPETITION

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
	using E = ERenderPass;
	auto& S = PassRenderStates;

	//                              DepthStencil                    Blend                Rasterizer                   Topology                                WireframeAware
	S[(uint32)E::Opaque] = { EDepthStencilState::Default,      EBlendState::Opaque,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::Translucent] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::SelectionMask] = { EDepthStencilState::StencilWrite,  EBlendState::NoColor,    ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::PostProcess] = { EDepthStencilState::NoDepth,       EBlendState::AlphaBlend, ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::Editor] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     true };
	S[(uint32)E::Grid] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     false };
	S[(uint32)E::GizmoOuter] = { EDepthStencilState::GizmoOutside, EBlendState::Opaque,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::GizmoInner] = { EDepthStencilState::GizmoInside,  EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::Font] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
	S[(uint32)E::OverlayFont] = { EDepthStencilState::NoDepth,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
	S[(uint32)E::SubUV] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}

// ============================================================
// Pass Batcher DrawBatch 바인딩 초기화
// ============================================================
void FRenderer::InitializePassBatchers()
{
	PassBatchers[(uint32)ERenderPass::Editor] = {
		[this](ERenderPass, const FViewContext&, ID3D11DeviceContext* Ctx) {
			DrawLineBatcher(EditorLineBatcher, Ctx);
		}
	};

	PassBatchers[(uint32)ERenderPass::Grid] = {
		[this](ERenderPass, const FViewContext&, ID3D11DeviceContext* Ctx) {
			DrawLineBatcher(GridLineBatcher, Ctx);
		}
	};

	PassBatchers[(uint32)ERenderPass::Font] = {
		[this](ERenderPass, const FViewContext&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.DrawBatch(Ctx, FontRes);
		}
	};

	PassBatchers[(uint32)ERenderPass::OverlayFont] = {
		[this](ERenderPass, const FViewContext&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.DrawScreenBatch(Ctx, FontRes);
		}
	};

	PassBatchers[(uint32)ERenderPass::SubUV] = {
		[this](ERenderPass, const FViewContext&, ID3D11DeviceContext* Ctx) {
			SubUVBatcher.DrawBatch(Ctx);
		}
	};

	PassBatchers[(uint32)ERenderPass::PostProcess] = {
		[this](ERenderPass Pass, const FViewContext& Bus, ID3D11DeviceContext* Ctx) {
			DrawPostProcessOutline(Bus, Ctx);
		}
	};
}

// ============================================================
// LineBatcher DrawBatch 공통
// ============================================================
void FRenderer::DrawLineBatcher(FLineBatcher& Batcher, ID3D11DeviceContext* Context)
{
	if (Batcher.GetLineCount() == 0) return;

	FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);
	if (EditorShader) EditorShader->Bind(Context);

	Batcher.DrawBatch(Context);
}

// ============================================================
// 기본 패스 실행기
// ============================================================
void FRenderer::ExecuteDefaultPass(const TArray<FRenderCommand>& Commands, const FViewContext& Bus, ID3D11DeviceContext* Context)
{
	for (const auto& Cmd : Commands)
	{
		BindCommand(Cmd, Context);

		// StaticMesh: 섹션별 SRV 바인딩 + 분할 드로우
		if (!Cmd.SectionDraws.empty())
		{
			DrawStaticMeshSections(Context, Cmd);
		}
		else
		{
			// 섹션이 없는 단일 메쉬 드로우인 경우
			// BindCommand에서 PerObjectCB가 업데이트 되었으므로 그냥 그리면 됨
			DrawCommand(Context, Cmd);
		}
	}
}

void FRenderer::ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode CurViewMode)
{
	const FPassRenderState& State = PassRenderStates[(uint32)Pass];

	ERasterizerState Rasterizer = State.Rasterizer;
	if (State.bWireframeAware && CurViewMode == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerState::WireFrame;
	}

	Device.SetDepthStencilState(State.DepthStencil);
	Device.SetBlendState(State.Blend);
	Device.SetRasterizerState(Rasterizer);
	Context->IASetPrimitiveTopology(State.Topology);
}

// ============================================================
// 커맨드 바인딩 — 셰이더 + PerObject CB + Extra CB (데이터 드리븐)
// ============================================================
void FRenderer::BindCommand(const FRenderCommand& InCmd, ID3D11DeviceContext* Context)
{
	// 커맨드가 지정한 셰이더 바인딩
	if (InCmd.Shader)
	{
		if (InCmd.Shader != LastBoundShader)
		{
			++GRenderStats.ShaderBinds;
			InCmd.Shader->Bind(Context);
			LastBoundShader = InCmd.Shader;
		}
		else
		{
			++GRenderStats.RedundantShaderBinds;
		}
	}

	// [CB 최적화] SectionDraws가 있는 메쉬는 DrawStaticMeshSections 내부에서 
	// 각 Section별로 Color가 적용된 PerObjectCB를 업로드합니다.
	// 따라서 여기서 공통 PerObjectCB를 성급히 업로드하면 프레임당 50000번의 완벽한 낭비(Redundant)가 발생합니다.
	if (InCmd.SectionDraws.empty())
	{
		Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));
		ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
		Context->VSSetConstantBuffers(ECBSlot::PerObject, 1, &cb);
	}

	// Extra CB — ExtraCB.Data를 지정 슬롯에 업로드
	if (InCmd.ExtraCB.Buffer)
	{
		InCmd.ExtraCB.Buffer->Update(Context, InCmd.ExtraCB.Data, InCmd.ExtraCB.Size);
		ID3D11Buffer* cb = InCmd.ExtraCB.Buffer->GetBuffer();
		Context->VSSetConstantBuffers(InCmd.ExtraCB.Slot, 1, &cb);
		Context->PSSetConstantBuffers(InCmd.ExtraCB.Slot, 1, &cb);
	}
}

void FRenderer::DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr || !InCommand.MeshBuffer->IsValid())
	{
		return;
	}

	// 버텍스 버퍼 바인딩 (캐싱 적용)
	if (InCommand.MeshBuffer != LastBoundMeshBuffer)
	{
		++GRenderStats.MeshBinds;
		uint32 offset = 0;
		ID3D11Buffer* vertexBuffer = InCommand.MeshBuffer->GetVertexBuffer().GetBuffer();
		if (vertexBuffer == nullptr)
		{
			return;
		}

		uint32 stride = InCommand.MeshBuffer->GetVertexBuffer().GetStride();
		if (stride == 0)
		{
			return;
		}

		InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

		ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
		if (indexBuffer != nullptr)
		{
			InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		}

		LastBoundMeshBuffer = InCommand.MeshBuffer;
	}
	else
	{
		++GRenderStats.RedundantMeshBinds;
	}

	uint32 vertexCount = InCommand.MeshBuffer->GetVertexBuffer().GetVertexCount();
	ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
	if (indexBuffer != nullptr)
	{
		uint32 indexCount = InCommand.MeshBuffer->GetIndexBuffer().GetIndexCount();
		InDeviceContext->DrawIndexed(indexCount, 0, 0);
		++GRenderStats.DrawCalls;
		++GRenderStats.DrawIndexedCalls;
		GRenderStats.TrianglesRendered += indexCount / 3;
	}
	else
	{
		InDeviceContext->Draw(vertexCount, 0);
		++GRenderStats.DrawCalls;
		++GRenderStats.DrawVertexCalls;
		GRenderStats.TrianglesRendered += vertexCount / 3;
	}
}

void FRenderer::DrawStaticMeshSections(ID3D11DeviceContext* Context, const FRenderCommand& Cmd)
{
	if (!Cmd.MeshBuffer || !Cmd.MeshBuffer->IsValid()) return;

	// 버텍스 버퍼 바인딩 (캐싱 적용)
	if (Cmd.MeshBuffer != LastBoundMeshBuffer)
	{
		++GRenderStats.MeshBinds;
		uint32 offset = 0;
		ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
		if (!vertexBuffer) return;
		uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
		Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

		ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
		if (!indexBuffer) return;
		Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

		LastBoundMeshBuffer = Cmd.MeshBuffer;
	}
	else
	{
		++GRenderStats.RedundantMeshBinds;
	}

	// StaticMeshShader가 s0에 SamplerState를 요구 (캐싱 가능하나 일단 유지)
	Context->PSSetSamplers(0, 1, &Resources.DefaultSampler);

	for (const FMeshSectionDraw& Section : Cmd.SectionDraws)
	{
		if (Section.IndexCount == 0) continue;

		// 섹션별 SRV 바인딩 (캐싱 적용)
		if (Section.DiffuseSRV != LastBoundDiffuseSRV)
		{
			++GRenderStats.SRVChanges;
			ID3D11ShaderResourceView* srv = Section.DiffuseSRV;
			Context->PSSetShaderResources(0, 1, &srv);
			LastBoundDiffuseSRV = Section.DiffuseSRV;
		}
		else
		{
			++GRenderStats.RedundantSRVChanges;
		}

		// 섹션별 DiffuseColor를 PrimitiveColor(b1)에 반영
		FPerObjectConstants SectionConstants = Cmd.PerObjectConstants;
		SectionConstants.Color = Section.DiffuseColor;
		Resources.PerObjectConstantBuffer.Update(Context, &SectionConstants, sizeof(FPerObjectConstants));
		ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
		Context->VSSetConstantBuffers(ECBSlot::PerObject, 1, &cb);

		Context->DrawIndexed(Section.IndexCount, Section.FirstIndex, 0);
		++GRenderStats.DrawCalls;
		++GRenderStats.DrawIndexedCalls;
		GRenderStats.TrianglesRendered += Section.IndexCount / 3;
	}
}

// ============================================================
// PostProcess Outline — DSV unbind → StencilSRV bind → Fullscreen Draw
// ============================================================
void FRenderer::DrawPostProcessOutline(const FViewContext& Bus, ID3D11DeviceContext* Context)
{
	ID3D11ShaderResourceView* StencilSRV = Bus.GetViewportStencilSRV();
	ID3D11DepthStencilView* DSV = Bus.GetViewportDSV();
	ID3D11RenderTargetView* RTV = Bus.GetViewportRTV();
	if (!StencilSRV || !RTV) return;

	// SelectionMask 큐가 비어 있으면 선택된 오브젝트 없음 → 스킵
	if (Bus.GetCommands(ERenderPass::SelectionMask).empty()) return;

	// 1) DSV 언바인딩 (StencilSRV와 동시 바인딩 불가)
	Context->OMSetRenderTargets(1, &RTV, nullptr);

	// 2) StencilSRV → PS t0 바인딩
	Context->PSSetShaderResources(0, 1, &StencilSRV);

	// 3) PostProcess 셰이더 바인딩
	FShader* PPShader = FShaderManager::Get().GetShader(EShaderType::OutlinePostProcess);
	if (PPShader) PPShader->Bind(Context);

	// 4) Outline CB (b3) 업데이트
	FConstantBuffer* OutlineCB = FConstantBufferPool::Get().GetBuffer(ECBSlot::PostProcess, sizeof(FOutlinePostProcessConstants));
	FOutlinePostProcessConstants PPConstants;
	PPConstants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
	PPConstants.OutlineThickness = 3.0f;
	OutlineCB->Update(Context, &PPConstants, sizeof(PPConstants));
	ID3D11Buffer* cb = OutlineCB->GetBuffer();
	Context->PSSetConstantBuffers(ECBSlot::PostProcess, 1, &cb);

	// 5) Fullscreen Triangle 드로우 (vertex buffer 없이 SV_VertexID 사용)
	Context->IASetInputLayout(nullptr);
	Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	Context->Draw(3, 0);
	++GRenderStats.DrawCalls;
	++GRenderStats.DrawVertexCalls;

	// 6) StencilSRV 언바인딩
	ID3D11ShaderResourceView* nullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &nullSRV);

	// 7) DSV 재바인딩 (후속 패스에서 뎁스 사용)
	Context->OMSetRenderTargets(1, &RTV, DSV);
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
	Device.Present();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FViewContext& InRenderBus)
{
	FFrameConstants frameConstantData = {};
	frameConstantData.View = InRenderBus.GetView();
	frameConstantData.Projection = InRenderBus.GetProj();
	frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
	frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();

	if (GEngine && GEngine->GetTimer())
	{
		frameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
	}

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
	Context->PSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
}
