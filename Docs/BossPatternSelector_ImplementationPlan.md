# Boss Pattern Selector Implementation Plan

> This is a completed implementation plan. For practical tuning, usage, and code-maintenance guidance, use `Docs/BossPatternSelector_HandoffGuide.md`.

## Goals

- Behavior Tree 없이 보스 이동/공격을 C++ pattern component 단위로 제작한다.
- `UBossPatternSelectorComponent`가 pattern 선택, 실행, 쿨타임, 가중치 랜덤, fallback을 담당한다.
- 각 pattern은 `UBossPatternComponentBase`를 상속하고, 선딜레이/세부 step/후딜레이/종료 흐름을 작은 순차 상태머신으로 구현한다.
- pattern 수치 조정은 `UPROPERTY(Edit, Save, ...)`로 에디터에서 바로 수정 가능하게 한다.
- 신규 pattern은 기존 pattern component를 복사해 class 이름, step, 파라미터만 바꾸면 추가할 수 있게 한다.
- 초기 샘플 pattern은 별도 테스트용 예제가 아니라, 사전에 기획된 실제 boss pattern 4개로 바로 구현한다.
- 탄막은 `UBulletHellComponent` public API를 호출하고, boss pattern은 bullet runtime storage나 render instance를 직접 만지지 않는다.
- 모든 Phase는 `ReleaseBuild.bat < NUL` 기준으로 빌드 검증 가능해야 한다.

## Implementation Status

- Phase 1-6 are implemented and have passed `ReleaseBuild.bat < NUL`.
- Phase 5 debug UX was expanded beyond the original log/debug-draw scope with `stat bosspattern`.
- `stat bosspattern` lists registered patterns and uses green for ready, blue for active, and red for blocked.
- The overlay shows reject reason, cooldown remaining, effective phase weight, selection count, `Phase`, and `HealthRatio`.
- Phase 6 uses `UBulletHellHealthProbeComponent` as an optional health provider. If it is missing, the selector falls back to `BossHealthRatio=1.0f` and `BossPhase=0`.
- Implemented pattern classes are `IdleTrackTarget`, `AimedRingVolley`, `HomingOrbTrail`, `SphericalPulseBarrage`, and `ThunderclapCascade`.
- Known runtime ownership gap: `HomingOrbTrail` intentionally does not move the boss. Movement remains owned by the separate boss movement work.

## No Goals

- Behavior Tree, blackboard, visual scripting graph를 만들지 않는다.
- 이번 작업에서 Lua Blueprint 기반 pattern authoring 또는 Lua bridge를 구현하지 않는다.
- Lua Blueprint 확장을 위해 C++ 구조를 과하게 일반화하지 않는다.
- pattern을 data-only asset으로 완전히 authoring하는 시스템은 만들지 않는다.
- 보스 애니메이션 graph, VFX graph, sound cue authoring UI는 이 문서 범위에 포함하지 않는다.
- 복잡한 navmesh/pathfinding 시스템은 만들지 않는다.
- 탄 하나하나의 AI는 boss pattern에 넣지 않는다. projectile behavior는 `UBulletHellComponent` 책임으로 유지한다.

## Implementation Policy

- Boss AI의 주 실행 단위는 `UActorComponent` 계열 C++ component로 둔다.
- `UBossPatternSelectorComponent`는 owner actor에 붙은 `UBossPatternComponentBase` 파생 component들을 수집한다.
- selector는 한 번에 하나의 pattern만 active로 실행한다. 병렬 pattern은 초기 범위에서 제외한다.
- 한 pattern 내부에서 여러 wave/cycle이 겹치는 것은 허용한다. 예를 들어 lightning cycle은 이전 cycle의 낙뢰가 끝나기 전에 다음 cycle을 시작할 수 있다.
- pattern은 자기 내부 step과 tuning parameter만 소유한다. 전역 선택 규칙, 이전 pattern 기록, cooldown 시간 갱신은 selector가 소유한다.
- pattern의 `GetCanUse()`는 단순하고 읽기 쉬운 조건만 가진다. 복잡한 점수 계산은 초기 범위에서 제외한다.
- 사용 가능한 pattern이 여러 개면 weight 기반 랜덤으로 선택한다.
- 사용 가능한 pattern이 없으면 fallback pattern 또는 idle wait로 넘어가며, silent stall을 만들지 않는다.
- pattern 수치 단위는 기존 gameplay 관례와 맞춰 거리/속도/반경을 meter 기준으로 둔다.
- 디버그 기능은 Phase 1부터 넣는다. 현재 pattern 이름, step, step elapsed, pattern elapsed, cooldown, 마지막 선택 실패 이유를 즉시 확인할 수 있어야 한다.
- game jam 중 팀원이 수정할 가능성이 높은 값은 private `UPROPERTY(Edit, Save, ...)`로 노출하고, 코드 흐름은 짧은 함수 단위로 분리한다.

## Current Codebase Alignment

