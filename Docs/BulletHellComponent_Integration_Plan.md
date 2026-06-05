# Bullet Hell Component Integration Plan

## Goals

- `UBulletHellComponent`를 추가해 탄막 projectile을 actor-per-bullet이 아닌 data-oriented instance 배열로 관리한다.
- `FBulletInstance`를 tick하면서 위치, 수명, 충돌/overlap, 제거를 처리한다.
- 살아있는 bullet transform을 `UInstancedStaticMeshComponent`에 bulk 반영한다.
- `Linear`, `Homing`, `ColdLaunch` 같은 기본 behavior를 확장 가능한 형태로 둔다.
- ISMC 검증용 임시 컴포넌트는 gameplay path와 분리하고, 탄막 검증은 `UBulletHellComponent`의 debug draw/stat/preset으로 수행한다.
- 모든 Phase는 `ReleaseBuild.bat < NUL` 기준으로 빌드 검증 가능해야 한다.

## No Goals

- 탄 하나하나를 actor로 생성하지 않는다.
- 서로 다른 actor/component의 renderer-side cross-proxy batching은 하지 않는다.
- per-instance material override는 지원하지 않는다.
- 네트워크 동기화, save/load, replay determinism은 초기 범위에 포함하지 않는다.
- 복잡한 projectile AI나 독립 component tree가 필요한 특수 투사체는 이 component의 기본 경로에 넣지 않는다.

## Implementation Policy

- `UBulletHellComponent`는 새 gameplay component로 추가한다. 기존 `UInstancedStaticMeshValidationComponent`를 개량/상속하지 않는다.
- `UBulletHellComponent`는 `UActorComponent` 계열 gameplay state owner다.
- `UInstancedStaticMeshComponent`는 render instance owner다. bullet lifetime, collision, damage, behavior를 알지 않는다.
- `UInstancedStaticMeshValidationComponent`는 ISMC 개발/회귀/stress 검증용 debug harness로 남긴다. gameplay 의미를 추가하지 않는다.
- 초기 Phase에서는 ISMC render binding 없이도 bullet 위치를 확인할 수 있도록 `UBulletHellComponent` 자체 debug draw를 제공한다. 이 debug draw는 validation component의 cross/sphere/bounds 표시 스타일을 참고하되 gameplay bullet state에서 직접 그린다.
- debug stat은 Phase 1부터 둔다. active/spawned/killed/expired/debug-draw-selected 같은 runtime count를 먼저 검증하고, render/collision/behavior Phase에서 render instance count, collision query/hit count, behavior transition count를 확장한다.
- bullet 삭제는 `RemoveInstanceSwap`과 맞는 swap-remove를 기본으로 한다.
- 외부 참조가 필요하면 array index를 직접 노출하지 않고 stable handle 또는 id/generation을 사용한다.
- collision MVP는 engine에 준비된 query 수준에 맞춘다. 현재 코드베이스에는 `UWorld::PhysicsSweep`, `UWorld::PhysicsSweepByObjectTypes`, `FCollisionShape::MakeSphere`가 있으므로 bullet radius 기반 sphere sweep을 1차 후보로 둔다. 비용이나 필터 정책 문제가 확인될 때 overlap/AABB fallback을 검토한다.
- mesh/material이 여러 종류면 `UBulletHellComponent`가 archetype별 ISMC를 여러 개 소유한다.

## Current Codebase Alignment

- `UInstancedStaticMeshComponent`는 `Source/Engine/Component/Primitive/InstancedStaticMeshComponent.h/.cpp`에 존재하며 `ReserveInstances`, `SetInstances`, `AddInstance`, `UpdateInstanceTransform`, `RemoveInstanceSwap`, `ClearInstances`, `GetInstanceCount`, `GetInstanceTransform`을 제공한다.
- `RemoveInstanceSwap`은 제거 위치로 마지막 render instance를 이동하고, 이동이 발생한 index를 반환한다. Bullet runtime storage도 같은 swap 순서를 유지하거나 `RenderIndex -> BulletIndex` 역매핑을 유지해야 한다.
- gameplay component 위치는 이미 `Source/Engine/Component/Gameplay`가 있고 `URagdollForceHitscanComponent` 같은 `UActorComponent` 기반 예시가 있다. `UBulletHellComponent`도 이 폴더에 둔다.
- `UInstancedStaticMeshValidationComponent`는 `Source/Engine/Component/Debug`의 development-only validation harness이며 헤더 주석상 gameplay semantics를 넣지 않는 것이 기존 의도다.
- 대량 transform 반영은 현재 별도 `UpdateInstancesBulk`가 아니라 `SetInstances(TArray<FTransform>&&)`를 bulk path로 사용한다.
- 충돌은 `ProjectileMovementComponent`가 이미 `PhysicsSweep` + shape sweep 경로를 사용하므로, BulletHell collision도 이 정책을 참고한다.
- debug draw는 `Debug/DrawDebugHelpers.h`의 `DrawDebugSphere`, `DrawDebugLine`, 필요 시 `DrawDebugBox`를 사용한다.

