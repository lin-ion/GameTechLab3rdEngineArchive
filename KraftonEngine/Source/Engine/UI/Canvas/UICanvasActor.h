#pragma once

#include "GameFramework/AActor.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Engine/UI/Canvas/UICanvasActor.generated.h"

class UUICanvas;
class APawn;

// [사이클 3] 숫자 텍스트 readout 이 표시할 값.
UENUM()
enum class EHudTextSource : uint8
{
	Health,         // 현재 체력(정수)
	HealthOverMax,  // "현재 / 최대"
	HealthPercent,  // "NN%"
	GameTime,       // 경과 시간(초)
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

	// 대상 UI 요소 식별자(UUIElement::ElementName, UI 에디터에서 부여). 캔버스 루트에서 FindByName.
	// UIElementPicker: Details 에서 이 액터의 UUIElement 후보 콤보로 선택(자유 텍스트 대신).
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Health Bar Element", UIElementPicker=true)
	FString HealthBarElementName;

	// 체력을 읽을 월드 액터 이름(GetFName().ToString()). 비면 로컬 플레이어(possessed pawn)를 기본 타깃으로,
	// 지정 시 그 이름의 액터를 타깃으로 한다(override). 해석된 APawn 은 아래에 캐시.
	// WorldActorPicker: Details 에서 월드의 APawn 후보 콤보로 선택(자유 텍스트 대신).
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Health Source Actor", WorldActorPicker=true)
	FString HealthSourceActorName;

	// [사이클 2] 체력 비율을 색으로도 피드백할지(가득=녹 → 절반=노 → 고갈=적). 기본 off.
	// 켜면 매 프레임 대상 바의 BackgroundColor 를 덮어쓴다(저작 색 무시) — 단색 외형 유지하려면 off.
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Health Bar Color Feedback")
	bool bHealthBarColorFeedback = false;

	// [사이클 3] 숫자 텍스트 readout 대상 UUITextElement 식별자(ElementName). 비면 비활성.
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Text Element", UIElementPicker=true)
	FString TextElementName;

	// [사이클 3] 위 텍스트 요소에 표시할 값. 소스 폰은 Health Source(비면 로컬 플레이어)를 공유.
	UPROPERTY(Edit, Save, Category="UI|Binding", DisplayName="Text Source", Enum=EHudTextSource)
	EHudTextSource TextSource = EHudTextSource::Health;

	TWeakObjectPtr<APawn> HealthSource;     // 이름 해석 결과 캐시(매 프레임 선형 스캔 회피). 직렬화 안 함.
	float                 HealthBarFullWidth = -1.0f;  // 최초 바인딩 시 캡처한 100% 기준 폭. 직렬화 안 함.
};