- gameplay component 위치는 `Source/Engine/Component/Gameplay`가 이미 있고 `UBulletHellComponent`, `UBulletHellDebugComponent`, `URagdollForceHitscanComponent` 같은 `UActorComponent` 기반 예시가 있다. Boss pattern 관련 component도 이 폴더에 둔다.
- `UBulletHellComponent`는 `SpawnBullet`, `SpawnCircleToTarget`, `SpawnSphereSurfaceToTarget`, `SpawnLineInDirection`, `FBulletSpawnParams`, `FBulletArchetype` 같은 public API를 제공한다. Boss pattern은 이 API만 사용한다.
- `UBulletHellComponent`는 bullet lifetime, collision, render binding, archetype별 renderer slot을 소유한다. Boss pattern은 bullet 배열, render index, ISMC slot을 직접 수정하지 않는다.
- `UBulletHellDebugComponent`에는 sample boss pattern spawn entry가 이미 있으므로, 초기 pattern 검증은 이 debug code와 기능이 겹치지 않게 production boss path를 별도로 만든다.
- 기존 component tuning은 `UPROPERTY(Edit, Save, Category=..., DisplayName=..., Min=..., Max=..., Speed=...)` 형태를 사용한다. Boss pattern parameter도 같은 노출 방식을 따른다.
- Lua Blueprint lifecycle과 game-specific binding 지점은 존재하지만, 이번 Boss Pattern 작업의 필수 의존성으로 두지 않는다.

## Core Class Design

### `FBossPatternContext`

Pattern 실행 중 매 frame 필요한 읽기 전용 입력 묶음이다.

Required fields:

1. `AActor* BossActor`
2. `AActor* TargetActor`
3. `UBulletHellComponent* BulletHell`
4. `float DeltaTime`
5. `float DistanceToTarget`
6. `FVector BossLocation`
7. `FVector TargetLocation`
8. `FVector DirectionToTarget`
9. `FVector BossForward`
10. `FVector BossRight`
11. `FVector BossUp`
12. `int32 BossPhase`
13. `float BossHealthRatio`

Policy:

- context는 selector가 매 tick 구성해서 active pattern에 전달한다.
- pattern은 context pointer를 장기 보관하지 않는다.
- target이 없거나 `BulletHell`이 없는 경우에도 fallback pattern이 동작할 수 있어야 한다.

### `EBossPatternStep`

모든 pattern이 공유하는 coarse step enum이다. 개별 pattern은 필요하면 내부 sub-step enum을 추가해도 된다.

```cpp
UENUM()
enum class EBossPatternStep : int32
{
    None,
    Windup,
    Task1,
    Task2,
    Task3,
    Recovery,
    Finished
};
```

Policy:

- 기본 pattern은 `Windup -> Task1 -> Recovery -> Finished`만 써도 된다.
- step이 더 필요하면 `Task2`, `Task3`까지 사용한다.
- 그 이상으로 길어지면 pattern class 내부에 의미 있는 enum을 따로 두거나 pattern을 둘로 나눈다.

### `FBossPatternDebugState`

Selector와 editor/debug log가 볼 수 있는 최소 runtime state다.

Required fields:

1. `FString ActivePatternName`
2. `FString LastSelectedPatternName`
3. `FString LastRejectedReason`
4. `EBossPatternStep ActiveStep`
5. `float ActiveStepElapsed`
6. `float ActivePatternElapsed`
7. `int32 CandidateCount`
8. `int32 UsableCandidateCount`
9. `int32 SelectionCount`
10. `int32 FallbackCount`

### `UBossPatternComponentBase`

모든 boss pattern의 base class다.

Public API:

```cpp
class UBossPatternComponentBase : public UActorComponent
{
public:
    virtual bool GetCanUse(const FBossPatternContext& Context, FString* OutRejectReason) const;
    virtual void StartPattern(const FBossPatternContext& Context);
    virtual void TickPattern(float DeltaTime, const FBossPatternContext& Context);
    virtual void CancelPattern(const FBossPatternContext& Context);
    virtual bool IsPatternFinished() const;

    float GetWeight() const;
    float GetCooldownRemaining() const;
    void NotifySelected();
    void TickCooldown(float DeltaTime);
};
```

Protected extension points:

```cpp
virtual void OnPatternStart(const FBossPatternContext& Context);
virtual void OnPatternEnd(const FBossPatternContext& Context);
virtual void OnStepEnter(EBossPatternStep Step, const FBossPatternContext& Context);
virtual void TickCurrentStep(float DeltaTime, const FBossPatternContext& Context);
virtual bool ShouldAdvanceStep(const FBossPatternContext& Context) const;
virtual EBossPatternStep GetNextStep(EBossPatternStep CurrentStep) const;
void FinishPattern(const FBossPatternContext& Context);
```

Common editable parameters:

1. `bEnabled`
2. `PatternName`
3. `Weight`
4. `Cooldown`
5. `InitialCooldown`
6. `MinTargetDistance`
7. `MaxTargetDistance`
8. `AllowedPhaseMask`
9. `WindupDuration`
10. `RecoveryDuration`
11. `bFaceTargetDuringPattern`
12. `bBlockImmediateRepeat`

Runtime fields:

1. `CurrentStep`
2. `StepElapsed`
3. `PatternElapsed`
4. `CooldownRemaining`
5. `SelectionCount`
6. `bRunning`
7. `bFinished`

Policy:

- `GetCanUse()` 기본 구현은 enabled, cooldown, distance, phase, repeat block만 검사한다.
- 파생 class는 필요한 경우 `GetCanUse()`를 override하되, 실패 이유를 짧은 문자열로 남긴다.
- `TickPattern()` 기본 구현은 timer 증가, face target 처리, `TickCurrentStep()`, step advance를 순서대로 수행한다.
- 파생 class는 대부분 `OnStepEnter()`, `TickCurrentStep()`, `GetNextStep()`만 수정하면 된다.

### `UBossPatternSelectorComponent`

Owner actor의 boss pattern 목록을 관리하고 하나를 선택해 실행한다.

Responsibilities:

1. `BeginPlay`에서 owner의 `UBossPatternComponentBase` 파생 component를 수집한다.
2. 매 tick pattern cooldown을 갱신한다.
3. active pattern이 없거나 종료되었으면 다음 pattern을 선택한다.
4. `GetCanUse() == true`인 candidate 중 weight 기반 랜덤으로 하나를 고른다.
5. 선택된 pattern의 `StartPattern()`을 호출한다.
6. active pattern의 `TickPattern()`을 호출한다.
7. fallback 또는 idle wait를 처리한다.
8. debug state와 log helper를 제공한다.

Editable parameters:

1. `bAutoStart`
2. `bEnablePatternSelection`
3. `bLogPatternSelection`
4. `bDrawPatternDebug`
5. `FallbackIdleDuration`
6. `RepeatBlockCount`
7. `TargetActor`
8. `TargetTag`
9. `bAutoResolvePlayerTarget`
10. `ForcedPatternName`
11. `ForcePatternRequest`
12. `CancelPatternRequest`

Selection policy:

- candidate list는 `bEnabled`인 pattern component만 포함한다.
- `Weight <= 0.0f`인 pattern은 선택 대상에서 제외한다.
- 모든 candidate가 실패하면 fallback count를 증가시키고 idle wait에 들어간다.
- forced pattern request는 cooldown과 distance 조건을 무시할지 여부를 별도 flag로 둔다. 기본은 무시하지 않는다.
- immediate repeat 방지는 selector의 recent pattern ring buffer와 pattern의 `bBlockImmediateRepeat`를 함께 사용한다.

## Planned Boss Pattern Classes

아래 4개는 샘플이 아니라 실제 초기 boss pattern이다. 이름은 코드 가독성을 기준으로 정한다.

### `UBossPattern_AimedRingVolley`

Mechanic:

- Boss를 중심으로 target 방향을 법선으로 하는 반지름 `RingRadius`의 원을 만든다.
- 원 위에 `ProjectileCount`개의 projectile을 균등 배치한다.
- projectile은 생성 직후 정지 상태로 대기한다.
- 각 projectile은 `LaunchDelay`초 후 target 방향으로 발사된다.
- 유도 성능은 사용하지 않는다.

Suggested step flow:

1. `Windup`: target 방향과 spawn circle basis를 확정한다.
2. `Task1`: 정지 projectile을 원 위에 생성한다.
3. `Task2`: `LaunchDelay` 동안 대기 projectile을 유지한다.
4. `Recovery`: pattern 종료 대기.

Key parameters:

1. `RingRadius`
2. `ProjectileCount`
3. `LaunchDelay`
4. `ProjectileSpeed`
5. `ProjectileLifetime`
6. `ProjectileRadius`
7. `SpawnForwardOffset`
8. `SpawnUpOffset`
9. `AngleOffsetDegrees`
10. `bLockTargetDirectionOnStart`

Implementation notes:

- `UBulletHellComponent`에 delayed launch를 직접 지원하는 API가 없다면, Boss pattern에서 pending launch 목록을 짧게 소유하고 `LaunchDelay` 후 velocity/runtime modifier를 적용하는 helper를 추가한다.
- delayed projectile은 launch 전 collision/damage를 끄거나, lifetime을 launch 시점부터 계산할지 정책을 명확히 한다.
- target이 사라지면 start 시점에 잠근 방향을 사용한다.

### `UBossPattern_HomingOrbTrail`

Mechanic:

- Boss가 앞쪽으로 이동하는 동안 `SpawnDuration` 동안 일정 간격으로 projectile을 1개씩 자기 주변에 생성한다.
- 총 생성 수는 `ProjectileCount`를 기준으로 하고, interval은 `SpawnDuration / ProjectileCount` 또는 명시 `SpawnInterval` 중 하나로 계산한다.
- projectile은 생성 직후 정지 상태로 대기한다.
- 각 projectile은 자기 생성 시점으로부터 `LaunchDelay`초 후 플레이어 방향으로 발사된다.
- 발사 후에는 homing 성능을 사용한다.
- 현재 boss 이동은 다른 팀원이 구현 중이므로 이 pattern에서는 이동을 구현하지 않고 TODO 주석만 남긴다.