## Phase 1: Bullet Runtime Data Model

### Goal

렌더링과 분리된 bullet runtime state를 만들고, spawn/update/remove가 actor 생성 없이 동작하게 한다.

### Tasks

1. `Source/Engine/Component/Gameplay/BulletHellComponent.h/.cpp`를 추가하고 project/filter 파일에 등록한다.
2. `FBulletInstance`를 추가한다.
3. 필수 필드는 id, generation, position, previous position, velocity, radius, age, lifetime, render instance index, alive flag로 둔다.
4. `FBulletHandle` 또는 동등한 stable id 구조를 추가한다.
5. `SpawnBullet`, `KillBullet`, `ClearBullets`, `GetBulletCount` API를 추가한다.
6. 내부 storage는 `TArray<FBulletInstance>`와 id-to-index `TMap<uint32, int32>` 계열 map을 사용한다.
7. `NextBulletId`와 generation을 분리해 dead handle이 다시 살아있는 bullet을 가리키지 않게 한다.
8. Phase 1에서는 `RenderInstanceIndex`를 `-1`로 유지하고, Phase 2 render binding 이후에 실제 index를 채운다.
9. swap-remove 시 이동된 bullet의 array index와 render instance index를 함께 갱신한다.
10. Phase 1에서는 render component 없이도 tick/lifetime/remove가 검증되도록 render binding을 optional로 둔다.
11. Phase 1부터 bullet debug draw를 추가한다. `bDrawBulletDebug`, `DebugDrawMode`, `HighlightedBulletId`, `DebugDrawMaxCount` 같은 설정을 두고, bullet 위치에 cross와 radius sphere를 그린다.
12. debug draw는 `FBulletInstance::Position`, `Radius`, `Velocity`를 기준으로 그리고, 필요 시 `PreviousPosition -> Position` segment도 표시해 movement/tunneling 문제를 초기에 확인할 수 있게 한다.
13. Phase 1부터 debug stat을 추가한다. 최소 항목은 `ActiveBulletCount`, `TotalSpawned`, `TotalKilled`, `TotalExpired`, `DebugDrawSelectedCount`, `DebugDrawTruncatedCount`다.
14. `GetBulletDebugStats` 또는 로그용 helper를 제공해 debug draw와 count 검증이 같은 runtime state를 보게 한다.
15. debug spawn preset을 추가한다. 최소 설정은 `DebugSpawnCount`, `DebugSpawnSpeed`, `DebugSpawnLifetime`, `DebugSpawnRadius`, `DebugSpawnPattern`이며, line/ring/radial 중 하나 이상을 제공한다.
16. `BeginPlay` 자동 spawn 여부와 manual spawn API를 분리한다. editor/PIE 반복 테스트용 자동 spawn은 toggle로만 켠다.

### Validation

- `ReleaseBuild.bat < NUL`
- render component 연결 없이 bullet spawn/count/kill/clear가 정상 동작하는지 로그 또는 debug command로 확인한다.
- render component 연결 없이도 debug draw로 살아있는 bullet 위치, radius, 이동 방향/segment가 보인다.
- Phase 1 debug stat의 active/spawned/killed/expired count가 실제 spawn/kill/lifetime 처리와 일치한다.
- debug draw max count가 active bullet보다 작을 때 `DebugDrawTruncatedCount`가 명시적으로 표시된다.
- debug spawn preset만으로 별도 gameplay caller 없이 bullet 위치와 lifetime 제거를 확인할 수 있다.
- random kill을 반복해도 handle lookup과 count가 꼬이지 않는다.
- lifetime 만료 bullet이 tick 후 제거된다.
- lifetime 만료 또는 kill된 bullet은 다음 tick/debug draw에서 사라진다.

### No Goal

- 화면 렌더링은 하지 않는다.
- collision query는 하지 않는다.
- homing/cold launch behavior는 구현하지 않는다.

## Phase 2: ISMC Render Binding

### Goal

