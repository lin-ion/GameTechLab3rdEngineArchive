# Instanced Static Mesh Transparent And Shadow Extension Plan

## Goals

- Phase 1 MVP의 `UInstancedStaticMeshComponent`를 transparent surface material까지 확장한다.
- transparent material이 붙은 instanced static mesh가 기존 `Transparent` pass와 blend/depth state 정책을 따르게 한다.
- shadow map instanced draw는 transparent 지원이 완료된 뒤 마지막 별도 phase로만 포함한다.
- shadow 작업이 문제를 만들면 즉시 discard/defer할 수 있도록 transparent 변경과 섞지 않는다.
- shadow 작업이 별도 pass refactor로 커지면 defer하고, projectile visibility는 bright/emissive material로 보장한다.
- 모든 Phase는 `ReleaseBuild.bat < NUL` 기준으로 빌드 검증 가능해야 한다.

## No Goals

- per-instance material override는 지원하지 않는다.
- per-instance transparent sorting은 지원하지 않는다.
- OIT, weighted blended transparency, depth peeling은 구현하지 않는다.
- 서로 다른 component/proxy의 cross-proxy batching은 하지 않는다.
- `UBulletHellComponent` gameplay 로직은 이 문서 범위에 포함하지 않는다.

## Implementation Policy

- transparent 확장은 기존 `FDrawCommandBuilder`의 section pass routing을 따른다.
- current baseline: `FDrawCommandBuilder::ResolveSectionShader`는 이미 `EVertexFactoryType::InstancedStaticMesh`를 `UberTransparent` instanced entry point로 라우팅한다.
- resolved: `UberTransparent.hlsl`에는 `VS_InstancedStaticMesh(VS_Input_InstancedPNCTT)` entry point가 있으며, non-graph `UberLit` 기반 transparent surface material은 ISMC instance transform을 유지한다.
- resolved follow-up: graph/generated surface material도 generated surface shader가 재컴파일된 경우 `VS_InstancedStaticMesh` entry point를 포함하고, `FDrawCommandBuilder`가 ISMC section에 해당 entry를 선택한다.
- current limitation: 이전 generator로 만들어진 generated shader 파일은 material graph를 다시 compile/save하기 전까지 instanced entry point가 없을 수 있다.
- transparent sort는 기본적으로 proxy 대표점 기준으로 시작한다. 현재 fallback은 component merged bounds center가 아니라 proxy cached world position이다.
- 같은 ISMC 안의 instance별 back-to-front ordering은 명시적으로 하지 않는다. 이를 하려면 인스턴싱을 깨거나 instance buffer를 매 프레임 카메라 기준으로 재정렬해야 하므로 MVP 범위를 넘는다. 보스 탄막용 bright/emissive 또는 낮은-alpha가 아닌 material을 우선 사용해 artifact를 회피한다.
- shadow는 main draw command path와 다르게 `ShadowMapPass`가 수동 draw loop를 갖고 있으므로 마지막 독립 phase로 둔다.
- projectile material이 밝거나 emissive하면 shadow 누락은 초기 gameplay 검증을 막는 blocker가 아니다.

## Phase 1: Transparent Shader Routing

### Goal

instanced static mesh section이 transparent material을 사용할 때 `Transparent` pass로 라우팅되고, instanced vertex shader entry point로 정상 렌더링되게 한다.

### Tasks

1. `EVertexFactoryType::InstancedStaticMesh`와 shader key가 transparent instanced entry point를 구분하는 현재 routing을 재확인한다.
2. `UberTransparent.hlsl`에 `VS_InstancedStaticMesh(VS_Input_InstancedPNCTT)` entry point를 추가한다.
3. instanced VS input은 opaque instanced static mesh와 같은 instance vertex layout을 사용한다.
4. `VS_InstancedStaticMesh`는 `UberLit.hlsl`의 instanced path와 같은 `InstanceModel * Model` transform 정책을 따른다.
5. material의 `RenderPass`/`BlendState`/`DepthStencil` 도출은 기존 surface transparent 정책을 그대로 따른다.
6. graph/generated transparent surface material은 최초 Phase 1 범위 밖이었지만, 테스트 중 확인된 instance transform 회귀 때문에 surface generator에 instanced VS entry를 추가했다.