Required code comment:

```cpp
// TODO(BossMovement): Move the boss forward during HomingOrbTrail.
// Movement ownership is handled by another teammate; this pattern only spawns and launches projectiles for now.
```

Suggested step flow:

1. `Windup`: spawn timing과 target reference를 준비한다.
2. `Task1`: `SpawnDuration` 동안 interval마다 정지 homing projectile을 생성한다.
3. `Task2`: 남아 있는 delayed projectile launch를 처리한다.
4. `Recovery`: pattern 종료 대기.

Key parameters:

1. `SpawnDuration`
2. `ProjectileCount`
3. `SpawnIntervalOverride`
4. `SpawnRadiusAroundBoss`
5. `LaunchDelay`
6. `ProjectileSpeed`
7. `ProjectileLifetime`
8. `ProjectileRadius`
9. `HomingStrength`
10. `HomingMaxTurnRateDegrees`
11. `HomingConeHalfAngleDegrees`
12. `SpawnForwardOffset`
13. `SpawnUpOffset`

Implementation notes:

- 생성 위치는 boss 주변 random/ring/alternating 방식 중 하나를 parameter로 둘 수 있다. MVP는 ring 또는 alternating angle로 충분하다.
- 각 projectile마다 launch deadline이 다르므로 `FPendingBossProjectileLaunch` 같은 작은 runtime struct가 필요하다.
- homing target actor가 사라지면 마지막 target 위치로 non-homing launch하거나 해당 projectile을 제거하는 정책을 parameter로 둔다.

### `UBossPattern_SphericalPulseBarrage`

Mechanic:

- Boss를 중심으로 `PulseDuration` 동안 일정 간격으로 `PulseCount`회 발동한다.
- 각 pulse마다 `ProjectilesPerPulse`개의 projectile을 boss 중심 반지름 `SphereRadius`의 구면에 생성한다.
- projectile은 생성 즉시 움직인다.
- 발사 방향은 구의 중심에서 spawn point로 향하는 outward direction이다.

Suggested step flow:

1. `Windup`: pulse timer를 초기화한다.
2. `Task1`: interval마다 sphere surface projectile batch를 생성한다.
3. `Recovery`: 마지막 pulse 이후 종료 대기.

Key parameters:

1. `PulseDuration`
2. `PulseCount`
3. `PulseIntervalOverride`
4. `ProjectilesPerPulse`
5. `SphereRadius`
6. `ProjectileSpeed`
7. `ProjectileLifetime`
8. `ProjectileRadius`
9. `bUseRandomSpherePoints`
10. `RandomSeedOffset`

Implementation notes:

- MVP는 deterministic latitude/longitude 또는 fibonacci sphere sampling 중 하나를 사용한다.
- 랜덤 sphere sampling을 쓰더라도 pattern 재현을 위해 optional seed를 둔다.
- `UBulletHellComponent::SpawnSphereSurfaceInDirection`이 한 방향 batch만 만든다면, 각 point마다 `SpawnBullet`을 호출하거나 새 batch helper를 추가한다.

### `UBossPattern_ThunderclapCascade`

Mechanic:

- Boss의 forward 방향 `StrikeForwardDistance` 앞에 1차 projectile을 생성한다.
- 1차 projectile은 낙뢰 이미지로 위에서 지면 방향으로 떨어진다.
- 1차 projectile이 지면에 도달하거나 lifetime이 끝나면 소멸한다.
- 1차 projectile 소멸 시점에 2차 projectile wave를 생성한다.
- 2차 projectile은 지면 위 `ShockwaveHeightOffset` 정도의 여유 높이를 가지고 원형으로 퍼져나간다.
- 플레이어가 점프해서 피하는 것을 유도하는 낮은 shockwave projectile이다.
- 이 1차 낙뢰 + 2차 shockwave cycle을 `CycleCount`회 반복한다.
- cycle interval이 짧으면 이전 cycle이 끝나기 전에 다음 cycle이 시작될 수 있다. 연속으로 낙뢰가 빠르게 꽂히는 이미지가 목표다.

Suggested step flow:

1. `Windup`: 첫 cycle 시작 시간을 준비한다.
2. `Task1`: `CycleCount`개의 strike cycle을 interval에 맞춰 시작한다.
3. `Task2`: active cycle들의 falling projectile과 shockwave spawn을 갱신한다.
4. `Recovery`: 모든 cycle 종료 또는 timeout 이후 종료한다.

Key parameters:

1. `CycleCount`
2. `CycleInterval`
3. `StrikeForwardDistance`
4. `StrikeSpawnHeight`
5. `StrikeFallSpeed`
6. `StrikeProjectileRadius`
7. `StrikeLifetime`
8. `GroundHeightOffset`
9. `ShockwaveHeightOffset`
10. `ShockwaveProjectileCount`
11. `ShockwaveRadius`
12. `ShockwaveSpeed`
13. `ShockwaveLifetime`
14. `ShockwaveProjectileRadius`
15. `MaxPatternDuration`

Runtime structs:

