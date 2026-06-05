# Instanced Static Mesh Component MVP Plan

## Goals

- `UInstancedStaticMeshComponent`와 `FInstancedStaticMeshSceneProxy`를 추가한다.
- 하나의 컴포넌트가 동일한 static mesh/material을 여러 transform으로 `DrawIndexedInstanced` 렌더링할 수 있게 한다.
- `AddInstance`, `UpdateInstanceTransform`, `RemoveInstanceSwap`, `ClearInstances`, `GetInstanceCount` API를 제공한다.
- instance transform 변경이 render proxy, world bounds, octree/frustum culling에 반영되게 한다.
- Phase 1부터 임시 검증 컴포넌트 또는 테스트 액터를 제공해 instance API를 즉시 화면/로그로 확인한다.
- debug draw로 CPU-side instance 위치를 표시해서 렌더링된 instance 위치와 비교할 수 있게 한다.
- 모든 Phase는 `ReleaseBuild.bat < NUL` 기준으로 빌드 검증 가능해야 한다.

## No Goals

- 서로 다른 액터/컴포넌트의 instance를 renderer 수집 단계에서 cross-proxy batching하지 않는다.
- per-instance material override는 지원하지 않는다.
- transparent material 지원은 이 문서 범위에 포함하지 않는다.
- shadow map instanced draw는 이 문서 범위에 포함하지 않는다.
- per-instance picking/collision result는 이 문서 범위에 포함하지 않는다.
- `UBulletHellComponent`의 gameplay update/collision/lifetime 로직은 이 문서 범위에 포함하지 않는다.

## Implementation Policy

- `UInstancedStaticMeshComponent`는 `UStaticMeshComponent`와 같은 mesh/material authoring 흐름을 최대한 재사용한다.
- `UInstancedStaticMeshComponent`는 gameplay 의미를 갖지 않는다. instance transform 저장, bounds 갱신, render dirty 전파, render-facing API만 담당한다.
- 순서 보존 삭제는 초기 범위에서 제외하고 `RemoveInstanceSwap`을 기본 삭제 방식으로 둔다.
- instance buffer는 proxy가 render-facing transient resource로 소유한다. component는 CPU-side transform 배열과 dirty state만 소유한다.
- 첫 렌더링 경로는 opaque main pass만 대상으로 한다. 이는 기능 제한이 아니라 가장 작은 검증 단위다.
- LOD는 MVP에서 LOD0만 사용한다. 기존 static mesh LOD 전환과 instance buffer 결합은 후속 확장으로 둔다.
- 임시 검증 actor/component는 Phase 1부터 만든다. GPU instancing이 들어오기 전에는 debug draw와 로그로 API/bounds를 확인하고, Phase 2 이후에는 같은 debug draw를 렌더 결과 위치 검증 기준으로 사용한다.

## Phase 1: Component Data Model, API, And Validation Harness

### Goal

`UInstancedStaticMeshComponent`의 CPU-side instance 배열과 public API를 만들고, 즉시 검증 가능한 임시 테스트 actor/component를 제공한다. 이 Phase의 완료 기준은 GPU instanced mesh draw 없이도 instance 추가, 갱신, swap 삭제, clear, bounds dirty, debug draw 위치 표시를 확인할 수 있는 것이다.

### Tasks

