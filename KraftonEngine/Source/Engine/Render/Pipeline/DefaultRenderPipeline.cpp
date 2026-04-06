#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Component/CameraComponent.h"
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
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FViewportRenderOptions Opts;
		Opts.ViewMode = EViewMode::Lit;
		Opts.ShowFlags = FShowFlags(); // Default show flags

		Bus.SetCameraInfo(Camera);
		Bus.SetRenderOptions(Opts);

		if (UScene* Scene = World->GetPersistentScene())
		{
			Scene->GetRenderProxy().CollectWorld(Bus, {});
		}
		if (UScene* Scene = World->GetActiveScene())
		{
			Scene->GetRenderProxy().CollectWorld(Bus, {});
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