```cpp
struct FThunderclapCycleState
{
    FVector ImpactLocation = FVector::ZeroVector;
    FBulletHandle StrikeHandle;
    float Age = 0.0f;
    bool bStrikeSpawned = false;
    bool bShockwaveSpawned = false;
    bool bFinished = false;
};
```

Implementation notes:

- ground detection이 아직 안정적이지 않으면, MVP는 `ImpactLocation.Z + GroundHeightOffset`을 기준으로 strike 소멸 시점을 계산한다.
- 실제 지면 query가 가능하면 `UWorld` collision query를 사용해 impact Z를 보정한다.
- shockwave는 `SpawnCircleInDirection`보다 원형 outward velocity가 필요하므로, 각 projectile을 원 둘레 또는 중심에서 개별 `SpawnBullet`하는 helper를 둘 수 있다.
- cycle overlap을 위해 pattern 전체 step은 하나지만 내부 `TArray<FThunderclapCycleState>`로 여러 cycle을 동시에 갱신한다.

## Pattern Authoring Rules

- 새 pattern을 만들 때는 기존 pattern 하나를 복사하고 class name, file name, `PatternName` default만 먼저 바꾼다.
- 새 pattern 파일은 `KraftonEngine.vcxproj`와 `KraftonEngine.vcxproj.filters`에 모두 등록한다.
- 새 pattern class는 owner actor에 component로 붙이면 selector가 `BeginPlay`에서 자동 수집한다.
- `PatternName`은 `ForcedPatternName`과 `stat bosspattern`에서 쓰이므로 class 이름과 비슷하게 유지한다.
- tuning parameter는 header의 private `UPROPERTY(Edit, Save, Category="Boss Pattern|...")` 아래에 모은다.
- `TickCurrentStep()` 안에 긴 if chain을 만들지 않는다. step별 private helper로 분리한다.
- `GetCanUse()`에서 gameplay side effect를 만들지 않는다.
- `OnStepEnter()`에서 한 번만 실행되어야 하는 spawn, sound, animation trigger를 처리한다.
- 매 frame 필요한 이동/회전만 `TickCurrentStep()`에서 처리한다.
- pattern 종료는 `FinishPattern()` 같은 base helper를 통해 수행해 selector state와 cooldown이 한 곳에서 갱신되게 한다.
- debug log 또는 `stat bosspattern`에서 pattern name, step/detail, reject reason, cooldown, phase 상태를 확인할 수 있어야 한다.

## Tuning Quick Reference

공통 selector 값:

- `FallbackIdleDuration`: 사용 가능한 pattern이 없을 때 대기 시간. 0.1-1.0초부터 시작한다.
- `RepeatBlockCount`: 바로 반복을 막고 싶으면 1, 더 강하게 섞고 싶으면 2-3을 쓴다.
- `Phase1HealthRatioThreshold`: 기본 0.66. 이 값 이하부터 phase 1이다.
- `Phase2HealthRatioThreshold`: 기본 0.33. 이 값 이하부터 phase 2다.
- `DebugBossHealthRatio` / `DebugBossPhase`: phase 튜닝 중에만 켠다.

공통 pattern 값:

- `Weight`: 기본 선택 빈도. 0이면 선택되지 않는다.
- `PhaseWeight0/1/2`: phase별 weight multiplier. 특정 phase에서 막으려면 0으로 둔다.
- `Cooldown`: 같은 pattern이 다시 나올 수 있는 최소 간격. 1-8초 범위부터 조정한다.
- `AllowedPhaseMask`: phase 자체를 금지하고 싶을 때 사용한다. phase weight 0은 선택 확률 조정용, mask는 hard block용이다.
- `MinTargetDistance` / `MaxTargetDistance`: 거리 조건. 모든 거리에서 허용하려면 기본값을 유지한다.
- `WindupDuration` / `RecoveryDuration`: 시작/종료 템포. 0.1-1.0초부터 조정한다.

Pattern별 주요 값:

- `AimedRingVolley`: `RingRadius`, `ProjectileCount`, `LaunchDelay`, `ProjectileSpeed`를 먼저 조정한다.
- `HomingOrbTrail`: `SpawnDuration`, `ProjectileCount`, `LaunchDelay`, `HomingStrength`를 먼저 조정한다.
- `SphericalPulseBarrage`: `PulseCount`, `ProjectilesPerPulse`, `SphereRadius`, `ProjectileSpeed`를 먼저 조정한다.
- `ThunderclapCascade`: `CycleCount`, `CycleInterval`, `StrikeForwardDistance`, `StrikeRandomXYRadius`, `ShockwaveProjectileCount`, `ShockwaveSpeed`를 먼저 조정한다.

## Unsupported Scope

- Behavior Tree, blackboard, visual scripting graph
- Lua Blueprint pattern authoring or Lua bridge
- Data-only pattern asset authoring
- Parallel top-level patterns in the selector
- Full boss movement, navmesh, or pathfinding
- Ground-query-perfect lightning impact placement
- Boss animation graph, VFX graph, and sound cue authoring UI
- Per-projectile AI inside boss patterns

## Fallback Behavior

