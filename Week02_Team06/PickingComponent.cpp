#include "pch.h"
#include "PickingComponent.h"

#include "PrimitiveComponent.h"
#include "CameraComponent.h"
#include "Actor.h"
#include "Mesh.h"

#include "Input.h"

bool UPickingComponent::IsPicked(const UMesh* MeshData, FMatrix World)
{
    FVector CameraRay = {};
    FVector CameraPos = {};

    //카메라의 현재 레이를 가져옴
    //현재 카메라를 어떻게 가져올지
    for (size_t i = 0; i < GUObjectArray.Size(); ++i)
    {
        if (dynamic_cast<UCameraComponent*>(GUObjectArray[i]))
        {
            CameraRay = static_cast<UCameraComponent*>(GUObjectArray[i])->GetCameraRayDirection();
            CameraPos = static_cast<UCameraComponent*>(GUObjectArray[i])->GetComponentLocation();
            break;
        }
    }

    CameraRay = FMatrix::TransformNormal(CameraRay, World.Inverse());
    CameraPos = FMatrix::TransformCoord(CameraPos, World.Inverse());

    const FVertexSimple* VertexBuffer = static_cast<const FVertexSimple*>(MeshData->GetVertexData());

    for (int32 i = 0; i < MeshData->GetVertexCount(); i += 3)
    { 
        if (RayIntersectsTriangle(CameraPos, CameraRay, VertexBuffer[i], VertexBuffer[i + 1], VertexBuffer[i + 2]))
        {
            return true;
        }
    }


    return false;
}

bool UPickingComponent::RayIntersectsTriangle(const FVector& CameraPos, const FVector& CameraRay, const FVertexSimple& V0, const FVertexSimple& V1, const FVertexSimple& V2)
{

    FVector Vertex0 = { V0.X, V0.Y, V0.Z };
    FVector Vertex1 = { V1.X, V1.Y, V1.Z };
    FVector Vertex2 = { V2.X, V2.Y, V2.Z };

    FVector Edge1 = Vertex1 - Vertex0;
    FVector Edge2 = Vertex2 - Vertex0;

    FVector Normal = Edge1.FVector::Cross(Edge2);

    //후면인지 판단
    if (Normal.FVector::Dot(CameraRay) > 0)
    {
        return false;
    }
    constexpr float epsilon = std::numeric_limits <float> ::epsilon();

    // det 계산 — 0이면 광선이 삼각형 평면과 평행
    FVector RayCrossE2 = CameraRay.Cross(Edge2);
    float   Det = Edge1.Dot(RayCrossE2);
    if (abs(Det) < epsilon) return false;

    float   InvDet = 1.f / Det;
    FVector S = CameraPos - Vertex0;

    // u 범위 체크
    float U = InvDet * S.Dot(RayCrossE2);   
    if (U < 0.f || U > 1.f) return false;

    // v 범위 체크
    FVector SCrossE1 = S.Cross(Edge1);
    float   V = InvDet * CameraRay.Dot(SCrossE1);
    if (V < 0.f || U + V > 1.f) return false;

    // t > 0 이면 광선 방향 앞쪽에 교차점 존재
    float T = InvDet * Edge2.Dot(SCrossE1);
    //위치 겟ㄴ은 나온 T를 이용하여 Origin + t * Ray방향벡터로 계산
    return T > epsilon;
}
