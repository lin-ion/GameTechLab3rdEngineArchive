#pragma once

#include "pch.h"
#include "PrimitiveComponent.h"
#include "PickingComponent.h"
#include "App.h"
#include "ResourceManager.h"
#include "FEditorViewportClient.h"
#include "Renderer.h"
#include "Math.h"

enum class EGizmoAxis { None, Center, X, Y, Z };

class UGizmoComponent : public UPrimitiveComponent
{
public:
    virtual void Render(ID3D11DeviceContext& Context) override 
    {
        UResourceManager* ResMgr = GApp->GetResourceManager();
        UMesh* ArrowMesh = ResMgr->FindMeshData("Gizmo");
        UMesh* SphereMesh = ResMgr->FindMeshData("Sphere");
        if (!ArrowMesh) return;

        FEditorViewportClient* Viewport = GApp->GetViewportClient();

        // 1. 카메라 행렬 획득 (View * Projection)
        FMatrix ViewProj = Viewport->GetViewMatrix() * Viewport->GetProjectionMatrix();

        // 2. 역스케일링 계산
        float Distance = (Viewport->GetViewLocation() - GetComponentLocation()).Length();
        float ScaleFactor = Distance * 0.15f;  

        // 3. 기즈모의 월드 행렬 생성
        // GetTransform 대신 SceneComponent에 정의된 GetComponentTransform() 사용
        FMatrix Model = FMatrix::MakeScale({ ScaleFactor, ScaleFactor, ScaleFactor }) * GetComponentTransform();

        // 4. 최종 MVP 계산
        FMatrix MVP = Model * ViewProj;

        // Gizmo의 Sphere 및 Arrow 렌더링
        if (SphereMesh)
        {
            FMatrix SphereModel = FMatrix::MakeScale({ ScaleFactor * 0.1f, ScaleFactor * 0.1f, ScaleFactor * 0.1f }) * GetComponentTransform();
            GApp->GetRenderer()->UpdateConstantBuffer(Context, SphereModel * ViewProj, { 1.f, 1.f, 1.f, 1.f});
			SphereMesh->Draw(Context);
        }

        if (ArrowMesh)
        {
            GApp->GetRenderer()->UpdateConstantBuffer(Context, Model * ViewProj, { 0.f, 1.f, 0.f, 1.f });
            ArrowMesh->Draw(Context);
            
            FMatrix XRotation = FMatrix::MakeRotationZ(-90.0f);
            GApp->GetRenderer()->UpdateConstantBuffer(Context, (XRotation * Model) * ViewProj, { 1.0f, 0.f, 0.f, 1.f });
            ArrowMesh->Draw(Context);
              
            FMatrix ZRotation = FMatrix::MakeRotationX(90.0f);
            GApp->GetRenderer()->UpdateConstantBuffer(Context, (ZRotation * Model) * ViewProj, { 0.f, 0.f, 1.f, 1.f });
            ArrowMesh->Draw(Context);
        }
    }

    EGizmoAxis CheckGizmoPicking(UPickingComponent* Picker)
    {
        if (!Picker) return EGizmoAxis::None;

        UResourceManager* ResMgr = GApp->GetResourceManager();
        UMesh* ArrowMesh = ResMgr->FindMeshData("Gizmo");
        UMesh* SphereMesh = ResMgr->FindMeshData("Sphere");

        FEditorViewportClient* Viewport = GApp->GetViewportClient();
        float Distance = (Viewport->GetViewLocation() - GetComponentLocation()).Length();
        float ScaleFactor = Distance * 0.15f;
        FMatrix BaseModel = FMatrix::MakeScale({ ScaleFactor, ScaleFactor, ScaleFactor }) * GetComponentTransform();

        // 2. 각 축 화살표 검수 (렌더링 때와 동일한 회전 적용)
        if (ArrowMesh)
        {
            // Y축 (Green - 기본)
            if (Picker->IsPicked(ArrowMesh, Viewport->GetViewLocation(), Viewport->GetCameraRayDirection(), BaseModel)) return EGizmoAxis::Y;

            // X축 (Red - Z축 -90도 회전)
            FMatrix XRotation = FMatrix::MakeRotationZ(-90.0f);
            if (Picker->IsPicked(ArrowMesh, Viewport->GetViewLocation(), Viewport->GetCameraRayDirection(), XRotation * BaseModel)) return EGizmoAxis::X;

            // Z축 (Blue - X축 90도 회전)
            FMatrix ZRotation = FMatrix::MakeRotationX(90.0f);
            if (Picker->IsPicked(ArrowMesh, Viewport->GetViewLocation(), Viewport->GetCameraRayDirection(), ZRotation * BaseModel)) return EGizmoAxis::Z;
        }

        return EGizmoAxis::None;
    }
};