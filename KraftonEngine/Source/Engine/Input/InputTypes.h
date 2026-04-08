#pragma once

#include <windows.h>
#include "Core/CoreTypes.h"

class FViewport;
class FViewportClient;

enum class EInputRouteDomain : uint8
{
    Editor,
    ObjViewer
};

enum class EMouseInputMode : uint8
{
    Absolute,
    Relative
};

struct FInputFrame
{
    uint64 FrameNumber = 0;
    HWND SourceWindow = nullptr;
    EMouseInputMode MouseInputMode = EMouseInputMode::Absolute;

    POINT MouseScreenPos = { 0, 0 };
    POINT MouseDelta = { 0, 0 };
    float WheelNotches = 0.0f;

    bool KeyDown[256] = {};
    bool KeyPressed[256] = {};
    bool KeyReleased[256] = {};

    bool bLeftDragStarted = false;
    bool bLeftDragging = false;
    bool bLeftDragEnded = false;
    bool bRightDragStarted = false;
    bool bRightDragging = false;
    bool bRightDragEnded = false;

    POINT LeftDragVector = { 0, 0 };
    POINT RightDragVector = { 0, 0 };

    bool IsDown(int VK) const { return KeyDown[VK]; }
    bool IsPressed(int VK) const { return KeyPressed[VK]; }
    bool IsReleased(int VK) const { return KeyReleased[VK]; }
};

struct FViewportInputContext
{
    FInputFrame Frame;

    FViewport* TargetViewport = nullptr;
    FViewportClient* TargetClient = nullptr;
    EInputRouteDomain Domain = EInputRouteDomain::Editor;

    POINT MouseClientPos = { 0, 0 };
    POINT MouseLocalPos = { 0, 0 };
    POINT MouseLocalDelta = { 0, 0 };

    bool bHovered = false;
    bool bFocused = false;
    bool bCaptured = false;
    bool bImGuiCapturedMouse = false;
    bool bImGuiCapturedKeyboard = false;
    bool bRelativeMouseMode = false;
};

class IInputContext
{
public:
    virtual ~IInputContext() = default;
    virtual bool HandleInput(FViewportInputContext& Context) = 0;
};