`UBulletHellComponent`가 `UInstancedStaticMeshComponent`와 연결되어 살아있는 bullet을 화면에 렌더링한다.

### Tasks

1. `UBulletHellComponent`는 `UInstancedStaticMeshComponent`를 상속하지 않는다. 기본 정책은 owner actor에 sibling `UInstancedStaticMeshComponent`를 생성/참조하고, BulletHell은 그 포인터를 render slot으로 소유한다.
2. spawn 시 ISMC `AddInstance`를 호출하고 `RenderInstanceIndex`를 저장한다.
3. kill/swap-remove 시 ISMC `RemoveInstanceSwap`을 호출하고 이동된 render instance index를 bullet state에 반영한다. bullet storage 순서와 render storage 순서가 다를 경우 `RenderIndexToBulletIndex` 역매핑을 갱신한다.
4. tick 후 살아있는 bullet transform 배열을 만들고 ISMC `SetInstances(std::move(Transforms))`로 bulk update한다. 개별 `UpdateInstanceTransform` 반복은 debug/소량 경로로만 둔다.
5. bullet orientation은 velocity 방향 또는 fixed orientation 중 MVP 정책을 정한다.
6. mesh/material 설정은 ISMC의 `SetStaticMeshByPath`, `SetMaterialByPath`를 사용한다.
7. Phase 1 debug stat에 `RenderInstanceCount`, `RendererSlotCount`, `RenderMismatchCount`를 추가한다.
8. Phase 1 debug spawn preset이 ISMC render binding 후에도 같은 bullet state를 사용하게 한다.

### Validation

- `ReleaseBuild.bat < NUL`
- scene에 `UBulletHellComponent`를 배치하면 여러 projectile mesh가 렌더링된다.
- bullet이 이동하고 lifetime이 끝나면 화면에서 사라진다.
- debug draw 위치와 ISMC render instance 위치가 같은 bullet state를 가리킨다.
- debug stat의 active bullet count와 render instance count가 일치하고, 불일치 시 `RenderMismatchCount`가 드러난다.
- random kill 또는 clear를 호출해도 render instance가 stale로 남지 않는다.
- 1000개 이상 bullet에서 actor-per-bullet 없이 하나 또는 소수의 ISMC로 렌더링된다.

### No Goal

- wall/player collision은 하지 않는다.
- behavior state machine은 하지 않는다.
- per-instance picking은 이 Phase의 필수 검증이 아니다.

## Phase 3: Collision And Erase Rules

### Goal

bullet이 벽, 플레이어, 플레이어 스킬 영역과 충돌/overlap되면 제거되는 기본 gameplay loop를 만든다.

### Tasks

1. bullet radius를 사용한 sphere sweep을 MVP 기본 경로로 둔다. `UWorld::PhysicsSweep`와 `FCollisionShape::MakeSphere(Radius)`를 사용한다.
2. `PreviousPosition -> Position` 구간을 보존해 빠른 bullet의 tunneling risk를 줄인다.
3. world static blocker query는 `ECollisionChannel::Projectile` trace channel 또는 object-type sweep 중 실제 응답 정책에 맞는 쪽으로 연결한다.
4. player hit 또는 skill erase volume query는 `Pawn`, `Trigger`, `Projectile` 채널 응답을 분리해 component/channel/filter 정책을 정한다.
5. collision/erase 성공 시 `KillBullet`을 호출하고 render instance도 제거한다.
6. Phase 1/2 debug stat에 `CollisionQueryCount`, `CollisionHitCount`, `CollisionKilledCount`, `EraseKilledCount`를 추가한다.
7. debug draw 또는 stat log로 checked/hit/killed count를 확인할 수 있게 한다.
8. sweep 비용이 큰 경우에만 broadphase AABB 또는 distance overlap fallback을 추가하고, fallback 사용 시 max delta 또는 radius expansion 제한을 문서화한다.

### Validation

- `ReleaseBuild.bat < NUL`
- 벽을 향해 이동한 bullet이 벽 근처에서 제거된다.
- player 또는 skill erase volume과 겹친 bullet이 제거된다.
- 같은 frame에 여러 bullet이 제거되어도 swap-remove가 render index를 깨지 않는다.
- collision/erase debug stat이 query/hit/killed count를 분리해서 보여준다.
- collision off 설정에서는 bullet이 lifetime까지 유지된다.

### No Goal

- 물리 rigidbody response를 만들지 않는다.
- per-bullet actor overlap event를 발생시키지 않는다.
- 완전한 CCD는 구현하지 않는다.

## Phase 4: Behavior State Machine

### Goal

