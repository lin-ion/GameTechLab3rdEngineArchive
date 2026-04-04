#pragma once
#include <algorithm>
#include "Core/CoreTypes.h"
#include "Render/Pipeline/RenderCommand.h"
#include "Profiling/Stats.h"

namespace FRenderSorting
{
	inline void SortCommands(TArray<FRenderCommand>& Commands)
	{
		if (Commands.size() < 2) return;

		SCOPE_STAT("Render.Sort");
		std::sort(Commands.begin(), Commands.end(), [](const FRenderCommand& A, const FRenderCommand& B) {
			// 1. Shader 우선 정렬
			if (A.Shader != B.Shader)
				return A.Shader < B.Shader;

			// 2. Mesh Buffer 정렬
			if (A.MeshBuffer != B.MeshBuffer)
				return A.MeshBuffer < B.MeshBuffer;

			// 3. Texture(SRV) 정렬
			ID3D11ShaderResourceView* SrvA = A.SectionDraws.empty() ? nullptr : A.SectionDraws[0].DiffuseSRV;
			ID3D11ShaderResourceView* SrvB = B.SectionDraws.empty() ? nullptr : B.SectionDraws[0].DiffuseSRV;

			if (SrvA != SrvB)
				return SrvA < SrvB;

			// 모두 같으면 순서 유지 (Depth 등 추후 확장 가능)
			return false;
		});
	}
}