- Selector가 붙었지만 pattern component가 없으면 `LastRejectedReason`은 `no pattern components`가 되고 fallback idle wait만 반복한다.
- Pattern component가 있어도 전부 disabled, cooldown, phase, weight, distance, repeat block에 막히면 fallback idle wait로 들어간다.
- Fallback은 별도 공격을 만들지 않고 selector가 멈춘 것처럼 보이지 않게 하는 안전 대기 상태다.
- 이 상태는 `stat bosspattern`의 `Select 0/N`, 빨간 blocked line, `LastReject`, `Fallback` count로 확인한다.

## Phase 1: Selector And Base Pattern Skeleton

### Goal

Boss pattern system의 최소 실행 루프를 만든다. 이 Phase의 완료 기준은 공격 없이도 selector가 pattern component를 수집하고, fallback idle pattern을 선택/실행/종료/쿨타임 갱신할 수 있는 것이다.

### Tasks

1. `Source/Engine/Component/Gameplay/BossPatternComponentBase.h/.cpp`를 추가한다.
2. `Source/Engine/Component/Gameplay/BossPatternSelectorComponent.h/.cpp`를 추가한다.
3. `Source/Engine/Component/Gameplay/BossPattern_IdleTrackTarget.h/.cpp`를 추가한다.
4. project/filter/header generation 경로에 새 파일을 등록한다.
5. `FBossPatternContext`, `FBossPatternDebugState`, `EBossPatternStep`를 추가한다.
6. base pattern의 common parameter와 runtime state를 추가한다.
7. selector가 owner component list에서 boss pattern component들을 수집하게 한다.
8. selector tick에서 cooldown 갱신, active pattern tick, 종료 감지, 다음 pattern 선택을 수행한다.
9. usable candidate가 없을 때 fallback idle wait를 수행한다.
10. `bLogPatternSelection`이 켜져 있으면 선택/실패/종료 로그를 출력한다.
11. `GetBossPatternDebugState()` 또는 동등한 debug getter를 제공한다.

### Validation

- `ReleaseBuild.bat < NUL`
- boss actor에 selector만 있어도 crash 없이 tick된다.
- boss actor에 pattern이 없으면 fallback idle wait로 들어가고 log가 과도하게 반복되지 않는다.
- idle pattern 1개를 붙이면 selector가 해당 pattern을 선택하고 종료 후 cooldown을 갱신한다.
- `Weight <= 0`, `bEnabled=false`, cooldown 중인 pattern이 candidate에서 제외된다.
- debug state에서 active pattern name, step, elapsed, candidate count를 확인할 수 있다.

### No Goal

- 탄막 spawn은 하지 않는다.
- 실제 공격 pattern은 만들지 않는다.
- editor 전용 UI를 만들지 않는다.

## Phase 2: Aimed Ring Volley

### Goal

`UBossPattern_AimedRingVolley`를 추가해 delayed launch가 필요한 첫 실제 boss pattern을 구현한다.

### Tasks

1. `Source/Engine/Component/Gameplay/BossPattern_AimedRingVolley.h/.cpp`를 추가한다.
2. `UBulletHellComponent` sibling component를 context에서 찾아 pattern에 전달한다.
3. target 방향을 법선으로 하는 spawn circle basis를 계산한다.
4. circle 위에 `ProjectileCount`개의 정지 projectile을 생성한다.
5. `LaunchDelay` 후 모든 projectile을 target 방향으로 발사한다.
6. 발사 후 homing은 끈다.
7. delayed launch helper 또는 `FPendingBossProjectileLaunch` runtime struct를 추가한다.
8. `UBulletHellComponent`가 없으면 `GetCanUse()`가 false를 반환하고 reject reason을 남긴다.

### Validation

- `ReleaseBuild.bat < NUL`
- boss 중심 target-facing circle 위에 projectile이 생성된다.
- 생성 직후 projectile은 `LaunchDelay` 동안 움직이지 않는다.
- delay 이후 projectile이 target 방향으로 발사된다.
- homing 값이 꺼져 있어 발사 후 방향을 바꾸지 않는다.
- target이 사라져도 start 시점 방향 또는 fallback 방향으로 crash 없이 발사된다.

### No Goal

- homing launch는 하지 않는다.
- boss movement는 하지 않는다.
- Lua Blueprint 호출은 하지 않는다.

## Phase 3: Homing Orb Trail And Spherical Pulse Barrage

### Goal

두 번째, 세 번째 실제 boss pattern을 구현한다. 하나는 delayed homing launch, 하나는 즉시 outward sphere pulse다.

### Tasks

1. `Source/Engine/Component/Gameplay/BossPattern_HomingOrbTrail.h/.cpp`를 추가한다.
2. `Source/Engine/Component/Gameplay/BossPattern_SphericalPulseBarrage.h/.cpp`를 추가한다.
3. homing orb trail은 `SpawnDuration` 동안 interval마다 boss 주변에 정지 projectile을 1개씩 생성한다.
4. homing orb trail의 각 projectile은 자기 생성 시점으로부터 `LaunchDelay` 후 target 방향으로 발사되고 homing을 켠다.
5. homing orb trail에는 boss forward movement TODO 주석을 남기고 실제 이동 구현은 하지 않는다.
6. spherical pulse barrage는 `PulseDuration` 동안 `PulseCount`회 발동한다.
7. 각 pulse는 boss 중심 `SphereRadius` 구면에 `ProjectilesPerPulse`개의 projectile을 생성한다.
8. spherical pulse projectile은 생성 즉시 outward direction으로 이동한다.
9. 두 pattern 모두 selector cooldown, distance, phase 조건을 사용한다.

