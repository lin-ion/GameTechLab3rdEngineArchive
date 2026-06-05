# Bullet Hell Component Integration Plan

## Goals

- `UBulletHellComponent`를 추가해 탄막 projectile을 actor-per-bullet이 아닌 data-oriented instance 배열로 관리한다.
- `FBulletInstance`를 tick하면서 위치, 수명, 충돌/overlap, 제거를 처리한다.
- 살아있는 bullet transform을 `UInstancedStaticMeshComponent`에 bulk 반영한다.
- `Linear`, `Homing`, `ColdLaunch` 같은 기본 behavior를 확장 가능한 형태로 둔다.
- Phase 1 ISMC 검증용 임시 컴포넌트를 대체하거나 그 코드를 정식 gameplay component로 흡수한다.
- 모든 Phase는 `ReleaseBuild.bat < NUL` 기준으로 빌드 검증 가능해야 한다.

## No Goals

- 탄 하나하나를 actor로 생성하지 않는다.
- 서로 다른 actor/component의 renderer-side cross-proxy batching은 하지 않는다.
- per-instance material override는 지원하지 않는다.
- 네트워크 동기화, save/load, replay determinism은 초기 범위에 포함하지 않는다.
- 복잡한 projectile AI나 독립 component tree가 필요한 특수 투사체는 이 component의 기본 경로에 넣지 않는다.

## Implementation Policy

- `UBulletHellComponent`는 gameplay state owner다.
- `UInstancedStaticMeshComponent`는 render instance owner다. bullet lifetime, collision, damage, behavior를 알지 않는다.
- bullet 삭제는 `RemoveInstanceSwap`과 맞는 swap-remove를 기본으로 한다.
- 외부 참조가 필요하면 array index를 직접 노출하지 않고 stable handle 또는 id/generation을 사용한다.
- collision MVP는 engine에 준비된 query 수준에 맞춘다. sweep이 없거나 비용이 크면 overlap/AABB/sphere test로 시작하고 tunneling 문제가 확인될 때 sweep을 확장한다.
- mesh/material이 여러 종류면 `UBulletHellComponent`가 archetype별 ISMC를 여러 개 소유한다.

## Phase 1: Bullet Runtime Data Model

### Goal

렌더링과 분리된 bullet runtime state를 만들고, spawn/update/remove가 actor 생성 없이 동작하게 한다.

### Tasks

1. `Source/Engine/Component/Gameplay/BulletHellComponent.h/.cpp` 또는 기존 component 분류에 맞는 위치를 정해 파일을 추가한다.
2. `FBulletInstance`를 추가한다.
3. 필수 필드는 id, generation, position, previous position, velocity, radius, age, lifetime, render instance index, alive flag로 둔다.
4. `FBulletHandle` 또는 동등한 stable id 구조를 추가한다.
5. `SpawnBullet`, `KillBullet`, `ClearBullets`, `GetBulletCount` API를 추가한다.
6. 내부 storage는 `TArray<FBulletInstance>`와 id-to-index map을 사용한다.
7. swap-remove 시 이동된 bullet의 array index와 render instance index를 함께 갱신한다.

### Validation

- `ReleaseBuild.bat < NUL`
- render component 연결 없이 bullet spawn/count/kill/clear가 정상 동작하는지 로그 또는 debug command로 확인한다.
- random kill을 반복해도 handle lookup과 count가 꼬이지 않는다.
- lifetime 만료 bullet이 tick 후 제거된다.

### No Goal

- 화면 렌더링은 하지 않는다.
- collision query는 하지 않는다.
- homing/cold launch behavior는 구현하지 않는다.

## Phase 2: ISMC Render Binding

### Goal

`UBulletHellComponent`가 `UInstancedStaticMeshComponent`와 연결되어 살아있는 bullet을 화면에 렌더링한다.

### Tasks

