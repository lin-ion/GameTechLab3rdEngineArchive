#pragma once

#include "GameFramework/AActor.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Engine/UI/Canvas/UICanvasActor.generated.h"

class UUICanvas;
class APawn;

// [사이클 4] 바인딩이 대상 요소에 적용하는 시각 채널.
UENUM()
enum class EHudBindingChannel : uint8
{
	BarWidth,     // 값의 비율을 가로 폭에 반영(좌피벗 권장 — 폭 감소가 좌→우)
	BarColor,     // 값의 비율을 색으로(가득=녹 → 절반=노 → 고갈=적)
	Text,         // 값을 문자열로 표시(대상이 UUITextElement 계열일 때)
	WorldAnchor,  // [사이클 6] 소스 액터 월드 위치(+높이)를 화면에 투영해 요소 Position 추적(머리 위 HP). 옵션.
};

// [사이클 4] 바인딩이 읽어올 게임플레이 값. 바 채널은 체력 비율을, 텍스트 채널은 아래 포맷을 쓴다.
UENUM()
enum class EHudBindingValue : uint8
{
	HealthCurrent,   // 현재 체력(정수) / 바: 체력 비율
	HealthOverMax,   // "현재 / 최대"    / 바: 체력 비율
	HealthPercent,   // "NN%"           / 바: 체력 비율
	GameTime,        // 경과 시간(초)    / 바: 의미 없음(full)
	// [사이클 5] 보스(UBossPatternSelectorComponent) 값 — 소스 액터가 보스일 때:
	BossHealthRatio, // 바: 보스 HP 비율 / 텍스트: "NN%"  (컴포넌트, 없으면 폰 체력 폴백)
	BossPhase,       // 바색: 페이즈→녹/노/적 / 텍스트: "Phase N"
	BossPatternName, // 텍스트: 현재 공격 패턴 이름
};

// [사이클 4] HUD 데이터 바인딩 1건. 에디터 Details 의 Bindings 배열에서 행 단위로 +/- 편집한다.
USTRUCT()
struct FHudBinding
{
	GENERATED_BODY()

	// 대상 UI 요소(UUIElement::ElementName). UIElementPicker: 이 액터의 UUIElement 후보 콤보.
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Target Element", UIElementPicker=true)
	FString TargetElement;

	// 값을 읽을 월드 액터(GetFName). 비면 로컬 플레이어(possessed pawn). WorldActorPicker: 월드 APawn 콤보.
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Source Actor", WorldActorPicker=true)
	FString SourceActor;

	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Channel", Enum=EHudBindingChannel)
	EHudBindingChannel Channel = EHudBindingChannel::BarWidth;

	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Value", Enum=EHudBindingValue)
	EHudBindingValue Value = EHudBindingValue::HealthCurrent;

	// [사이클 6] WorldAnchor 채널 전용: 소스 액터 위치 +Z 오프셋(머리 위, 월드 단위). 다른 채널은 무시.
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="World Anchor Height")
	float WorldAnchorHeight = 100.0f;

	// ── 런타임 캐시(UPROPERTY 아님 → 반사/직렬화 제외) ──
	TWeakObjectPtr<APawn> SourceCache;            // 소스 액터 해석 결과
	float                 BarFullWidth = -1.0f;   // BarWidth 최초 캡처한 100% 기준 폭
};

// 신규 계층형 UI Canvas 를 소유하는 액터(진단 결정: Actor + Component → .Scene 직렬화).
// RootComponent 가 UUICanvas 이며, BeginPlay 에서 FUICanvasManager 에 등록한다.
// 액터가 월드에 살아있는 동안 AActor::AddReferencedObjects 가 RootComponent 를 통해
// 트리를 keepalive 하고, 매니저 등록으로 레이아웃/드로우 패스에 노출된다.
UCLASS()
class AUICanvasActor : public AActor
{
public:
	GENERATED_BODY()
	// 데이터 바인딩(체력바 등)을 매 프레임 갱신하려면 액터 틱이 필요하다. 전역 FTickManager 는
	// bNeedsTick && HasActorBegunPlay() 를 통과한 액터만 틱하므로(편집 월드는 BeginPlay 미진입 →
	// 틱 안 함), 런타임에만 동작하고 편집 모드엔 영향 없다(데이터 바인딩 사이클 2).
	AUICanvasActor() { bNeedsTick = true; }

	void BeginPlay() override;
	void EndPlay() override;
	void Tick(float DeltaTime) override;   // 데이터 바인딩 갱신(체력바 width 등)

	UUICanvas* GetCanvas() const { return Canvas.Get(); }

	// 루트 Canvas 구성(컴포넌트 생성 + RootComponent 설정). BeginPlay 또는 스폰 직후 호출.
	void InitCanvas();

	// UI .uasset(EAssetPackageType::UI) 경로에서 캔버스 트리를 복원해 RootComponent 로 세팅한다.
	// 트리 빌드 전용 — 화면 렌더 등록(FUICanvasManager::RegisterCanvas)은 여기서 하지 않고
	// BeginPlay 로 미룬다(진단 R1). UIAssetPath 도 함께 기록한다(에셋 참조 모델, R4).
	void LoadFromAsset(const FString& InAssetPath);

	// 화면에 띄울 UI .uasset 경로를 지정한다(빌드는 BeginPlay 로 지연 — R1/R4). 드롭/스폰 배선이 호출.
	void SetUIAssetPath(const FString& InAssetPath) { UIAssetPath = InAssetPath; }
	const FString& GetUIAssetPath() const { return UIAssetPath.ToString(); }

	// [R4] 에셋 참조(UIAssetPath)가 있으면 캔버스 트리를 .Scene 에 인라인 직렬화하지 않는다
	// (트리는 로드 후 BeginPlay 가 .uasset 으로 재구성). AActor 훅 오버라이드.
	bool ShouldSerializeRootComponentTree() const override;

private:
	// [데이터 바인딩] 매 프레임 Tick 에서 소스 폰을 해석하고, 체력바(width+옵션 색, 사이클 1·2)와
	// 숫자 텍스트 readout(사이클 3)을 갱신한다. 좌측 피벗 바는 폭 감소가 좌→우.
	void UpdateDataBindings();

	TWeakObjectPtr<UUICanvas> Canvas = nullptr;

	// 화면에 띄울 UI .uasset 참조(경로 기반). UPROPERTY(Save) 라 .Scene 에 자동 직렬화/복원되고,
	// 로드 후 BeginPlay 가 이 경로로 캔버스를 재구성한다(메시 StaticMeshPath 선례 동형 — 진단 R4/D).
	UPROPERTY(Edit, Save, Category="UI", DisplayName="UI Asset", AssetType="UIAsset")
	FSoftObjectPtr UIAssetPath;

	// [사이클 4] 다중 데이터 바인딩 디스크립터 배열. 각 항목 = {대상 요소, 소스 액터, 채널, 값}.
	// HUD 의 체력바·색·텍스트를 행 단위로 자유 구성(에디터 +/- 로 추가/삭제). 런타임 캐시는 FHudBinding 내부.
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Bindings", Type=Array, Struct=FHudBinding)
	TArray<FHudBinding> Bindings;
};