### Validation

- `ReleaseBuild.bat < NUL`
- homing orb trail이 일정 간격으로 projectile을 1개씩 생성한다.
- 각 homing projectile은 자기 생성 이후 `LaunchDelay`를 기준으로 따로 발사된다.
- homing projectile이 발사 후 target을 향해 방향을 보정한다.
- homing orb trail 코드에 boss movement TODO 주석이 남아 있다.
- spherical pulse barrage가 지정한 pulse count와 interval로 여러 번 발동한다.
- sphere surface projectile이 boss 반대 방향으로 즉시 퍼진다.

### No Goal

- boss forward movement를 구현하지 않는다.
- navmesh/pathfinding을 구현하지 않는다.
- projectile별 actor를 만들지 않는다.

## Phase 4: Thunderclap Cascade

### Goal

네 번째 실제 boss pattern인 낙뢰 + 충격파 연속 cycle을 구현한다.

### Tasks

1. `Source/Engine/Component/Gameplay/BossPattern_ThunderclapCascade.h/.cpp`를 추가한다.
2. boss forward `StrikeForwardDistance` 위치에 strike impact location을 계산한다.
3. strike projectile을 `StrikeSpawnHeight`에서 아래 방향으로 생성한다.
4. strike가 지면 또는 impact height에 도달하면 제거한다.
5. strike 제거 시점에 shockwave projectile을 생성한다.
6. shockwave projectile은 `ShockwaveHeightOffset` 높이에서 원형 outward direction으로 퍼진다.
7. `CycleCount`와 `CycleInterval`로 strike cycle을 여러 번 시작한다.
8. cycle overlap을 허용하기 위해 `TArray<FThunderclapCycleState>`로 active cycle들을 갱신한다.
9. `MaxPatternDuration`을 둬서 cycle state가 꼬여도 pattern이 무한히 지속되지 않게 한다.

### Validation

- `ReleaseBuild.bat < NUL`
- boss 앞쪽 지정 거리에서 strike projectile이 생성된다.
- strike projectile이 아래로 떨어지고 impact 시점에 사라진다.
- strike가 사라진 위치에서 낮은 높이의 shockwave projectile이 원형으로 퍼진다.
- `CycleInterval`을 짧게 두면 여러 strike cycle이 겹쳐 실행된다.
- `CycleCount`가 끝나고 active cycle이 모두 종료되면 pattern이 recovery로 넘어간다.
- `MaxPatternDuration`에 도달하면 pattern이 강제로 종료된다.

### No Goal

- 복잡한 lightning VFX를 만들지 않는다.
- 완전한 ground query/pathing 시스템을 만들지 않는다.
- player jump 판정 시스템을 새로 만들지 않는다.

## Phase 5: Debug UX And Tuning Loop

### Goal

게임잼 중 빠른 반복을 위해 강제 실행, 로그, runtime 상태 확인, 안전 fallback을 정리한다.

### Tasks

1. selector에 `ForcedPatternName`, `ForcePatternRequest`, `CancelPatternRequest`를 추가한다.
2. forced pattern 실행 시 cooldown/distance/phase 조건을 존중할지 정하는 flag를 추가한다.
3. active pattern cancel 후 recovery 없이 종료할지, fallback으로 넘어갈지 정책을 구현한다.
4. `bDrawPatternDebug`가 켜지면 boss 위치 근처에 current pattern/step/cooldown summary를 debug draw 또는 log로 표시한다.
5. pattern reject reason을 최근 N개까지 저장하거나 마지막 reason만 노출한다.
6. selection count, fallback count, pattern별 selected count를 debug getter에 포함한다.
7. delayed projectile count, active thunderclap cycle count 같은 pattern별 debug count를 log helper로 확인할 수 있게 한다.

### Validation

- `ReleaseBuild.bat < NUL`
- editor/PIE에서 request 값을 변경해 특정 pattern을 강제 실행할 수 있다.
- forced pattern 이름이 잘못됐을 때 crash 없이 실패 reason이 남는다.
- active pattern cancel 후 selector가 다음 tick에 정상 상태로 돌아온다.
- debug 표시가 현재 pattern과 step을 실제 runtime state와 일치하게 보여준다.
- delayed launch 또는 thunderclap active cycle이 남아 있을 때 debug count로 확인할 수 있다.

### No Goal

- 전용 visual editor를 만들지 않는다.
- Lua Blueprint bridge를 만들지 않는다.
- pattern parameter preset asset을 만들지 않는다.

## Phase 6: Boss Phase And Selection Refinement

### Goal