1. `Source/Engine/Component/Primitive/InstancedStaticMeshComponent.h/.cpp`를 추가한다.
2. `UInstancedStaticMeshComponent`는 static mesh와 material slot/override 처리를 `UStaticMeshComponent`와 같은 정책으로 제공한다.
3. CPU-side `TArray<FMatrix>` 또는 `TArray<FTransform>` instance storage를 추가한다.
4. `AddInstance`, `UpdateInstanceTransform`, `RemoveInstanceSwap`, `ClearInstances`, `GetInstanceCount`, `GetInstanceTransform` API를 추가한다.
5. `RemoveInstanceSwap`은 마지막 instance를 삭제 위치로 이동시키고, 호출자에게 이동된 index를 알릴 수 있는 반환 정책을 정한다.
6. instance 변경 시 render dirty와 world bounds dirty를 마킹한다.
7. component class가 reflection/header generation 대상에 포함되도록 project/header generation 경로를 갱신한다.
8. editor 또는 runtime에서 배치 가능한 임시 validation actor/component를 추가한다.
9. validation actor/component는 mesh path, material path, grid count, spacing, update toggle, random removal toggle을 최소 설정으로 제공한다.
10. validation actor/component는 Phase 1에서는 CPU-side instance 위치를 debug draw cross/sphere/AABB로 표시한다.
11. debug draw는 instance index별 위치를 확인할 수 있도록 색상 또는 선택된 index 강조 정책을 둔다.

### Validation

- `ReleaseBuild.bat < NUL`
- validation actor/component에서 instance를 0개, 1개, 다수 추가하고 `GetInstanceCount`가 정확한지 확인한다.
- `RemoveInstanceSwap` 후 마지막 instance가 삭제 위치로 이동하고 count가 감소하는지 로그 또는 디버거로 확인한다.
- clear 후 count가 0이고 world bounds가 invalid/fallback 상태로 돌아가는지 확인한다.
- debug draw가 CPU-side instance transform 위치에 표시되는지 확인한다.
- update toggle을 켰을 때 debug draw 위치가 움직이고, random removal toggle을 켰을 때 debug draw 개수가 줄어드는지 확인한다.

### No Goal

- GPU buffer 생성은 하지 않는다.
- instanced static mesh surface 렌더링은 하지 않는다.
- instance별 picking result는 만들지 않는다.

## Phase 2: Scene Proxy And Opaque Instanced Draw

### Goal

`FInstancedStaticMeshSceneProxy`가 component의 instance transform을 동적 instance VB로 업로드하고, static mesh LOD0 VB/IB와 결합해서 opaque main pass에서 `DrawIndexedInstanced`로 그리게 한다.

### Tasks

1. `Source/Engine/Render/Proxy/InstancedStaticMeshSceneProxy.h/.cpp`를 추가한다.
2. `UInstancedStaticMeshComponent::CreateSceneProxy()`가 새 proxy를 반환하게 한다.
3. proxy는 `EPrimitiveProxyFlags::StaticMesh`를 유지하되 instanced static mesh 식별이 필요하면 별도 flag 추가를 검토한다.
4. instance vertex 구조체를 추가한다. 최소 필드는 world matrix row 4개와 optional color로 둔다.
5. `FMeshSectionDraw::BufferOverride`에 static mesh VB/IB와 instance VB/count/stride를 채운다.
6. `EVertexFactoryType` 또는 shader routing에 instanced static mesh를 구분할 수 있는 값을 추가한다.
7. `UberLit.hlsl`에 instanced static mesh VS entry point를 추가하고 `INSTANCE_` semantic으로 slot 1 input layout을 구성한다.
8. `ShaderManager`의 UberLit permutation key/entry point 선택에 instanced static mesh를 추가한다.
9. `FDrawCommandBuilder::ResolveSectionShader`가 instanced static mesh section에 맞는 shader를 선택하게 한다.
10. `KraftonEngine.vcxproj`와 `.filters`에 새 C++/HLSL 파일을 반영한다.

### Validation

- `ReleaseBuild.bat < NUL`
- 임시 테스트 액터/컴포넌트가 같은 cube 또는 sphere mesh를 여러 위치에 한 번의 instanced section으로 렌더링한다.
- debug draw 위치와 실제 렌더링된 instance 위치가 일치해야 한다.
- instance count를 1, 100, 1000으로 바꿔도 화면에 모두 표시된다.
- 기존 `UStaticMeshComponent` 렌더링이 회귀하지 않는다.
- RenderDoc 또는 debug log가 가능하면 `DrawIndexedInstanced` 호출과 instance count를 확인한다.

