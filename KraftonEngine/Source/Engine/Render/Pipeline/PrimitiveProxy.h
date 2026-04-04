#pragma once
#include "Render/Pipeline/RenderCommand.h"
#include "Render/Pipeline/ViewContext.h"

class UPrimitiveComponent;

class FPrimitiveProxy
{
public:
	FPrimitiveProxy(UPrimitiveComponent* InOwner);
	virtual ~FPrimitiveProxy() = default;

	virtual void UpdateProxy() = 0;
	virtual void OnDraw(FViewContext& View);

	void MarkDirty() { bIsDirty = true; }
	bool IsDirty() const { return bIsDirty; }

	UPrimitiveComponent* GetOwner() const { return Owner; }
	void SetSelected(bool bInSelected) { bSelected = bInSelected; }

protected:
	UPrimitiveComponent* Owner;
	FRenderCommand CachedCommand;
	bool bIsDirty = true;
	bool bSelected = false;
};

class FDefaultPrimitiveProxy : public FPrimitiveProxy
{
public:
	FDefaultPrimitiveProxy(UPrimitiveComponent* InOwner);
	void UpdateProxy() override;
};
