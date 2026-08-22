#pragma once

#include "Render/Common/ViewTypes.h"
#include "Render/Scene/RenderCommand.h"

struct FOutlinePostProcessData
{
    bool              bEnabled = false;
    FOutlineConstants Constants = {};
};

struct FPostProcessViewDesc
{
    int32 X = 0;
    int32 Y = 0;
    int32 Width = 0;
    int32 Height = 0;

    // RenderBus에서 snapshot한 viewport별 실행 정보
    EViewMode  ViewMode = EViewMode::Lit;

    FShowFlags ShowFlags = {};
    FMatrix    View = FMatrix::Identity;
    FMatrix    Proj = FMatrix::Identity;
    float      NearPlane = 0.1f;
    float      FarPlane = 1000.0f;


	TArray<FFireBallInfo> FireBallInfoArray;
	FHeightFogInfo HeightFogInfo;

    // Selection 마스크 수집 결과에서 파생된 outline 정보
    FOutlinePostProcessData Outline = {};
};
