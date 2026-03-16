#include "pch.h"
#include "CubeComponent.h"
#include "Mesh.h"

void UCubeComponent::Render(ID3D11DeviceContext& DevcieContext)
{
    MeshAsset->Draw(DevcieContext);
}
