#include "UI/Canvas/UILabel.h"

#include "UI/UIManager.h"
#include "UI/UserWidget.h"

#include <string>

namespace
{
	// RmlUi 라벨 템플릿(루트 상대 경로). LoadDocument 가 존재 여부를 확인한다.
	constexpr const char* LabelDocPath = "Content/UI/SimpleUILabel.rml";

	FString ToDp(float Value)
	{
		return FString(std::to_string(Value)) + "dp";
	}
}

void UUILabel::OnLayoutUpdated(float GlobalScale)
{
	// 최초 1회 RmlUi 위젯 생성 + viewport 등록. 높은 ZOrder 로 RmlUi 레이어 내 위쪽에 둔다.
	if (!Widget.Get() && !bMountAttempted)
	{
		bMountAttempted = true;
		if (UUserWidget* NewWidget = UUIManager::Get().CreateWidget(nullptr, LabelDocPath))
		{
			NewWidget->AddToViewport(1000);
			Widget = NewWidget;
		}
	}

	UUserWidget* W = Widget.Get();
	if (!W)
	{
		return;
	}

	// ScreenRect(스크린 px) → 레퍼런스 dp 로 환산. dp_ratio = GlobalScale 라
	// dp 좌표가 신규 UI 의 ScreenRect 와 1:1 로 정합한다.
	const float Scale = (GlobalScale > 0.0f) ? GlobalScale : 1.0f;
	const FUIRect& R = GetScreenRect();

	W->SetText("label", Text);
	W->SetProperty("label", "left", ToDp(R.Pos.X / Scale));
	W->SetProperty("label", "top", ToDp(R.Pos.Y / Scale));
}

void UUILabel::BeginDestroy()
{
	if (UUserWidget* W = Widget.Get())
	{
		UUIManager::Get().RemoveFromViewport(W);
	}
	Super::BeginDestroy();
}
