#pragma once

class UWindow;
class URenderer;
class UGraphics;
class USceneManager;
class UImGuiDrawer;
class UResourceManager;
class UWorld;
class FEditorViewportClient;

class UApp
{
public:
	UApp() = default;
	~UApp() = default;

public:
	bool Initialize(HINSTANCE hInstance);
	void Run();
	void Release();

private:
	const int				   TargetFrame = { 60 };
	float					   DeltaTime = { 1.f / TargetFrame };

	UWindow* Window     = { nullptr };
	UGraphics* Graphics = { nullptr };

	URenderer* Renderer = { nullptr };
	UWorld* World = { nullptr };

	FEditorViewportClient* ViewportClient;


	UImGuiDrawer* ImGuiDrawer = { nullptr };
	UResourceManager* ResourceManager = { nullptr };

};