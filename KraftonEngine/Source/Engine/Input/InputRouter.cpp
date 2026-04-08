#include "Engine/Input/InputRouter.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Engine/Input/InputSystem.h"
#include "Viewport/ViewportClient.h"

void FInputRouter::SetOwnerWindow(HWND InOwnerWindow)
{
    OwnerWindow = InOwnerWindow;
    InputSystem::Get().SetOwnerWindow(InOwnerWindow);
}

void FInputRouter::SetImGuiCaptureState(bool bCaptureMouse, bool bCaptureKeyboard)
{
    bImGuiCaptureMouse = bCaptureMouse;
    bImGuiCaptureKeyboard = bCaptureKeyboard;
}

void FInputRouter::ClearTargets()
{
    Targets.clear();
    HoveredViewport = nullptr;
}

void FInputRouter::RegisterTarget(
    FViewport* InViewport,
    FViewportClient* InClient,
    EInputRouteDomain InDomain,
    FRectProvider InRectProvider)
{
    if (!InViewport || !InClient || !InRectProvider)
    {
        return;
    }

    FTargetEntry Entry;
    Entry.Viewport = InViewport;
    Entry.Client = InClient;
    Entry.Domain = InDomain;
    Entry.RectProvider = std::move(InRectProvider);
    Targets.push_back(std::move(Entry));
}