### Validation

- `ReleaseBuild.bat < NUL`
- non-graph transparent material을 설정한 ISMC가 화면에 표시된다.
- alpha blend, additive, modulate 중 현재 material system이 지원하는 blend state가 기존 transparent mesh와 동일하게 적용된다.
- 기존 non-instanced transparent static mesh가 회귀하지 않는다.
- opaque material을 설정한 ISMC는 여전히 opaque/predepth 경로를 탄다.

### No Goal

- instance별 depth sorting은 하지 않는다.
- transparent projectile self-overlap artifact를 완전히 해결하지 않는다.
- shadow map은 다루지 않는다.
- graph/generated material의 per-instance sorting은 다루지 않는다.

## Phase 2: Transparent Sort And State Stability

### Goal

transparent instanced static mesh가 기존 transparent command sorting과 render state cache를 깨지 않도록 안정화한다.

### Tasks

1. proxy 대표 sort position을 사용할지, ISMC section에 `bHasSortPos`/`SortWorldPos`를 채워 bounds center를 쓸지 결정한다.
2. MVP 기본값은 기존 command builder fallback인 proxy cached world position을 유지한다.
3. moving instance bounds center가 sort key에 stale로 남지 않게 transform/bounds dirty와 연결한다.
4. transparent pass에서 instance VB slot 1 unbind가 기존 draw에 영향을 주지 않는지 확인한다.
5. transparent material 상태 변경 시 section pass가 즉시 갱신되게 한다.

### Validation

- `ReleaseBuild.bat < NUL`
- camera를 움직일 때 transparent ISMC가 기존 transparent mesh/font/particle과 큰 순서 회귀 없이 섞인다.
- material을 opaque에서 transparent로, transparent에서 opaque로 바꿨을 때 pass routing이 갱신된다.
- instance transform update 후 sort 대표 위치가 stale로 남아 심한 정렬 오류를 만들지 않는다.

### No Goal

- 같은 ISMC 내부의 instance별 back-to-front 정렬은 하지 않는다.
- transparent object 사이의 모든 artifact를 해결하지 않는다.
- DoF/CoC 같은 translucent post-process integration은 하지 않는다.

## Phase 3: Transparent Validation Cleanup

### Goal

shadow 작업에 들어가기 전에 transparent ISMC 결과를 독립적으로 닫고, 이후 shadow phase를 discard해도 transparent 지원이 남을 수 있게 한다.

### Tasks

1. transparent ISMC에 필요한 shader/code 변경만 남기고 임시 debug log를 제거한다.
2. non-graph material 지원, graph/generated material의 재컴파일 조건, per-instance sorting 제한을 문서/주석 중 필요한 위치에 남긴다.
3. opaque ISMC, transparent ISMC, 기존 transparent static mesh가 같은 command path에서 회귀 없이 동작하는지 확인한다.
4. shadow phase 시작 전 `git diff` 기준으로 transparent 변경 묶음과 shadow 변경 묶음이 구분 가능한 상태인지 확인한다.

### Validation

- `ReleaseBuild.bat < NUL`
- opaque ISMC는 기존 opaque/predepth 경로를 유지한다.
- transparent ISMC는 `Transparent` pass에서 blend/depth-read-only 정책을 따른다.
- 기존 non-instanced transparent static mesh가 회귀하지 않는다.

### No Goal

- shadow map support는 이 Phase 안에서 구현하지 않는다.
- graph/generated material의 per-instance sorting은 이 Phase 안에서 구현하지 않는다.

## Phase 4: Shadow Map Feasibility Gate

### Goal

shadow map instanced draw가 작은 작업으로 닫히는지 확인하고, 마지막 shadow phase 진행 또는 defer를 결정한다.

### Tasks

