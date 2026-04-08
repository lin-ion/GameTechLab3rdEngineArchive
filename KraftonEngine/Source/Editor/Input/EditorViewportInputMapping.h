#pragma once

#include "Engine/Input/InputBinding.h"

namespace EditorViewportInputMapping
{
    enum class EEditorViewportAction : int32
    {
        CycleMode,
        CycleGizmoMode,
        ToggleGizmoCoordinateSpace,
        FocusSelection,
        DeleteSelection,
        SelectAll,
        GizmoPrimaryPressed,
        GizmoPrimaryDown,
        GizmoPrimaryReleased,
        CtrlModifierDown,
        ShiftModifierDown,
        AltModifierDown,
        BoxSelectReplaceDown,
        BoxSelectAdditiveDown,
        NavOrbitAltLeftDown,
        NavDollyAltRightDown,
        NavPanAltMiddleDown,
        NavLookRightDown,
        NavLookMiddleDown,
        NavMoveForward,
        NavMoveLeft,
        NavMoveBackward,
        NavMoveRight,
        NavMoveDown,
        NavMoveUp,
        NavRotateUp,
        NavRotateLeft,
        NavRotateDown,
        NavRotateRight,
        NavWheelScroll
    };

    inline const TArray<FInputBinding>& GetBindings()
    {
        static const TArray<FInputBinding> Bindings =
        {
            { static_cast<int32>(EEditorViewportAction::CycleMode), EInputBindingTrigger::Released, { VK_TAB, false, false, false }, EInputEventType::KeyReleased, 200 },
            { static_cast<int32>(EEditorViewportAction::CycleGizmoMode), EInputBindingTrigger::Released, { VK_SPACE, false, false, false }, EInputEventType::KeyReleased, 100 },
            { static_cast<int32>(EEditorViewportAction::ToggleGizmoCoordinateSpace), EInputBindingTrigger::Released, { 'X', false, false, false }, EInputEventType::KeyReleased, 90 },
            { static_cast<int32>(EEditorViewportAction::FocusSelection), EInputBindingTrigger::Released, { 'F', false, false, false }, EInputEventType::KeyReleased, 80 },
            { static_cast<int32>(EEditorViewportAction::DeleteSelection), EInputBindingTrigger::Released, { VK_DELETE, false, false, false }, EInputEventType::KeyReleased, 80 },
            { static_cast<int32>(EEditorViewportAction::SelectAll), EInputBindingTrigger::Pressed, { 'A', true, false, false }, EInputEventType::KeyPressed, 85 },

            { static_cast<int32>(EEditorViewportAction::GizmoPrimaryPressed), EInputBindingTrigger::Pressed, { VK_LBUTTON, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::GizmoPrimaryDown), EInputBindingTrigger::Down, { VK_LBUTTON, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::GizmoPrimaryReleased), EInputBindingTrigger::Released, { VK_LBUTTON, false, false, false }, EInputEventType::KeyReleased },

            { static_cast<int32>(EEditorViewportAction::CtrlModifierDown), EInputBindingTrigger::Down, { VK_CONTROL, true, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::ShiftModifierDown), EInputBindingTrigger::Down, { VK_SHIFT, false, false, true }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::AltModifierDown), EInputBindingTrigger::Down, { VK_MENU, false, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::BoxSelectReplaceDown), EInputBindingTrigger::Down, { VK_LBUTTON, true, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::BoxSelectAdditiveDown), EInputBindingTrigger::Down, { VK_LBUTTON, true, true, true }, EInputEventType::KeyPressed },

            { static_cast<int32>(EEditorViewportAction::NavOrbitAltLeftDown), EInputBindingTrigger::Down, { VK_LBUTTON, false, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavDollyAltRightDown), EInputBindingTrigger::Down, { VK_RBUTTON, false, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavPanAltMiddleDown), EInputBindingTrigger::Down, { VK_MBUTTON, false, true, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavLookRightDown), EInputBindingTrigger::Down, { VK_RBUTTON, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavLookMiddleDown), EInputBindingTrigger::Down, { VK_MBUTTON, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveForward), EInputBindingTrigger::Down, { 'W', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveLeft), EInputBindingTrigger::Down, { 'A', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveBackward), EInputBindingTrigger::Down, { 'S', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveRight), EInputBindingTrigger::Down, { 'D', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveDown), EInputBindingTrigger::Down, { 'Q', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavMoveUp), EInputBindingTrigger::Down, { 'E', false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavRotateUp), EInputBindingTrigger::Down, { VK_UP, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavRotateLeft), EInputBindingTrigger::Down, { VK_LEFT, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavRotateDown), EInputBindingTrigger::Down, { VK_DOWN, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavRotateRight), EInputBindingTrigger::Down, { VK_RIGHT, false, false, false }, EInputEventType::KeyPressed },
            { static_cast<int32>(EEditorViewportAction::NavWheelScroll), EInputBindingTrigger::EventType, { 0, false, false, false }, EInputEventType::WheelScrolled }
        };

        return Bindings;
    }

    inline bool IsTriggered(const FViewportInputContext& Context, EEditorViewportAction Action)
    {
        return InputBindingUtils::IsActionTriggered(Context, GetBindings(), static_cast<int32>(Action));
    }
}