1. `UBulletHellComponent`가 target `UInstancedStaticMeshComponent`를 직접 소유하거나 sibling component로 참조하는 정책을 정한다.
2. spawn 시 ISMC `AddInstance`를 호출하고 `RenderInstanceIndex`를 저장한다.
3. kill/swap-remove 시 ISMC `RemoveInstanceSwap`을 호출하고 이동된 render instance index를 bullet state에 반영한다.
4. tick 후 살아있는 bullet transform을 ISMC에 bulk update한다.
5. bullet orientation은 velocity 방향 또는 fixed orientation 중 MVP 정책을 정한다.
6. debug 설정으로 spawn count, speed, lifetime, radius를 조절할 수 있게 한다.

### Validation

- `ReleaseBuild.bat < NUL`
- scene에 `UBulletHellComponent`를 배치하면 여러 projectile mesh가 렌더링된다.
- bullet이 이동하고 lifetime이 끝나면 화면에서 사라진다.
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

1. bullet radius를 사용한 simple sphere overlap 또는 ray/sphere sweep 후보를 정한다.
2. `PreviousPosition -> Position` 구간을 보존해 빠른 bullet의 tunneling risk를 줄인다.
3. world static blocker query를 위한 최소 API를 연결한다.
4. player hit 또는 skill erase volume query를 위한 component/channel/filter 정책을 정한다.
5. collision/erase 성공 시 `KillBullet`을 호출하고 render instance도 제거한다.
6. debug draw 또는 stat log로 checked/hit/killed count를 확인할 수 있게 한다.
7. sweep이 없는 MVP라면 max delta 또는 radius expansion 제한을 문서화한다.

### Validation

- `ReleaseBuild.bat < NUL`
- 벽을 향해 이동한 bullet이 벽 근처에서 제거된다.
- player 또는 skill erase volume과 겹친 bullet이 제거된다.
- 같은 frame에 여러 bullet이 제거되어도 swap-remove가 render index를 깨지 않는다.
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

### Validation

- `ReleaseBuild.bat < NUL`
- linear bullet이 기존 Phase 2 동작을 유지한다.
- homing bullet이 target을 향해 방향을 바꾼다.
- cold launch bullet이 n초 후 지정 방향/속도로 전환된다.
- behavior 전환 frame에도 render transform과 collision position이 튀지 않는다.

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

### Validation

- `ReleaseBuild.bat < NUL`
- 두 종류 이상의 bullet mesh/material이 동시에 렌더링된다.
- 각 archetype bullet이 자기 behavior와 radius를 사용한다.
- 한 archetype의 bullet을 제거해도 다른 archetype renderer slot이 깨지지 않는다.

### No Goal

- renderer 수집 단계의 cross-proxy batching은 하지 않는다.
- per-instance material override는 하지 않는다.
- editor asset authoring UI 완성은 하지 않는다.

## Phase 6: Debug UX, Stats, And Cleanup

### Goal

탄막 시스템을 반복 테스트할 수 있는 최소 debug UX와 stats를 제공하고, Phase 1 ISMC 임시 검증 코드를 제거하거나 정식 경로로 흡수한다.

### Tasks

1. active bullet count, spawned/killed/collision count, render instance count를 표시하는 stat/debug output을 추가한다.
2. debug draw로 bullet bounds 또는 collision query를 표시할 수 있게 한다.
3. 임시 ISMC validation actor/component가 남아 있다면 제거하거나 `UBulletHellComponent` debug preset으로 대체한다.
4. public API와 unsupported behavior를 주석/문서로 정리한다.
5. performance sanity check용 대량 spawn preset을 추가한다.

### Validation

- `ReleaseBuild.bat < NUL`
- debug stat이 active bullet count와 render instance count를 일치하게 보여준다.
- 대량 spawn/update/remove를 반복해도 crash나 stale render instance가 없다.
- 임시 validation-only 코드가 production path에 남지 않았는지 `rg`로 확인한다.

### No Goal

- 최종 게임 콘텐츠용 pattern editor는 만들지 않는다.
- save/load와 networking은 다루지 않는다.