1. `ShadowMapPass.cpp`의 caster loop가 현재 proxy-level `FDrawCommandBuffer`만 바인딩하고 section-level `BufferOverride`를 무시하는 것을 확인한다.
2. `ShadowDepth.hlsl`에 instanced static mesh VS entry point를 추가하는 범위를 산정한다.
3. `EShadowDepthDefines::EVertexFactory::InstancedStaticMesh`는 enum에 있으나 `MakePermutationKey`와 `GetOrCreateShadowDepthPermutation`이 static/skeletal만 처리하는 현재 gap을 확인한다.
4. VSM과 non-VSM 경로 모두에서 PS null/PS bound 정책이 유지되는지 확인한다.
5. 작업이 section effective buffer 선택, instance VB slot 1 bind/unbind, `DrawIndexedInstanced` 추가 수준이면 마지막 phase로 진행한다.
6. 작업이 `ShadowMapPass`의 큰 구조 변경, draw command 재통합, cascade scheduling 변경을 요구하면 shadow support를 defer한다.

### Decision

- 진행한다.
- 확인된 gap: `ShadowDepth.hlsl`에는 아직 `VS_InstancedStaticMesh`가 없고, `EShadowDepthDefines::MakePermutationKey`/`FShaderManager::GetOrCreateShadowDepthPermutation`은 `InstancedStaticMesh`를 static mesh permutation으로 처리한다.
- 확인된 gap: `FShadowMapPass::DrawShadowCasters`는 section loop를 돌지만 proxy-level `ProxyBuffer`만 바인딩하고, ISMC가 채우는 `Section.BufferOverride`와 instance VB slot 1을 사용하지 않는다. 현재 ISMC `PrepareDrawBuffer`는 section override를 갱신한 뒤 proxy buffer를 비워두므로 shadow caster loop에서 통째로 skip된다.
- 예상 작업 범위는 `ShadowDepth` instanced VS 추가, instanced shadow permutation 선택, section effective buffer 선택, `DrawIndexedInstanced`/slot 1 bind-unbind 추가로 한정된다.
- VSM과 non-VSM의 PS 정책은 현재 `BoundShader` 전환 직후 non-VSM에서 PS를 null로 바인딩하는 구조를 유지하면 된다.
- transparent section은 `ShouldDrawShadowSection`이 `Opaque` section만 통과시키므로 계속 shadow caster에서 제외된다.
- cascade scheduling, atlas allocation, draw command 기반 pass 재통합은 필요하지 않다.

### Validation

- `ReleaseBuild.bat < NUL`
- 코드 변경 없이 feasibility만 하는 경우, 결정 내용을 이 문서에 반영한다. 완료: 위 Decision에 반영했다.
- 진행 결정 시 마지막 shadow Phase로 넘어가기 전에 기존 static/skeletal shadow가 정상 렌더링되는지 확인한다.

### No Goal

- 이 Phase 안에서 shadow support를 구현하지 않는다.
- transparent shadow나 alpha-tested shadow는 다루지 않는다.

## Phase 5: Optional Instanced Shadow Map Support

### Goal

Phase 4에서 작게 끝난다고 판단된 경우에만 ISMC opaque section을 shadow caster로 렌더링한다. 이 phase는 마지막 구현 phase이며, 문제 발생 시 통째로 discard/defer할 수 있어야 한다.

### Tasks

1. `ShadowDepth.hlsl`에 `VS_InstancedStaticMesh`를 추가한다.
2. `EShadowDepthDefines::MakePermutationKey`와 `FShaderManager::GetOrCreateShadowDepthPermutation`에 instanced static mesh permutation을 추가한다.
3. `ShadowMapPass` caster loop에서 section별 effective buffer를 고른다. `Section.BufferOverride.HasBuffers()`면 section buffer를, 아니면 proxy buffer를 사용한다.
4. instanced section이면 slot 1 instance VB를 바인딩한다.
5. instanced section은 `DrawIndexedInstanced`를 호출하고, non-instanced section은 기존 `DrawIndexed` 경로를 유지한다.
6. shadow draw 후 instance VB slot 1을 unbind한다.
7. transparent section은 기존 `ShouldDrawShadowSection` 정책처럼 shadow caster에서 제외한다.
8. static/skeletal shadow path의 shader binding, skin matrix SRV binding, VSM PS binding 정책은 변경하지 않는다.