보스 체력/페이즈에 따라 pattern pool과 weight가 바뀌는 최소 정책을 추가한다.

### Tasks

1. selector가 boss health provider를 직접 강제하지 않고, 우선 `BossHealthRatio`를 optional로 계산한다.
2. health component가 없으면 `BossHealthRatio=1.0f`, `BossPhase=0`으로 동작한다.
3. pattern의 `AllowedPhaseMask`를 적용한다.
4. phase별 weight multiplier가 필요하면 `PhaseWeight0`, `PhaseWeight1`, `PhaseWeight2`처럼 단순 field로 시작한다.
5. 최근 사용 pattern ring buffer로 같은 pattern 반복을 줄인다.
6. hp threshold 기반 phase 계산은 selector parameter로 노출한다.

### Validation

- `ReleaseBuild.bat < NUL`
- phase 0에서 허용되지 않는 pattern은 선택되지 않는다.
- health ratio 또는 debug phase override를 바꾸면 pattern pool이 바뀐다.
- repeat block count를 1 이상으로 두면 같은 pattern이 바로 반복되지 않는다.
- 모든 pattern이 repeat/cooldown으로 막힌 경우 fallback이 동작한다.

### No Goal

- full boss health/damage system을 새로 만들지 않는다.
- designer-facing phase editor를 만들지 않는다.

## Phase 7: Cleanup And Handoff Notes

### Goal

팀원이 게임잼 중 pattern을 복사/수정할 때 필요한 최소 문서와 코드 주석을 남긴다.

### Tasks

1. pattern base header에 새 pattern 추가 절차를 짧게 주석으로 남긴다.
2. `Docs/BossPatternSelector_ImplementationPlan.md`의 완료/미완료 항목을 실제 구현 결과에 맞게 갱신한다.
3. pattern별 주요 tuning parameter와 안전 범위를 정리한다.
4. unsupported scope를 정리한다: Lua BP authoring, BT, navmesh, parallel pattern, data asset authoring.
5. fallback pattern이 없는 boss actor를 배치했을 때의 동작을 명확히 한다.
6. final build 후 팀원 확인 체크리스트를 남긴다.

### Validation

- `ReleaseBuild.bat < NUL`
- 새 pattern 추가 절차는 이번 구현에서 5개 pattern 파일 세트를 추가하며 검증했다. 이후 새 pattern을 추가할 때도 project/filter 등록 후 `ReleaseBuild.bat < NUL`을 기준으로 확인한다.
- 기존 pattern parameter만 바꿔도 발사 타이밍, projectile 수, 속도, 반경이 바뀌는지 확인한다.
- selector/pattern debug log만 보고 현재 상태를 추적할 수 있다.

## Final Verification Checklist

- `ReleaseBuild.bat < NUL`이 통과한다.
- PIE/Game에서 `stat bosspattern`을 켜면 등록된 pattern 목록이 보인다.
- Active pattern은 파란색, ready pattern은 초록색, blocked pattern은 빨간색으로 보인다.
- Cooldown 중인 pattern은 남은 시간이 줄어드는 것을 확인할 수 있다.
- `HealthRatio`와 `Phase` 표시가 debug override 또는 health probe 값과 맞는다.
- `AllowedPhaseMask`, `PhaseWeight0/1/2`, `RepeatBlockCount`, `Cooldown`, `Weight`가 선택 pool에 반영된다.
- `ForcedPatternName`과 `ForcePatternRequest`로 특정 pattern을 강제 실행할 수 있다.
- `CancelPatternRequest` 후 selector가 fallback 또는 다음 선택 루프로 정상 복귀한다.
- 각 pattern의 주요 수치를 바꾸면 발사 수, 속도, 반경, 간격, 낙뢰 위치 랜덤성이 즉시 달라진다.

## Suggested File List

- `Source/Engine/Component/Gameplay/BossPatternComponentBase.h`
- `Source/Engine/Component/Gameplay/BossPatternComponentBase.cpp`
- `Source/Engine/Component/Gameplay/BossPatternSelectorComponent.h`
- `Source/Engine/Component/Gameplay/BossPatternSelectorComponent.cpp`
- `Source/Engine/Component/Gameplay/BossPattern_IdleTrackTarget.h`
- `Source/Engine/Component/Gameplay/BossPattern_IdleTrackTarget.cpp`
- `Source/Engine/Component/Gameplay/BossPattern_AimedRingVolley.h`
- `Source/Engine/Component/Gameplay/BossPattern_AimedRingVolley.cpp`
- `Source/Engine/Component/Gameplay/BossPattern_HomingOrbTrail.h`
- `Source/Engine/Component/Gameplay/BossPattern_HomingOrbTrail.cpp`
- `Source/Engine/Component/Gameplay/BossPattern_SphericalPulseBarrage.h`
- `Source/Engine/Component/Gameplay/BossPattern_SphericalPulseBarrage.cpp`
- `Source/Engine/Component/Gameplay/BossPattern_ThunderclapCascade.h`
- `Source/Engine/Component/Gameplay/BossPattern_ThunderclapCascade.cpp`
