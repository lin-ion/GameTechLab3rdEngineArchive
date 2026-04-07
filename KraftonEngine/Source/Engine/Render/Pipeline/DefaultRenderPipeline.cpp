#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "GameFramework/World.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
}

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	FViewportCamera* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FViewportRenderOptions Opts;
		Opts.ViewMode = EViewMode::Lit;
		Opts.ShowFlags = FShowFlags(); // Default show flags

		Bus.SetCameraInfo(
			Camera->GetViewMatrix(),
			Camera->GetProjectionMatrix(),
			Camera->GetForwardVector(),
			Camera->GetRightVector(),
			Camera->GetUpVector(),
			Camera->IsOrthogonal(),
			Camera->GetOrthoWidth());
		Bus.SetRenderOptions(Opts);

		if (ULevel* PersistentLevel = World->GetPersistentLevel())
		{
			PersistentLevel->GetRenderProxy().CollectWorld(Bus, {});
		}
		if (ULevel* ActiveLevel = World->GetActiveLevel())
		{
			ActiveLevel->GetRenderProxy().CollectWorld(Bus, {});
		}

		Bus.CollectViewElements();
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}

void FDefaultRenderPipeline::Reset()
{
	Bus.Reset();
}
