#include "Editor/Viewport/FLevelViewportLayoutUI.h"
#include "Editor/Viewport/FLevelViewportLayoutUI.h"

#include <algorithm>

#include "Editor/EditorEngine.h"
#include "Editor/Gizmo/TransformGizmo.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/FLevelViewportLayout.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "ImGui/imgui.h"
#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "Render/Pipeline/Renderer.h"
#include "UI/SSplitter.h"
#include "Viewport/Viewport.h"
#include "WICTextureLoader.h"

namespace
{
	enum class EViewportToolbarIcon : int32
	{
		Menu = 0,
		Select,
		Translate,
		Rotate,
		Scale,
		WorldSpace,
		LocalSpace,
		TranslateSnap,
		RotateSnap,
		ScaleSnap,
		Camera,
		Setting,
		Count
	};

	enum class EToolbarButtonShape : uint8
	{
		Normal,
		PairFirst,
		PairSecond
	};

	ID3D11ShaderResourceView* GetViewportToolbarIcon(EViewportToolbarIcon Icon);
	bool DrawToolbarTextButton(const char* Id, const char* Label, EToolbarButtonShape Shape = EToolbarButtonShape::Normal);

	static ID3D11ShaderResourceView* GViewportToolbarIcons[static_cast<int32>(EViewportToolbarIcon::Count)] = {};
	static bool GViewportToolbarIconsLoaded = false;

	const wchar_t* GetViewportToolbarIconFileName(EViewportToolbarIcon Icon)
	{
		switch (Icon)
		{
		case EViewportToolbarIcon::Menu:          return L"Menu.png";
		case EViewportToolbarIcon::Select:        return L"Select.png";
		case EViewportToolbarIcon::Translate:     return L"Translate.png";
		case EViewportToolbarIcon::Rotate:        return L"Rotate.png";
		case EViewportToolbarIcon::Scale:         return L"Scale.png";
		case EViewportToolbarIcon::WorldSpace:    return L"WorldSpace.png";
		case EViewportToolbarIcon::LocalSpace:    return L"LocalSpace.png";
		case EViewportToolbarIcon::TranslateSnap: return L"Translate_Snap.png";
		case EViewportToolbarIcon::RotateSnap:    return L"Rotate_Snap.png";
		case EViewportToolbarIcon::ScaleSnap:     return L"Scale_Snap.png";
		case EViewportToolbarIcon::Camera:        return L"Camera.png";
		case EViewportToolbarIcon::Setting:       return L"Setting.png";
		default:                                  return L"";
		}
	}

	ImVec2 GetToolbarIconRenderSize(EViewportToolbarIcon Icon, float FallbackSize, float MaxIconSize)
	{
		ID3D11ShaderResourceView* IconSRV = GetViewportToolbarIcon(Icon);
		if (!IconSRV)
		{
			return ImVec2(FallbackSize, FallbackSize);
		}

		ID3D11Resource* Resource = nullptr;
		IconSRV->GetResource(&Resource);
		if (!Resource)
		{
			return ImVec2(FallbackSize, FallbackSize);
		}

		ImVec2 IconSize(FallbackSize, FallbackSize);
		D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
		Resource->GetType(&Dimension);
		if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		{
			ID3D11Texture2D* Texture2D = static_cast<ID3D11Texture2D*>(Resource);
			D3D11_TEXTURE2D_DESC Desc{};
			Texture2D->GetDesc(&Desc);
			IconSize = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
		}
		Resource->Release();

		if (IconSize.x > MaxIconSize || IconSize.y > MaxIconSize)
		{
			const float Scale = (IconSize.x > IconSize.y) ? (MaxIconSize / IconSize.x) : (MaxIconSize / IconSize.y);
			IconSize.x *= Scale;
			IconSize.y *= Scale;
		}

		if (IconSize.x < 1.0f || IconSize.y < 1.0f)
		{
			return ImVec2(FallbackSize, FallbackSize);
		}

		return IconSize;
	}

	void EnsureViewportToolbarIconsLoaded(FLevelViewportLayout& Layout)
	{
		if (GViewportToolbarIconsLoaded || !Layout.GetRenderer())
		{
			return;
		}

		ID3D11Device* Device = Layout.GetRenderer()->GetFD3DDevice().GetDevice();
		if (!Device)
		{
			return;
		}

		const std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icon/ViewportToolBar/");
		for (int32 i = 0; i < static_cast<int32>(EViewportToolbarIcon::Count); ++i)
		{
			const std::wstring FilePath = IconDir + GetViewportToolbarIconFileName(static_cast<EViewportToolbarIcon>(i));
			DirectX::CreateWICTextureFromFile(Device, FilePath.c_str(), nullptr, &GViewportToolbarIcons[i]);
		}

		GViewportToolbarIconsLoaded = true;
	}

	ID3D11ShaderResourceView* GetViewportToolbarIcon(EViewportToolbarIcon Icon)
	{
		return GViewportToolbarIcons[static_cast<int32>(Icon)];
	}

	ImDrawFlags GetToolbarButtonRoundFlags(EToolbarButtonShape Shape)
	{
		switch (Shape)
		{
		case EToolbarButtonShape::PairFirst:
			return ImDrawFlags_RoundCornersLeft;
		case EToolbarButtonShape::PairSecond:
			return ImDrawFlags_RoundCornersRight;
		default:
			return ImDrawFlags_RoundCornersAll;
		}
	}

