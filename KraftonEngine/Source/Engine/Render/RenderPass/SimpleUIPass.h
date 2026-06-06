#pragma once

#include "Render/RenderPass/RenderPassBase.h"

// 신규 계층형 UI 의 draw-only 렌더패스(진단 B4, 사이클 5).
// FUICanvasManager 가 보유한 Canvas 트리에서 사이클 3 레이아웃 패스가 캐시해 둔
// 화면 사각형(ScreenRect)만 읽어 단색 쿼드로 제출한다. 여기서 레이아웃은 하지 않는다.
// enum 순서상 RmlUi(ERenderPass::UI) 바로 다음이라 RmlUi 위에 그려진다.
class FSimpleUIPass final : public FRenderPassBase
{
public:
	FSimpleUIPass();

	bool BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
};