### No Goal

- transparent pass는 다루지 않는다.
- shadow map pass는 다루지 않는다.
- per-instance sorting은 하지 않는다.
- per-instance material override는 하지 않는다.

## Phase 3: Bounds, Dirty Update, And Removal Stability

### Goal

instance transform 변경과 삭제가 bounds, octree/frustum culling, proxy update에 안정적으로 반영되게 한다. 이 Phase의 완료 기준은 움직이는/삭제되는 instance가 화면과 culling에서 stale 상태를 남기지 않는 것이다.

### Tasks

1. component `UpdateWorldAABB()`에서 static mesh local bounds를 모든 instance transform에 적용해 합산 world bounds를 계산한다.
2. instance가 0개일 때의 bounds fallback 정책을 정한다.
3. instance transform 변경 시 `MarkRenderStateDirty` 또는 필요한 경량 dirty flag가 정확히 전파되게 한다.
4. proxy는 component instance snapshot을 읽고 dirty frame에 instance VB를 갱신한다.
5. `RemoveInstanceSwap` 후 이동된 instance의 transform이 render buffer에 즉시 반영되게 한다.
6. 대량 update를 위한 `SetInstancesBulk` 또는 `UpdateInstancesBulk` 도입 여부를 결정한다. 도입한다면 이 Phase에서 render dirty를 한 번만 발생시키는 경로로 구현한다.

### Validation

- `ReleaseBuild.bat < NUL`
- 화면 밖에 있던 instance 묶음이 카메라/frustum 안으로 들어오면 렌더링되고, 밖으로 나가면 culling된다.
- 매 프레임 instance 위치를 갱신해도 debug draw와 렌더링된 instance 사이에 stale 위치 차이가 남지 않는다.
- 랜덤 삭제와 swap 삭제를 반복해도 debug draw와 렌더링된 instance 모두 잘못된 위치의 instance를 남기지 않는다.
- instance 0개 상태에서 crash 없이 렌더링을 skip한다.

### No Goal

- per-instance occlusion culling은 하지 않는다.
- per-instance physics body는 만들지 않는다.
- gameplay bullet handle은 만들지 않는다.

## Phase 4: Validation Harness Stress Modes And Cleanup

### Goal

Phase 1에서 만든 임시 validation actor/component를 stress 검증용으로 확장하고, `UBulletHellComponent`로 넘어가기 전 test-only 경계를 정리한다. 이 Phase는 새 테스트 경로를 처음 만드는 단계가 아니라 Phase 1~3의 검증 하네스를 반복 검증 가능한 상태로 마무리하는 단계다.

### Tasks

1. validation actor/component에 대량 count preset을 추가한다.
2. update mode에서는 instance transform을 주기적으로 갱신한다.
3. removal mode에서는 일부 instance를 `RemoveInstanceSwap`으로 제거한다.
4. debug draw 표시 모드를 all, selected index, bounds-only 중 선택할 수 있게 한다.
6. 이 임시 경로가 추후 `UBulletHellComponent`로 대체될 수 있음을 주석과 문서에 명시한다.
7. production path에 남아도 되는 code와 test-only code를 분리한다.

### Validation

- `ReleaseBuild.bat < NUL`
- 임시 actor를 scene에 배치하고 여러 instance가 보이는지 확인한다.
- transform animation이 실제 렌더링과 debug draw 양쪽에서 같은 위치로 움직이는지 확인한다.
- removal toggle이 instance 수를 줄이고 화면 렌더링과 debug draw 양쪽에서 사라지는지 확인한다.
- selected index debug draw가 `GetInstanceTransform` 결과 위치를 정확히 가리키는지 확인한다.
- 기존 static mesh actor와 함께 배치해도 render pass나 material state가 섞이지 않는다.

### No Goal

- gameplay collision, projectile lifetime, damage 처리는 하지 않는다.
- final editor UX를 만들지 않는다.
- test actor를 장기 public API로 고정하지 않는다.