void FInputRouter::Tick()
{
    InputSystem::Get().Tick();
    InputSystem& Input = InputSystem::Get();
    const FInputFrame& Frame = Input.GetFrame();
    FInputFrame RoutedFrame = Frame;
    const bool bRelativeMouseMode = Input.IsRelativeMouseMode();

    if (!OwnerWindow || Targets.empty())
    {
        HoveredViewport = nullptr;
        FocusedViewport = nullptr;
        CapturedViewport = nullptr;
        RelativeViewport = nullptr;
        Input.EndRelativeMouseMode();
        return;
    }

    if (GetForegroundWindow() != OwnerWindow && GetCapture() != OwnerWindow)
    {
        HoveredViewport = nullptr;
        FocusedViewport = nullptr;
        CapturedViewport = nullptr;
        RelativeViewport = nullptr;
        Input.EndRelativeMouseMode();
        return;
    }

    POINT MouseScreenPos = Frame.MouseScreenPos;
    POINT MouseClientPos = MouseScreenPos;
    ScreenToClient(OwnerWindow, &MouseClientPos);

    FTargetEntry* HoveredEntry = nullptr;
    FRect HoveredRect = {};
    if (!bRelativeMouseMode)
    {
        for (FTargetEntry& Entry : Targets)
        {
            FRect Rect = {};
            if (!Entry.RectProvider(Rect))
            {
                continue;
            }

            if (IsPointInRect(MouseClientPos, Rect))
            {
                HoveredEntry = &Entry;
                HoveredRect = Rect;
                break;
            }
        }
    }

    HoveredViewport = HoveredEntry ? HoveredEntry->Viewport : nullptr;

    bool bAnyPointerPressed =
        Frame.IsPressed(VK_LBUTTON) || Frame.IsPressed(VK_RBUTTON) || Frame.IsPressed(VK_MBUTTON);
    bool bAnyPointerDown =
        Frame.IsDown(VK_LBUTTON) || Frame.IsDown(VK_RBUTTON) || Frame.IsDown(VK_MBUTTON)
        || Frame.bLeftDragging || Frame.bRightDragging;

    bool bFocusedStillValid = false;
    bool bCapturedStillValid = false;
    for (const FTargetEntry& Entry : Targets)
    {
        bFocusedStillValid |= (FocusedViewport == Entry.Viewport);
        bCapturedStillValid |= (CapturedViewport == Entry.Viewport);
    }
    if (!bFocusedStillValid)
    {
        FocusedViewport = nullptr;
    }
    if (!bCapturedStillValid)
    {
        CapturedViewport = nullptr;
    }
    if (RelativeViewport)
    {
        bool bRelativeStillValid = false;
        for (const FTargetEntry& Entry : Targets)
        {
            bRelativeStillValid |= (RelativeViewport == Entry.Viewport);
        }
        if (!bRelativeStillValid)
        {
            RelativeViewport = nullptr;
            Input.EndRelativeMouseMode();
        }
    }

    const bool bImGuiBlocksViewportAcquire = bImGuiCaptureMouse && (CapturedViewport == nullptr) && (HoveredEntry == nullptr);
    if (bImGuiBlocksViewportAcquire)
    {
        bAnyPointerPressed = false;
        bAnyPointerDown = false;
    }

    if (bAnyPointerPressed && HoveredEntry)
    {
        FocusedViewport = HoveredEntry->Viewport;
        CapturedViewport = HoveredEntry->Viewport;
    }

    if (!bAnyPointerDown)
    {
        CapturedViewport = nullptr;
    }

    FTargetEntry* TargetEntry = nullptr;
    FRect TargetRect = {};

    if (bRelativeMouseMode && RelativeViewport)
    {
        TargetEntry = FindEntryByViewport(RelativeViewport, TargetRect);
    }
    if (!TargetEntry && CapturedViewport)
    {
        TargetEntry = FindEntryByViewport(CapturedViewport, TargetRect);
    }
    if (!TargetEntry && HoveredEntry)
    {
        TargetEntry = HoveredEntry;
        TargetRect = HoveredRect;
    }
    if (!TargetEntry && FocusedViewport)
    {
        TargetEntry = FindEntryByViewport(FocusedViewport, TargetRect);
    }

    if (!TargetEntry)
    {
        RelativeViewport = nullptr;
        Input.EndRelativeMouseMode();
        return;
    }

    if (bRelativeMouseMode)
    {
        MouseScreenPos = Input.GetRelativeRestoreScreenPos();
        MouseClientPos = MouseScreenPos;
        ScreenToClient(OwnerWindow, &MouseClientPos);
    }

    if (bImGuiCaptureKeyboard)
    {
        for (int32 VK = 0; VK < 256; ++VK)
        {
            const bool bIsMouseButton =
                (VK == VK_LBUTTON) || (VK == VK_RBUTTON) || (VK == VK_MBUTTON)
                || (VK == VK_XBUTTON1) || (VK == VK_XBUTTON2);
            if (!bIsMouseButton)
            {
                RoutedFrame.KeyDown[VK] = false;
                RoutedFrame.KeyPressed[VK] = false;
                RoutedFrame.KeyReleased[VK] = false;
            }
        }
    }

    const bool bBlockMouseForImGui = bImGuiCaptureMouse && (CapturedViewport == nullptr) && (HoveredEntry == nullptr);
    if (bBlockMouseForImGui)
    {
        RoutedFrame.MouseDelta.x = 0;
        RoutedFrame.MouseDelta.y = 0;
        RoutedFrame.WheelNotches = 0.0f;
        RoutedFrame.bLeftDragStarted = false;
        RoutedFrame.bLeftDragging = false;
        RoutedFrame.bLeftDragEnded = false;
        RoutedFrame.bRightDragStarted = false;
        RoutedFrame.bRightDragging = false;
        RoutedFrame.bRightDragEnded = false;
        RoutedFrame.LeftDragVector = { 0, 0 };
        RoutedFrame.RightDragVector = { 0, 0 };

        const int32 MouseVks[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
        for (int32 MouseVk : MouseVks)
        {
            RoutedFrame.KeyDown[MouseVk] = false;
            RoutedFrame.KeyPressed[MouseVk] = false;
            RoutedFrame.KeyReleased[MouseVk] = false;
        }
    }

    RoutedFrame.MouseScreenPos = MouseScreenPos;

    FViewportInputContext Context;
    Context.Frame = RoutedFrame;
    Context.TargetViewport = TargetEntry->Viewport;
    Context.TargetClient = TargetEntry->Client;
    Context.Domain = TargetEntry->Domain;
    Context.MouseClientPos = MouseClientPos;
    Context.MouseLocalPos.x = MouseClientPos.x - static_cast<LONG>(TargetRect.X);
    Context.MouseLocalPos.y = MouseClientPos.y - static_cast<LONG>(TargetRect.Y);
    Context.MouseLocalDelta = RoutedFrame.MouseDelta;
    Context.bHovered = (HoveredViewport == TargetEntry->Viewport);
    Context.bFocused = (FocusedViewport == TargetEntry->Viewport);
    Context.bCaptured = (CapturedViewport == TargetEntry->Viewport);
    Context.bImGuiCapturedMouse = bImGuiCaptureMouse;
    Context.bImGuiCapturedKeyboard = bImGuiCaptureKeyboard;
    Context.bRelativeMouseMode = bRelativeMouseMode;

    POINT RelativeRestorePos = MouseScreenPos;
    const bool bWantsRelativeMouseMode =
        !bImGuiCaptureMouse
        && TargetEntry->Client
        && TargetEntry->Client->WantsRelativeMouseMode(Context, RelativeRestorePos);

    if (!bRelativeMouseMode && bWantsRelativeMouseMode)
    {
        RelativeViewport = TargetEntry->Viewport;
        CapturedViewport = TargetEntry->Viewport;
        FocusedViewport = TargetEntry->Viewport;
        Input.BeginRelativeMouseMode(OwnerWindow, RelativeRestorePos);
        Context.bCaptured = true;
        Context.bFocused = true;
        Context.bRelativeMouseMode = true;
        Context.Frame.MouseInputMode = EMouseInputMode::Relative;
        Context.Frame.MouseScreenPos = RelativeRestorePos;
        Context.MouseClientPos = RelativeRestorePos;
        ScreenToClient(OwnerWindow, &Context.MouseClientPos);
        Context.MouseLocalPos.x = Context.MouseClientPos.x - static_cast<LONG>(TargetRect.X);
        Context.MouseLocalPos.y = Context.MouseClientPos.y - static_cast<LONG>(TargetRect.Y);
    }
    else if (bRelativeMouseMode && (!bWantsRelativeMouseMode || RelativeViewport != TargetEntry->Viewport))
    {
        RelativeViewport = nullptr;
        Input.EndRelativeMouseMode();
        Context.bRelativeMouseMode = false;
        Context.Frame.MouseInputMode = EMouseInputMode::Absolute;
    }
    else if (bRelativeMouseMode)
    {
        RelativeViewport = TargetEntry->Viewport;
    }

    const bool bConsumed = TargetEntry->Client->ProcessInput(Context);
    (void)bConsumed;
}

bool FInputRouter::IsPointInRect(const POINT& Point, const FRect& Rect)
{
    return Point.x >= Rect.X
        && Point.x < (Rect.X + Rect.Width)
        && Point.y >= Rect.Y
        && Point.y < (Rect.Y + Rect.Height);
}

FInputRouter::FTargetEntry* FInputRouter::FindEntryByViewport(FViewport* InViewport, FRect& OutRect)
{
    for (FTargetEntry& Entry : Targets)
    {
        if (Entry.Viewport != InViewport)
        {
            continue;
        }

        if (Entry.RectProvider(OutRect))
        {
            return &Entry;
        }

        return nullptr;
    }

    return nullptr;
}

const FInputRouter::FTargetEntry* FInputRouter::FindEntryByViewport(FViewport* InViewport, FRect& OutRect) const
{
    for (const FTargetEntry& Entry : Targets)
    {
        if (Entry.Viewport != InViewport)
        {
            continue;
        }

        if (Entry.RectProvider(OutRect))
        {
            return &Entry;
        }

        return nullptr;
    }

    return nullptr;
}