	bool DrawToolbarTextButton(const char* Id, const char* Label, EToolbarButtonShape Shape)
	{
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		const ImVec2 Padding = ImGui::GetStyle().FramePadding;
		const ImVec2 ButtonSize(TextSize.x + Padding.x * 2.0f, TextSize.y + Padding.y * 2.0f);
		const bool bPressed = ImGui::InvisibleButton(Id, ButtonSize);

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();
		const ImU32 BgColor = ImGui::GetColorU32(
			bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
		ImGui::GetWindowDrawList()->AddRectFilled(
			Min,
			Max,
			BgColor,
			ImGui::GetStyle().FrameRounding,
			GetToolbarButtonRoundFlags(Shape));

		const ImVec2 TextPos(
			Min.x + (ButtonSize.x - TextSize.x) * 0.5f,
			Min.y + (ButtonSize.y - TextSize.y) * 0.5f);
		const ImU32 TextColor = ImGui::GetColorU32(ImGuiCol_Text);
		ImGui::GetWindowDrawList()->AddText(TextPos, TextColor, Label);
		ImGui::GetWindowDrawList()->AddText(ImVec2(TextPos.x + 0.8f, TextPos.y), TextColor, Label);
		return bPressed;
	}

	bool DrawToolbarIconButton(const char* Id, EViewportToolbarIcon Icon, const char* FallbackLabel, float FallbackIconSize, float MaxIconSize, EToolbarButtonShape Shape = EToolbarButtonShape::Normal)
	{
		if (ID3D11ShaderResourceView* IconSRV = GetViewportToolbarIcon(Icon))
		{
			const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackIconSize, MaxIconSize);
			const ImVec2 Padding = ImGui::GetStyle().FramePadding;
			const ImVec2 ButtonSize(IconSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
			const bool bPressed = ImGui::InvisibleButton(Id, ButtonSize);
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();
			const bool bHovered = ImGui::IsItemHovered();
			const bool bHeld = ImGui::IsItemActive();
			const ImU32 BgColor = ImGui::GetColorU32(
				bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
			ImGui::GetWindowDrawList()->AddRectFilled(
				Min,
				Max,
				BgColor,
				ImGui::GetStyle().FrameRounding,
				GetToolbarButtonRoundFlags(Shape));
			ImGui::GetWindowDrawList()->AddImage(
				(ImTextureID)IconSRV,
				ImVec2(Min.x + Padding.x, Min.y + (ButtonSize.y - IconSize.y) * 0.5f),
				ImVec2(Min.x + Padding.x + IconSize.x, Min.y + (ButtonSize.y + IconSize.y) * 0.5f));
			return bPressed;
		}
		return DrawToolbarTextButton(Id, FallbackLabel, Shape);
	}

	bool DrawToolbarImageButton(const char* Id, ID3D11ShaderResourceView* ImageSRV, const char* FallbackLabel, float FallbackIconSize, float MaxIconSize, EToolbarButtonShape Shape = EToolbarButtonShape::Normal)
	{
		if (!ImageSRV)
		{
			return DrawToolbarTextButton(Id, FallbackLabel, Shape);
		}

		ID3D11Resource* Resource = nullptr;
		ImageSRV->GetResource(&Resource);
		ImVec2 IconSize(FallbackIconSize, FallbackIconSize);
		if (Resource)
		{
			D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
			Resource->GetType(&Dimension);
			if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
			{
				D3D11_TEXTURE2D_DESC Desc{};
				static_cast<ID3D11Texture2D*>(Resource)->GetDesc(&Desc);
				IconSize = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
			}
			Resource->Release();
		}
		if (IconSize.x > MaxIconSize || IconSize.y > MaxIconSize)
		{
			const float Scale = (IconSize.x > IconSize.y) ? (MaxIconSize / IconSize.x) : (MaxIconSize / IconSize.y);
			IconSize.x *= Scale;
			IconSize.y *= Scale;
		}

		const ImVec2 Padding = ImGui::GetStyle().FramePadding;
		const ImVec2 ButtonSize(IconSize.x + Padding.x * 2.0f, ImGui::GetFrameHeight());
		const bool bPressed = ImGui::InvisibleButton(Id, ButtonSize);
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();
		const ImU32 BgColor = ImGui::GetColorU32(
			bHeld ? ImGuiCol_ButtonActive : (bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
		ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, BgColor, ImGui::GetStyle().FrameRounding, GetToolbarButtonRoundFlags(Shape));
		ImGui::GetWindowDrawList()->AddImage(
			(ImTextureID)ImageSRV,
			ImVec2(Min.x + Padding.x, Min.y + (ButtonSize.y - IconSize.y) * 0.5f),
			ImVec2(Min.x + Padding.x + IconSize.x, Min.y + (ButtonSize.y + IconSize.y) * 0.5f));
		return bPressed;
	}
}

void FLevelViewportLayoutUI::RenderViewportUI(FLevelViewportLayout& Layout, float DeltaTime)
{
	Layout.bMouseOverViewport = false;
	Layout.TickLayoutTransition(DeltaTime);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_None);

	const ImVec2 ContentPos = ImGui::GetCursorScreenPos();
	const ImVec2 ContentSize = ImGui::GetContentRegionAvail();

	if (ContentSize.x > 0 && ContentSize.y > 0)
	{
		FRect ContentRect = { ContentPos.x, ContentPos.y, ContentSize.x, ContentSize.y };
		const int32 OnePaneSlotIndex = (Layout.CurrentLayout == EViewportLayout::OnePane && Layout.bIsTemporaryOnePane)
			? (std::max)(0, (std::min)(Layout.TemporaryOnePaneSourceSlot, static_cast<int32>(Layout.LevelViewportClients.size()) - 1))
			: 0;
		const bool bCoverToOnePane =
			Layout.bUseCoverTransitionToOnePane
			&& Layout.LayoutTransitionState == FLevelViewportLayout::ELayoutTransitionState::CollapsingCurrent
			&& Layout.PendingTargetLayout == EViewportLayout::OnePane;
		const bool bCoverFromOnePane =
			Layout.bUseCoverTransitionFromOnePane
			&& Layout.LayoutTransitionState == FLevelViewportLayout::ELayoutTransitionState::ExpandingTarget
			&& Layout.PendingTargetLayout != EViewportLayout::OnePane;
		const bool bRenderViewportOverlayUI = !bCoverToOnePane;

		if (Layout.RootSplitter)
		{
			Layout.RootSplitter->ComputeLayout(ContentRect);
		}
		else if (OnePaneSlotIndex < FLevelViewportLayout::MaxViewportSlots && Layout.ViewportWindows[OnePaneSlotIndex])
		{
			Layout.ViewportWindows[OnePaneSlotIndex]->SetRect(ContentRect);
		}

		for (int32 i = 0; i < Layout.ActiveSlotCount; ++i)
		{
			const int32 SlotIndex = (Layout.CurrentLayout == EViewportLayout::OnePane) ? OnePaneSlotIndex : i;
			if (SlotIndex < static_cast<int32>(Layout.LevelViewportClients.size()))
			{
				FLevelEditorViewportClient* VC = Layout.LevelViewportClients[SlotIndex];
				VC->UpdateLayoutRect();
				const bool bIsPIEFocusViewport = Layout.bPIEViewportMode && VC == Layout.PIEFocusedViewportClient;
				VC->RenderViewportImage(VC == Layout.ActiveViewportClient, !bIsPIEFocusViewport);
			}
		}

		for (int32 i = 0; bRenderViewportOverlayUI && i < Layout.ActiveSlotCount; ++i)
		{
			const int32 SlotIndex = (Layout.CurrentLayout == EViewportLayout::OnePane) ? OnePaneSlotIndex : i;
			if (Layout.bPIEViewportMode
				&& SlotIndex >= 0
				&& SlotIndex < static_cast<int32>(Layout.LevelViewportClients.size())
				&& Layout.LevelViewportClients[SlotIndex] == Layout.PIEFocusedViewportClient)
			{
				continue;
			}
			RenderPaneToolbar(Layout, SlotIndex);
		}

		if (bRenderViewportOverlayUI)
		{
			RenderActiveViewportStatOverlay(Layout);
		}

		if (Layout.RootSplitter)
		{
			TArray<SSplitter*> AllSplitters;
			SSplitter::CollectSplitters(Layout.RootSplitter, AllSplitters);

			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			const ImU32 BarColor = IM_COL32(80, 80, 80, 255);

			for (SSplitter* S : AllSplitters)
			{
				const FRect& Bar = S->GetSplitBarRect();
				DrawList->AddRectFilled(
					ImVec2(Bar.X, Bar.Y),
					ImVec2(Bar.X + Bar.Width, Bar.Y + Bar.Height),
					BarColor);
			}
		}

		if (bCoverToOnePane
			&& Layout.TransitionFocusSlot >= 0
			&& Layout.TransitionFocusSlot < static_cast<int32>(Layout.LevelViewportClients.size())
			&& Layout.TransitionFocusSlot < FLevelViewportLayout::MaxViewportSlots
			&& Layout.ViewportWindows[Layout.TransitionFocusSlot]
			&& Layout.LevelViewportClients[Layout.TransitionFocusSlot]
			&& Layout.LevelViewportClients[Layout.TransitionFocusSlot]->GetViewport()
			&& Layout.LevelViewportClients[Layout.TransitionFocusSlot]->GetViewport()->GetSRV())
		{
			const FRect& FromRect = Layout.ViewportWindows[Layout.TransitionFocusSlot]->GetRect();
			const float T = Clamp(Layout.LayoutTransitionElapsed / Layout.LayoutTransitionDuration, 0.0f, 1.0f);
			const float SmoothT = T * T * (3.0f - 2.0f * T);

			const float L = FromRect.X + (ContentRect.X - FromRect.X) * SmoothT;
			const float TT = FromRect.Y + (ContentRect.Y - FromRect.Y) * SmoothT;
			const float R = (FromRect.X + FromRect.Width) + ((ContentRect.X + ContentRect.Width) - (FromRect.X + FromRect.Width)) * SmoothT;
			const float B = (FromRect.Y + FromRect.Height) + ((ContentRect.Y + ContentRect.Height) - (FromRect.Y + FromRect.Height)) * SmoothT;

			ImGui::GetWindowDrawList()->AddImage(
				(ImTextureID)Layout.LevelViewportClients[Layout.TransitionFocusSlot]->GetViewport()->GetSRV(),
				ImVec2(L, TT),
				ImVec2(R, B));
		}
		else if (bCoverFromOnePane
			&& Layout.TransitionFocusSlot >= 0
			&& Layout.TransitionFocusSlot < static_cast<int32>(Layout.LevelViewportClients.size())
			&& Layout.TransitionFocusSlot < FLevelViewportLayout::MaxViewportSlots
			&& Layout.ViewportWindows[Layout.TransitionFocusSlot]
			&& Layout.LevelViewportClients[Layout.TransitionFocusSlot]
			&& Layout.LevelViewportClients[Layout.TransitionFocusSlot]->GetViewport()
			&& Layout.LevelViewportClients[Layout.TransitionFocusSlot]->GetViewport()->GetSRV())
		{
			const FRect& ToRect = Layout.ViewportWindows[Layout.TransitionFocusSlot]->GetRect();
			const float T = Clamp(Layout.LayoutTransitionElapsed / Layout.LayoutTransitionDuration, 0.0f, 1.0f);
			const float SmoothT = T * T * (3.0f - 2.0f * T);

			const float L = ContentRect.X + (ToRect.X - ContentRect.X) * SmoothT;
			const float TT = ContentRect.Y + (ToRect.Y - ContentRect.Y) * SmoothT;
			const float R = (ContentRect.X + ContentRect.Width) + ((ToRect.X + ToRect.Width) - (ContentRect.X + ContentRect.Width)) * SmoothT;
			const float B = (ContentRect.Y + ContentRect.Height) + ((ToRect.Y + ToRect.Height) - (ContentRect.Y + ContentRect.Height)) * SmoothT;

			ImGui::GetWindowDrawList()->AddImage(
				(ImTextureID)Layout.LevelViewportClients[Layout.TransitionFocusSlot]->GetViewport()->GetSRV(),
				ImVec2(L, TT),
				ImVec2(R, B));
		}

		if (ImGui::IsWindowHovered())
		{
			const ImVec2 MousePos = ImGui::GetIO().MousePos;
			FPoint MP = { MousePos.x, MousePos.y };

			for (int32 i = 0; i < Layout.ActiveSlotCount; ++i)
			{
				const int32 SlotIndex = (Layout.CurrentLayout == EViewportLayout::OnePane) ? OnePaneSlotIndex : i;
				if (SlotIndex < FLevelViewportLayout::MaxViewportSlots && Layout.ViewportWindows[SlotIndex] && Layout.ViewportWindows[SlotIndex]->IsHover(MP))
				{
					Layout.bMouseOverViewport = true;
					break;
				}
			}

			if (Layout.RootSplitter)
			{
				if (ImGui::IsMouseClicked(0))
				{
					Layout.DraggingSplitter = SSplitter::FindSplitterAtBar(Layout.RootSplitter, MP);
				}

				if (ImGui::IsMouseReleased(0))
				{
					Layout.DraggingSplitter = nullptr;
				}

				if (Layout.DraggingSplitter)
				{
					const FRect& DR = Layout.DraggingSplitter->GetRect();
					if (Layout.DraggingSplitter->GetOrientation() == ESplitOrientation::Horizontal)
					{
						const float NewRatio = (MousePos.x - DR.X) / DR.Width;
						Layout.DraggingSplitter->SetRatio(Clamp(NewRatio, 0.15f, 0.85f));
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
					}
					else
					{
						const float NewRatio = (MousePos.y - DR.Y) / DR.Height;
						Layout.DraggingSplitter->SetRatio(Clamp(NewRatio, 0.15f, 0.85f));
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
					}
				}
				else
				{
					SSplitter* Hovered = SSplitter::FindSplitterAtBar(Layout.RootSplitter, MP);
					if (Hovered)
					{
						if (Hovered->GetOrientation() == ESplitOrientation::Horizontal)
						{
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
						}
						else
						{
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
						}
					}
				}
			}

			if (!Layout.DraggingSplitter && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
			{
				for (int32 i = 0; i < Layout.ActiveSlotCount; ++i)
				{
					if (i < static_cast<int32>(Layout.LevelViewportClients.size())
						&& Layout.ViewportWindows[i] && Layout.ViewportWindows[i]->IsHover(MP))
					{
						if (Layout.LevelViewportClients[i] != Layout.ActiveViewportClient)
						{
							Layout.SetActiveViewport(Layout.LevelViewportClients[i]);
						}
						break;
					}
				}
			}
		}
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

void FLevelViewportLayoutUI::RenderActiveViewportStatOverlay(FLevelViewportLayout& Layout)
{
	if (!Layout.Editor || !Layout.ActiveViewportClient) return;

	int32 ActiveSlotIndex = -1;
	for (int32 i = 0; i < Layout.ActiveSlotCount; ++i)
	{
		if (i < static_cast<int32>(Layout.LevelViewportClients.size()) && Layout.LevelViewportClients[i] == Layout.ActiveViewportClient)
		{
			ActiveSlotIndex = i;
			break;
		}
	}

	if (ActiveSlotIndex < 0 || !Layout.ViewportWindows[ActiveSlotIndex]) return;

	const FOverlayStatSystem& OverlaySystem = Layout.Editor->GetOverlayStatSystem();
	const TArray<FOverlayStatGroup> Groups = OverlaySystem.BuildGroups(*Layout.Editor);
	if (Groups.empty()) return;

	const FOverlayStatLayout& OverlayLayout = OverlaySystem.GetLayout();
	const FRect& PaneRect = Layout.ViewportWindows[ActiveSlotIndex]->GetRect();

	char OverlayID[64];
	snprintf(OverlayID, sizeof(OverlayID), "##ViewportStatOverlay_%d", ActiveSlotIndex);

	ImGui::SetNextWindowPos(ImVec2(PaneRect.X + OverlayLayout.StartX, PaneRect.Y + OverlayLayout.StartY));
	ImGui::SetNextWindowBgAlpha(0.72f);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.11f, 0.72f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.42f, 0.55f, 0.85f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.95f, 0.98f, 1.0f));

	ImGuiWindowFlags OverlayFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs;

	ImGui::Begin(OverlayID, nullptr, OverlayFlags);
	ImGui::SetWindowFontScale(OverlayLayout.TextScale);
	ImGui::TextUnformatted("Status");
	ImGui::Separator();

	for (size_t GroupIdx = 0; GroupIdx < Groups.size(); ++GroupIdx)
	{
		const FOverlayStatGroup& Group = Groups[GroupIdx];
		for (const FString& Line : Group.Lines)
		{
			const size_t DelimPos = Line.find(':');
			if (DelimPos != FString::npos)
			{
				const FString Key = Line.substr(0, DelimPos + 1);
				const FString Value = DelimPos + 1 < Line.size() ? Line.substr(DelimPos + 1) : FString();

				ImGui::TextColored(ImVec4(0.67f, 0.75f, 0.85f, 1.0f), "%s", Key.c_str());
				if (!Value.empty())
				{
					ImGui::SameLine();
					ImGui::TextUnformatted(Value.c_str());
				}
			}
			else
			{
				ImGui::TextUnformatted(Line.c_str());
			}
		}

		if (GroupIdx + 1 < Groups.size() && !Group.Lines.empty())
		{
			ImGui::Dummy(ImVec2(0.0f, OverlayLayout.GroupSpacing * 0.5f));
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0.0f, OverlayLayout.GroupSpacing * 0.25f));
		}
	}

	ImGui::End();
	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(4);
}

void FLevelViewportLayoutUI::ReleaseResources()
{
	for (int32 i = 0; i < static_cast<int32>(EViewportToolbarIcon::Count); ++i)
	{
		if (GViewportToolbarIcons[i])
		{
			GViewportToolbarIcons[i]->Release();
			GViewportToolbarIcons[i] = nullptr;
		}
	}
	GViewportToolbarIconsLoaded = false;
}

void FLevelViewportLayoutUI::RenderPaneToolbar(FLevelViewportLayout& Layout, int32 SlotIndex)
{
	if (SlotIndex >= FLevelViewportLayout::MaxViewportSlots || !Layout.ViewportWindows[SlotIndex]) return;
	EnsureViewportToolbarIconsLoaded(Layout);

	const FRect& PaneRect = Layout.ViewportWindows[SlotIndex]->GetRect();
	if (PaneRect.Width <= 0 || PaneRect.Height <= 0) return;

	char OverlayID[64];
	snprintf(OverlayID, sizeof(OverlayID), "##PaneToolbar_%d", SlotIndex);

	constexpr float PaneToolbarHeight = 34.0f;
	ImGui::SetNextWindowPos(ImVec2(PaneRect.X, PaneRect.Y));
	ImGui::SetNextWindowBgAlpha(1.0f);
	ImGui::SetNextWindowSize(ImVec2(PaneRect.Width, PaneToolbarHeight), ImGuiCond_Always);

	ImGuiWindowFlags OverlayFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.12f, 0.14f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.37f, 0.40f, 0.78f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.43f, 0.44f, 0.47f, 0.90f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.31f, 0.34f, 0.95f));
	ImGui::Begin(OverlayID, nullptr, OverlayFlags);
	{
		ImGui::PushID(SlotIndex);
		constexpr float ToolbarFallbackIconSize = 14.0f;
		constexpr float ToolbarMaxIconSize = 16.0f;
		const float ItemHeight = (std::max)(ImGui::GetFrameHeight(), ToolbarMaxIconSize + ImGui::GetStyle().FramePadding.y * 2.0f);
		const float CenteredY = (PaneToolbarHeight - ItemHeight) * 0.5f;
		if (CenteredY > 0.0f)
		{
			ImGui::SetCursorPosY(CenteredY);
		}
		auto CalcButtonWidth = [ToolbarFallbackIconSize, ToolbarMaxIconSize](const char* Label, EViewportToolbarIcon Icon, bool bIconButton) -> float
		{
			if (bIconButton)
			{
				const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, ToolbarFallbackIconSize, ToolbarMaxIconSize);
				return IconSize.x + ImGui::GetStyle().FramePadding.x * 2.0f;
			}
			return ImGui::CalcTextSize(Label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		};
		auto AddWidth = [] (float& TotalWidth, int32& Count, float ItemWidth)
		{
			if (Count > 0)
			{
				TotalWidth += ImGui::GetStyle().ItemSpacing.x;
			}
			TotalWidth += ItemWidth;
			++Count;
		};

		if (SlotIndex < static_cast<int32>(Layout.LevelViewportClients.size()))
		{
			FLevelEditorViewportClient* VC = Layout.LevelViewportClients[SlotIndex];
			FViewportRenderOptions& Opts = VC->GetRenderOptions();
			const bool bOnePane = (Layout.CurrentLayout == EViewportLayout::OnePane);

			static const char* ViewportTypeNames[] = {
				"Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back", "Free Orthographic"
			};
			const int32 CurrentTypeIdx = static_cast<int32>(Opts.ViewportType);
			const char* CurrentTypeName = ViewportTypeNames[CurrentTypeIdx];

			FTransformGizmo* Gizmo = Layout.Editor->GetGizmo();

			static bool bTranslateSnapEnabled[FLevelViewportLayout::MaxViewportSlots] = { false, false, false, false };
			static bool bRotateSnapEnabled[FLevelViewportLayout::MaxViewportSlots] = { false, false, false, false };
			static bool bScaleSnapEnabled[FLevelViewportLayout::MaxViewportSlots] = { false, false, false, false };
			static int32 TranslateSnapIndex[FLevelViewportLayout::MaxViewportSlots] = { 0, 0, 0, 0 };
			static int32 RotateSnapIndex[FLevelViewportLayout::MaxViewportSlots] = { 0, 0, 0, 0 };
			static int32 ScaleSnapIndex[FLevelViewportLayout::MaxViewportSlots] = { 0, 0, 0, 0 };

			const char* TranslateSnapLabels[] = { "1", "5", "10", "50", "100" };
			const char* RotateSnapLabels[] = { "5", "10", "15", "30", "45" };
			const char* ScaleSnapLabels[] = { "0.1", "0.25", "0.5", "1.0", "5.0" };

			FEditorSettings& Settings = FEditorSettings::Get();

			char CameraValueLabel[40];
			snprintf(CameraValueLabel, sizeof(CameraValueLabel), "Cam %.1fx ▼", Settings.CameraSpeed);

			char VTPopupID[64];
			snprintf(VTPopupID, sizeof(VTPopupID), "ViewportTypePopup_%d", SlotIndex);
			if (ImGui::BeginPopup(VTPopupID))
			{
				for (int32 t = 0; t < static_cast<int32>(IM_ARRAYSIZE(ViewportTypeNames)); ++t)
				{
					const bool bSelected = (t == CurrentTypeIdx);
					if (ImGui::Selectable(ViewportTypeNames[t], bSelected))
					{
						VC->SetViewportType(static_cast<ELevelViewportType>(t));
					}
				}
				ImGui::EndPopup();
			}

			if (Gizmo)
			{
				enum class ETransformToolbarState : int32
				{
					Selection = 0,
					Translate = 1,
					Rotate = 2,
					Scale = 3
				};

				ETransformToolbarState CurrentToolState = ETransformToolbarState::Selection;
				if (Opts.ShowFlags.bGizmo)
				{
					switch (Gizmo->GetMode())
					{
					case EGizmoMode::Translate: CurrentToolState = ETransformToolbarState::Translate; break;
					case EGizmoMode::Rotate: CurrentToolState = ETransformToolbarState::Rotate; break;
					case EGizmoMode::Scale: CurrentToolState = ETransformToolbarState::Scale; break;
					default: CurrentToolState = ETransformToolbarState::Selection; break;
					}
				}

				auto DrawTransformIcon = [ToolbarFallbackIconSize, ToolbarMaxIconSize, &CurrentToolState](const char* Id, EViewportToolbarIcon Icon, ETransformToolbarState TargetState) -> bool
				{
					const bool bSelected = (CurrentToolState == TargetState);
					if (bSelected)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.46f, 0.63f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.37f, 0.52f, 0.70f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.27f, 0.39f, 0.54f, 1.0f));
					}
					const bool bPressed = DrawToolbarIconButton(Id, Icon, Id, ToolbarFallbackIconSize, ToolbarMaxIconSize);
					if (bSelected)
					{
						ImGui::PopStyleColor(3);
					}
					return bPressed;
				};

				if (DrawTransformIcon("##SelectTool", EViewportToolbarIcon::Select, ETransformToolbarState::Selection))
				{
					Opts.ShowFlags.bGizmo = false;
				}
				ImGui::SameLine();
				if (DrawTransformIcon("##TranslateTool", EViewportToolbarIcon::Translate, ETransformToolbarState::Translate))
				{
					Opts.ShowFlags.bGizmo = true;
					Gizmo->SetTranslateMode();
				}
				ImGui::SameLine();
				if (DrawTransformIcon("##RotateTool", EViewportToolbarIcon::Rotate, ETransformToolbarState::Rotate))
				{
					Opts.ShowFlags.bGizmo = true;
					Gizmo->SetRotateMode();
				}
				ImGui::SameLine();
				if (DrawTransformIcon("##ScaleTool", EViewportToolbarIcon::Scale, ETransformToolbarState::Scale))
				{
					Opts.ShowFlags.bGizmo = true;
					Gizmo->SetScaleMode();
				}

				ImGui::SameLine(0.0f, 10.0f);
				const ImVec2 SeparatorStart = ImGui::GetCursorScreenPos();
				const float SeparatorHeight = ImGui::GetFrameHeight() - 4.0f;
				ImGui::GetWindowDrawList()->AddLine(
					ImVec2(SeparatorStart.x, SeparatorStart.y + 2.0f),
					ImVec2(SeparatorStart.x, SeparatorStart.y + 2.0f + SeparatorHeight),
					IM_COL32(155, 155, 155, 255),
					1.0f);
				ImGui::Dummy(ImVec2(1.0f, ImGui::GetFrameHeight()));

				ImGui::SameLine(0.0f, 10.0f);
				const bool bWorldSpace = Gizmo->IsWorldSpace();
				const EViewportToolbarIcon CoordinateIcon = bWorldSpace ? EViewportToolbarIcon::WorldSpace : EViewportToolbarIcon::LocalSpace;
				if (DrawToolbarIconButton("##CoordinateSpaceToggle", CoordinateIcon, bWorldSpace ? "World" : "Local", ToolbarFallbackIconSize, ToolbarMaxIconSize))
				{
					Gizmo->ToggleCoordinateSpace();
				}

				if (bOnePane)
				{
					auto DrawSnapSection = [SlotIndex, ToolbarFallbackIconSize, ToolbarMaxIconSize](EViewportToolbarIcon SnapIcon, const char* Prefix, bool& bEnabled, int32& ValueIndex, const char* const* Labels, int32 LabelCount)
					{
						ImGui::SameLine(0.0f, 6.0f);

						if (bEnabled)
						{
							ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.43f, 0.30f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.50f, 0.36f, 1.0f));
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.36f, 0.26f, 1.0f));
						}

						char ToggleID[48];
						snprintf(ToggleID, sizeof(ToggleID), "##%sSnapToggle_%d", Prefix, SlotIndex);
						if (DrawToolbarIconButton(ToggleID, SnapIcon, Prefix, ToolbarFallbackIconSize, ToolbarMaxIconSize, EToolbarButtonShape::PairFirst))
						{
							bEnabled = !bEnabled;
						}

						if (bEnabled)
						{
							ImGui::PopStyleColor(3);
						}

						ImGui::SameLine(0.0f, 0.0f);
						char ValueButtonLabel[24];
						snprintf(ValueButtonLabel, sizeof(ValueButtonLabel), "%s ▼", Labels[ValueIndex]);

						char PopupID[40];
						snprintf(PopupID, sizeof(PopupID), "##%sSnapPopup_%d", Prefix, SlotIndex);
						char ValueBtnID[48];
						snprintf(ValueBtnID, sizeof(ValueBtnID), "##%sSnapValueBtn_%d", Prefix, SlotIndex);

						if (DrawToolbarTextButton(ValueBtnID, ValueButtonLabel, EToolbarButtonShape::PairSecond))
						{
							ImGui::OpenPopup(PopupID);
						}

						if (ImGui::BeginPopup(PopupID))
						{
							for (int32 i = 0; i < LabelCount; ++i)
							{
								const bool bSelected = (ValueIndex == i);
								if (ImGui::RadioButton(Labels[i], bSelected))
								{
									ValueIndex = i;
								}
							}
							ImGui::EndPopup();
						}
					};

					DrawSnapSection(EViewportToolbarIcon::TranslateSnap, "T", bTranslateSnapEnabled[SlotIndex], TranslateSnapIndex[SlotIndex], TranslateSnapLabels, IM_ARRAYSIZE(TranslateSnapLabels));
					DrawSnapSection(EViewportToolbarIcon::RotateSnap, "R", bRotateSnapEnabled[SlotIndex], RotateSnapIndex[SlotIndex], RotateSnapLabels, IM_ARRAYSIZE(RotateSnapLabels));
					DrawSnapSection(EViewportToolbarIcon::ScaleSnap, "S", bScaleSnapEnabled[SlotIndex], ScaleSnapIndex[SlotIndex], ScaleSnapLabels, IM_ARRAYSIZE(ScaleSnapLabels));
				}
			}

			ImGui::SameLine();

			char CameraPopupID[48];
			snprintf(CameraPopupID, sizeof(CameraPopupID), "##CameraSpeedPopup_%d", SlotIndex);
			char SettingsPopupID[64];
			snprintf(SettingsPopupID, sizeof(SettingsPopupID), "SettingsPopup_%d", SlotIndex);
			char LayoutPopupID[64];
			snprintf(LayoutPopupID, sizeof(LayoutPopupID), "LayoutPopup_%d", SlotIndex);
			char OverflowPopupID[64];
			snprintf(OverflowPopupID, sizeof(OverflowPopupID), "ToolbarOverflowPopup_%d", SlotIndex);

			float RightWidth = 0.0f;
			int32 RightItemCount = 0;
			AddWidth(RightWidth, RightItemCount, CalcButtonWidth(CurrentTypeName, EViewportToolbarIcon::Menu, false));
			if (bOnePane)
			{
				AddWidth(RightWidth, RightItemCount, CalcButtonWidth(CameraValueLabel, EViewportToolbarIcon::Menu, false));
			}
			AddWidth(RightWidth, RightItemCount, CalcButtonWidth("Settings", EViewportToolbarIcon::Setting, true));
			AddWidth(RightWidth, RightItemCount, CalcButtonWidth("Layout", EViewportToolbarIcon::Menu, true));

			EViewportLayout NextToggleLayout = EViewportLayout::OnePane;
			if (Layout.CurrentLayout == EViewportLayout::OnePane)
			{
				NextToggleLayout = (Layout.LastSplitLayout == EViewportLayout::OnePane)
					? EViewportLayout::FourPanes2x2
					: Layout.LastSplitLayout;
			}
			AddWidth(RightWidth, RightItemCount, ToolbarMaxIconSize + ImGui::GetStyle().FramePadding.x * 2.0f);

			const float RightStartX = ImGui::GetWindowContentRegionMax().x - RightWidth;
			const bool bToolbarOverflow = (RightStartX <= ImGui::GetCursorPosX() + 2.0f);
			if (!bToolbarOverflow && RightStartX > ImGui::GetCursorPosX())
			{
				ImGui::SetCursorPosX(RightStartX);
			}

			if (bToolbarOverflow)
			{
				const float OverflowButtonWidth = CalcButtonWidth("Overflow", EViewportToolbarIcon::Menu, true);
				const float OverflowPosX = ImGui::GetWindowContentRegionMax().x - OverflowButtonWidth;
				if (OverflowPosX > ImGui::GetCursorPosX())
				{
					ImGui::SetCursorPosX(OverflowPosX);
				}

				if (DrawToolbarIconButton("##ToolbarOverflowIcon", EViewportToolbarIcon::Menu, "...", ToolbarFallbackIconSize, ToolbarMaxIconSize))
				{
					ImGui::OpenPopup(OverflowPopupID);
				}

				if (ImGui::BeginPopup(OverflowPopupID))
				{
					if (ImGui::MenuItem("Perspective"))
					{
						ImGui::OpenPopup(VTPopupID);
					}
					if (bOnePane && ImGui::MenuItem("Camera Speed"))
					{
						ImGui::OpenPopup(CameraPopupID);
					}
					if (ImGui::MenuItem("Settings"))
					{
						ImGui::OpenPopup(SettingsPopupID);
					}
					if (ImGui::MenuItem("Layout"))
					{
						ImGui::OpenPopup(LayoutPopupID);
					}
					const char* ToggleLabel = (Layout.CurrentLayout == EViewportLayout::OnePane) ? "Split" : "Merge";
					if (ImGui::MenuItem(ToggleLabel))
					{
						if (Layout.LevelViewportClients[SlotIndex] != Layout.ActiveViewportClient)
						{
							Layout.SetActiveViewport(Layout.LevelViewportClients[SlotIndex]);
						}
						Layout.ToggleViewportSplit();
					}
					ImGui::EndPopup();
				}
			}
			else
			{

				if (DrawToolbarTextButton("##ViewportTypeBtn", CurrentTypeName))
				{
					ImGui::OpenPopup(VTPopupID);
				}

				ImGui::SameLine();
				if (bOnePane && DrawToolbarTextButton("##CameraSpeedBtn", CameraValueLabel))
				{
					ImGui::OpenPopup(CameraPopupID);
				}

				ImGui::SameLine(0.0f, 8.0f);
				if (DrawToolbarIconButton("##SettingsIcon", EViewportToolbarIcon::Setting, "Settings", ToolbarFallbackIconSize, ToolbarMaxIconSize))
				{
					ImGui::OpenPopup(SettingsPopupID);
				}

				ImGui::SameLine();
				if (DrawToolbarIconButton("##LayoutMenuIcon", EViewportToolbarIcon::Menu, "Layout", ToolbarFallbackIconSize, ToolbarMaxIconSize, EToolbarButtonShape::PairFirst))
				{
					ImGui::OpenPopup(LayoutPopupID);
				}

				ImGui::SameLine(0.0f, 0.0f);
				const int32 ToggleIdx = static_cast<int32>(NextToggleLayout);
				if (Layout.LayoutIcons[ToggleIdx])
				{
					if (DrawToolbarImageButton("##SplitToggleIcon", Layout.LayoutIcons[ToggleIdx], "Split", ToolbarFallbackIconSize, ToolbarMaxIconSize, EToolbarButtonShape::PairSecond))
					{
						if (Layout.LevelViewportClients[SlotIndex] != Layout.ActiveViewportClient)
						{
							Layout.SetActiveViewport(Layout.LevelViewportClients[SlotIndex]);
						}
						Layout.ToggleViewportSplit();
					}
				}
				else
				{
					const char* ToggleLabel = (Layout.CurrentLayout == EViewportLayout::OnePane) ? "Split" : "Merge";
					if (DrawToolbarTextButton("##SplitToggleTextBtn", ToggleLabel, EToolbarButtonShape::PairSecond))
					{
						if (Layout.LevelViewportClients[SlotIndex] != Layout.ActiveViewportClient)
						{
							Layout.SetActiveViewport(Layout.LevelViewportClients[SlotIndex]);
						}
						Layout.ToggleViewportSplit();
					}
				}
			}

			if (bOnePane && ImGui::BeginPopup(CameraPopupID))
			{
				float CameraSpeed = Settings.CameraSpeed;
				if (ImGui::SliderFloat("Speed", &CameraSpeed, 0.1f, 32.0f, "%.1fx"))
				{
					Settings.CameraSpeed = CameraSpeed;
				}
				ImGui::EndPopup();
			}

			if (ImGui::BeginPopup(SettingsPopupID))
			{
				ImGui::Text("View Mode");
				int32 CurrentMode = static_cast<int32>(Opts.ViewMode);
				ImGui::RadioButton("Lit", &CurrentMode, static_cast<int32>(EViewMode::Lit));
				ImGui::SameLine();
				ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
				ImGui::SameLine();
				ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
				Opts.ViewMode = static_cast<EViewMode>(CurrentMode);

				ImGui::Separator();
				ImGui::Text("Show");
				ImGui::Checkbox("Primitives", &Opts.ShowFlags.bPrimitives);
				ImGui::Checkbox("BillboardText", &Opts.ShowFlags.bBillboardText);
				ImGui::Checkbox("Grid", &Opts.ShowFlags.bGrid);
				ImGui::Checkbox("Gizmo", &Opts.ShowFlags.bGizmo);
				ImGui::Checkbox("Bounding Volume", &Opts.ShowFlags.bBoundingVolume);

				ImGui::Separator();
				ImGui::Text("Grid");
				ImGui::SliderFloat("Spacing", &Opts.GridSpacing, 0.1f, 10.0f, "%.1f");
				ImGui::SliderInt("Half Line Count", &Opts.GridHalfLineCount, 10, 500);

				ImGui::Separator();
				ImGui::Text("Camera");
				ImGui::SliderFloat("Move Sensitivity", &Opts.CameraMoveSensitivity, 0.1f, 5.0f, "%.1f");
				ImGui::SliderFloat("Rotate Sensitivity", &Opts.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");
				ImGui::EndPopup();
			}

			if (ImGui::BeginPopup(LayoutPopupID))
			{
				constexpr int32 LayoutCount = static_cast<int32>(EViewportLayout::MAX);
				constexpr int32 Columns = 4;
				constexpr float IconSize = 32.0f;

				for (int32 i = 0; i < LayoutCount; ++i)
				{
					ImGui::PushID(i);
					const bool bSelected = (static_cast<EViewportLayout>(i) == Layout.CurrentLayout);
					if (bSelected)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33f, 0.46f, 0.63f, 1.0f));
					}

					bool bClicked = false;
					if (Layout.LayoutIcons[i])
					{
						bClicked = ImGui::ImageButton("##icon", (ImTextureID)Layout.LayoutIcons[i], ImVec2(IconSize, IconSize));
					}
					else
					{
						char FallbackLabel[4];
						snprintf(FallbackLabel, sizeof(FallbackLabel), "%d", i);
						bClicked = ImGui::Button(FallbackLabel, ImVec2(IconSize + 8, IconSize + 8));
					}
					if (bSelected)
					{
						ImGui::PopStyleColor();
					}
					if (bClicked)
					{
						Layout.SetLayout(static_cast<EViewportLayout>(i));
						ImGui::CloseCurrentPopup();
					}
					if ((i + 1) % Columns != 0 && i + 1 < LayoutCount)
					{
						ImGui::SameLine();
					}
					ImGui::PopID();
				}
				ImGui::EndPopup();
			}


		}

		ImGui::PopID();
	}
	ImGui::End();
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(3);
}
