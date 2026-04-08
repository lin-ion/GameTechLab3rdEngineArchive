#pragma once

#include "Engine/Input/InputTypes.h"

class FEditorViewportClient;

class FEditorCommandInputContext final : public IInputContext
{
public:
	FEditorCommandInputContext(FEditorViewportClient* InOwner, float* InDeltaTime);
	bool HandleInput(FViewportInputContext& Context) override;

private:
	FEditorViewportClient* Owner = nullptr;
	float* DeltaTimePtr = nullptr;
};

class FEditorGizmoInputContext final : public IInputContext
{
public:
	FEditorGizmoInputContext(FEditorViewportClient* InOwner, float* InDeltaTime);
	bool HandleInput(FViewportInputContext& Context) override;

private:
	FEditorViewportClient* Owner = nullptr;
	float* DeltaTimePtr = nullptr;
};

class FEditorSelectionInputContext final : public IInputContext
{
public:
	FEditorSelectionInputContext(FEditorViewportClient* InOwner, float* InDeltaTime);
	bool HandleInput(FViewportInputContext& Context) override;

private:
	FEditorViewportClient* Owner = nullptr;
	float* DeltaTimePtr = nullptr;
};

class FEditorNavigationInputContext final : public IInputContext
{
public:
	FEditorNavigationInputContext(FEditorViewportClient* InOwner, float* InDeltaTime);
	bool HandleInput(FViewportInputContext& Context) override;

private:
	FEditorViewportClient* Owner = nullptr;
	float* DeltaTimePtr = nullptr;
};