### Result

- 구현 완료.
- `ShadowDepth.hlsl`에 `VS_InstancedStaticMesh(VS_Input_InstancedPNCTT)`를 추가했다.
- shadow depth shader permutation은 `InstancedStaticMesh` vertex factory일 때 instanced VS entry를 선택한다.
- `ShadowMapPass` caster loop는 section별 effective buffer를 사용하며, ISMC section의 `Section.BufferOverride`를 통해 slot 0 static mesh VB/IB와 slot 1 instance VB를 바인딩한다.
- instanced shadow section은 `DrawIndexedInstanced`를 사용하고, draw 후 instance VB slot 1을 unbind한다.
- non-instanced static mesh, skeletal mesh, skin matrix SRV, VSM/non-VSM PS binding 정책은 기존 경로를 유지한다.
- transparent/alpha blended section은 `ShouldDrawShadowSection`의 `Opaque` pass filter 때문에 shadow caster에서 제외된다.

### Validation

- `ReleaseBuild.bat < NUL`
- ISMC opaque projectile 또는 cube grid가 directional light shadow를 만든다.
- 기존 static mesh, skeletal mesh shadow가 회귀하지 않는다.
- VSM과 non-VSM mode에서 crash 없이 동작한다.
- shadow를 끄면 ISMC shadow caster draw가 사라진다.
- 문제가 생기면 이 phase의 변경만 revert해도 transparent ISMC 지원이 유지된다.

### No Goal

- transparent/alpha blended projectile shadow는 지원하지 않는다.
- per-instance shadow culling은 하지 않는다.
- shadow pass를 draw command 기반으로 전면 리팩터링하지 않는다.

## Phase 6: Final Cleanup And Documentation

### Goal

transparent와 optional shadow 확장 결과를 정리하고, `UBulletHellComponent`가 의존할 수 있는 ISMC 기능 경계를 문서화한다.

### Tasks

1. 임시 debug log와 hardcoded test material path를 제거한다.
2. public API와 unsupported behavior를 주석으로 정리한다.
3. transparent sorting 제한과 shadow 지원 여부를 명확히 문서화한다.
4. Phase 4/5에서 shadow를 defer한 경우, defer 조건과 재개 조건을 남긴다.
5. `UBulletHellComponent` 계획서가 참조할 ISMC API 목록을 확정한다.

### Final ISMC Capability Boundary

- `UInstancedStaticMeshComponent` supports opaque and transparent surface material routing through the normal section command path.
- Non-graph `UberLit` transparent materials use `UberTransparent` with `VS_InstancedStaticMesh`.
- Graph/generated surface materials support ISMC transforms after the material graph is recompiled/saved with the generator that emits `VS_InstancedStaticMesh`.
- Transparent ISMC sorting is proxy-level only. Instances inside one ISMC are not sorted back-to-front.
- Opaque ISMC sections cast shadows through `ShadowMapPass` using `ShadowDepth::VS_InstancedStaticMesh` and `DrawIndexedInstanced`.
- Transparent/alpha-blended ISMC sections do not cast shadows.
- No per-instance material override, per-instance shadow culling, per-instance transparent sorting, OIT, weighted transparency, alpha-tested shadow, or shadow-pass draw-command refactor is included.
- `UBulletHellComponent` can rely on `SetStaticMesh`/`SetStaticMeshByPath`, `SetMaterial`/`SetMaterialByPath`, `ReserveInstances`, `SetInstances`, `AddInstance`, `UpdateInstanceTransform`, `RemoveInstanceSwap`, `ClearInstances`, `GetInstanceCount`, and `GetInstanceTransform`.

### Validation

- `ReleaseBuild.bat < NUL`
- opaque ISMC, transparent ISMC, 기존 static mesh를 같은 scene에서 확인한다.
- shadow를 구현한 경우 기존 shadow demo scene 또는 간단한 test scene에서 회귀를 확인한다.
- `rg`로 임시 test-only path가 production path에 남지 않았는지 확인한다.

### No Goal

- bullet gameplay 구현은 하지 않는다.
- editor-facing final UI는 만들지 않는다.