linear bullet 외에 homing, cold launch, timed speed/direction change를 데이터 기반 behavior로 처리한다.

### Tasks

1. `EBulletBehaviorType`과 `EBulletPhase`를 추가한다.
2. `Linear` behavior는 기존 velocity integration을 유지한다.
3. `Homing` behavior는 target actor/component 또는 target position을 향해 max turn rate와 homing strength로 velocity를 갱신한다.
4. `ColdLaunch` behavior는 delay phase 후 direction/speed를 전환한다.
5. timed event list 또는 단일 activation time으로 speed/direction 변경을 지원한다.
6. behavior update는 collision query 전에 수행할지 후에 수행할지 정책을 정한다. 기본은 behavior update 후 movement/collision이다.
7. behavior별 debug spawn preset을 추가한다.
8. debug stat에 `BehaviorTransitionCount`, behavior type별 active count를 추가한다.

### Validation

- `ReleaseBuild.bat < NUL`
- linear bullet이 기존 Phase 2 동작을 유지한다.
- homing bullet이 target을 향해 방향을 바꾼다.
- cold launch bullet이 n초 후 지정 방향/속도로 전환된다.
- behavior 전환 frame에도 render transform과 collision position이 튀지 않는다.
- behavior debug stat이 active behavior 분포와 transition count를 보여준다.

### No Goal

- scripting graph 기반 behavior authoring은 하지 않는다.
- 독립 AI/controller가 필요한 projectile은 지원하지 않는다.
- network prediction은 하지 않는다.

## Phase 5: Archetype And Multi-Renderer Grouping

### Goal

서로 다른 bullet mesh/material/behavior preset을 archetype으로 관리하고, archetype별 ISMC에 렌더링한다.

### Tasks

1. `FBulletArchetype`을 추가한다.
2. archetype은 mesh path, material path, radius, default speed, lifetime, behavior defaults를 포함한다.
3. `UBulletHellComponent`는 archetype별 renderer slot을 만든다.
4. 같은 mesh/material을 공유하는 bullet은 같은 ISMC에 들어가게 한다.
5. bullet state에는 archetype index와 render instance index를 저장한다.
6. archetype 변경이나 clear 시 해당 renderer slot을 함께 정리한다.
7. debug stat에 archetype별 active count와 renderer slot별 instance count를 추가한다.

### Validation

- `ReleaseBuild.bat < NUL`
- 두 종류 이상의 bullet mesh/material이 동시에 렌더링된다.
- 각 archetype bullet이 자기 behavior와 radius를 사용한다.
- 한 archetype의 bullet을 제거해도 다른 archetype renderer slot이 깨지지 않는다.
- archetype별 stat이 각 renderer slot의 instance count와 일치한다.

### No Goal

- renderer 수집 단계의 cross-proxy batching은 하지 않는다.
- per-instance material override는 하지 않는다.
- editor asset authoring UI 완성은 하지 않는다.

## Phase 6: Debug UX Refinement And Cleanup

### Goal

초기 Phase에서 이미 도입한 debug draw/stat을 정리하고, Phase 1 ISMC 임시 검증 코드가 production gameplay path에 섞이지 않게 마무리한다.

### Tasks

1. Phase 1~3에서 추가한 debug stat 항목을 하나의 출력/API로 정리한다.
2. Phase 1 debug draw를 정리해 bullet bounds, velocity segment, collision query 표시를 모드별로 선택할 수 있게 한다.
3. `UInstancedStaticMeshValidationComponent`는 ISMC 자체 회귀/stress 검증용으로 유지할지, BulletHell debug preset으로 대체할지 결정한다. 유지하는 경우 gameplay path와 메뉴/문서에서 분리되어야 한다.
4. public API와 unsupported behavior를 주석/문서로 정리한다.
5. performance sanity check용 대량 spawn preset을 추가한다.
6. `AInstancedStaticMeshValidationActor`는 production gameplay actor가 아니므로 BulletHell 검증이 충분해지면 editor debug spawn 경로에서 제거하거나 명확히 debug-only로 표시한다.

### Validation

- `ReleaseBuild.bat < NUL`
- debug stat이 active bullet count, render instance count, collision count를 일관된 항목명으로 보여준다.
- 대량 spawn/update/remove를 반복해도 crash나 stale render instance가 없다.
- 임시 validation-only 코드가 production path에 남지 않았는지 `rg`로 확인한다.

### No Goal

- 최종 게임 콘텐츠용 pattern editor는 만들지 않는다.
- save/load와 networking은 다루지 않는다.

