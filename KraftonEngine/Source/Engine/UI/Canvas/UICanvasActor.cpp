#include "UI/Canvas/UICanvasActor.h"

#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UITextElement.h"
#include "UI/Canvas/UICanvasManager.h"
#include "UI/UIAsset.h"
#include "UI/UIAssetManager.h"
#include "Serialization/SceneSaveManager.h"
#include "GameFramework/World.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "Core/Logging/Log.h"

#include <cstdio>

namespace
{
	// [사이클 2] 체력 비율 → 색: 가득(1)=녹 · 절반(0.5)=노 · 고갈(0)=적. 2구간 lerp 로 중간 브라운 회피.
	// FVector4 의 검증된 4-인자 생성자만 사용(성분 접근자 가정 없음).
	FVector4 HealthRatioToColor(float Ratio)
	{
		if (Ratio >= 0.5f)
		{
			const float T = (Ratio - 0.5f) * 2.0f;   // 노(0.5) → 녹(1.0)
			return FVector4(0.95f + (0.15f - 0.95f) * T,
			                0.85f,
			                0.15f + (0.20f - 0.15f) * T, 1.0f);
		}
		const float T = Ratio * 2.0f;                // 적(0.0) → 노(0.5)
		return FVector4(0.90f + (0.95f - 0.90f) * T,
		                0.15f + (0.85f - 0.15f) * T,
		                0.15f, 1.0f);
	}

	// [사이클 4] 바인딩 값 → 바 비율 [0,1]. 체력 값은 GetHealthRatio, GameTime 은 의미 없어 full(1).
	float ValueAsRatio(EHudBindingValue Value, APawn* Pawn)
	{
		switch (Value)
		{
		case EHudBindingValue::HealthCurrent:
		case EHudBindingValue::HealthOverMax:
		case EHudBindingValue::HealthPercent:
			return Pawn ? Pawn->GetHealthRatio() : 0.0f;
		case EHudBindingValue::GameTime:
			return 1.0f;
		}
		return 0.0f;
	}

	// [사이클 4] 바인딩 값 → 표시 문자열(텍스트 채널). 소스 무효 시 "--". snprintf 로 FString(const char*) 구성.
	FString FormatBindingValue(EHudBindingValue Value, APawn* Pawn, UWorld* World)
	{
		char Buf[64] = {};
		switch (Value)
		{
		case EHudBindingValue::HealthCurrent:
			if (Pawn) { snprintf(Buf, sizeof(Buf), "%d", (int)(Pawn->GetCurrentHealth() + 0.5f)); return FString(Buf); }
			break;
		case EHudBindingValue::HealthOverMax:
			if (Pawn) { snprintf(Buf, sizeof(Buf), "%d / %d", (int)(Pawn->GetCurrentHealth() + 0.5f), (int)(Pawn->GetMaxHealth() + 0.5f)); return FString(Buf); }
			break;
		case EHudBindingValue::HealthPercent:
			if (Pawn) { snprintf(Buf, sizeof(Buf), "%d%%", (int)(Pawn->GetHealthRatio() * 100.0f + 0.5f)); return FString(Buf); }
			break;
		case EHudBindingValue::GameTime:
			if (World) { snprintf(Buf, sizeof(Buf), "%ds", (int)World->GetGameTimeSeconds()); return FString(Buf); }
			break;
		}
		return FString("--");
	}

	// [사이클 4] 소스 폰 해석. 빈 이름=로컬 플레이어(possessed pawn), 지정 시 그 이름(GetFName)의 액터.
	APawn* ResolveSourcePawn(UWorld* World, const FString& Name)
	{
		if (!World)
		{
			return nullptr;
		}
		if (Name.empty())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				return PC->GetPossessedPawn();
			}
			return nullptr;
		}
		for (AActor* A : World->GetActors())
		{
			if (IsValid(A) && A->GetFName().ToString() == Name)
			{
				return Cast<APawn>(A);
			}
		}
		return nullptr;
	}
}

void AUICanvasActor::InitCanvas()
{
	if (Canvas.Get())
	{
		return;
	}
	// 로드된 씬: 컴포넌트 트리 직렬화(DeserializeSceneComponentTree)가 이미 UUICanvas 를
	// RootComponent 로 복원했을 수 있다. 그 경우 새로 만들지 말고 재사용한다(중복 생성 방지).
	if (UUICanvas* Existing = Cast<UUICanvas>(GetRootComponent()))
	{
		Canvas = Existing;
		return;
	}
	UUICanvas* NewCanvas = AddComponent<UUICanvas>();
	SetRootComponent(NewCanvas);
	Canvas = NewCanvas;
}

void AUICanvasActor::LoadFromAsset(const FString& InAssetPath)
{
	if (InAssetPath.empty())
	{
		return;
	}

	// 이 액터의 UI .uasset 참조를 기록한다(직렬화/재구성의 기준 — R4). 직접 호출돼도 에셋 참조 모델 유지.
	UIAssetPath = InAssetPath;

	// 파일 단위 로드(헤더 검증 + 트리 JSON 페이로드). 경로는 매니저가 project-relative 로 정규화.
	UUIAsset* Asset = FUIAssetManager::Get().Load(InAssetPath);
	if (!Asset)
	{
		UE_LOG("[UICanvasActor] LoadFromAsset: UI .uasset 로드 실패 — '%s'", InAssetPath.c_str());
		return;
	}

	// JSON 블롭 → 라이브 캔버스 트리. 컴포넌트는 이 액터(Owner=this)에 등록된다.
	// 선례: FUIEditorWidget::BuildLiveTree (CreateObject<AUICanvasActor> + DeserializeUITree + SetRootComponent).
	USceneComponent* Root = FSceneSaveManager::DeserializeUITree(Asset->GetCanvasData(), this);
	UUICanvas* NewCanvas = Cast<UUICanvas>(Root);
	if (!NewCanvas)
	{
		UE_LOG("[UICanvasActor] LoadFromAsset: 복원된 루트가 UUICanvas 가 아님 — '%s'", InAssetPath.c_str());
		return;
	}

	// [R2] InitCanvas() 의 "빈 UUICanvas 새로 생성" 분기를 일부러 우회한다. 복원된 캔버스를
	// 곧장 RootComponent 로 세팅함으로써, 빈 캔버스가 먼저 루트로 박힌 뒤 교체되어 두 개의
	// UUICanvas 가 생기는 이중 루트를 막는다. 이후 BeginPlay 에서 InitCanvas 가 다시 호출돼도
	// RootComponent 가 이미 UUICanvas 이므로 그걸 재사용한다(중복 생성 없음).
	SetRootComponent(NewCanvas);
	Canvas = NewCanvas;

	// [R1] 화면 렌더 등록(RegisterCanvas)은 여기서 하지 않고 BeginPlay 로 미룬다. 따라서 편집
	// 모드 월드(bHasBegunPlay=false → UWorld::AddActor 가 BeginPlay 미호출)에서는 드롭한 UI 가
	// 화면에 뜨지 않고, Play(PIE)/런타임에서만 보인다. 편집 모드 즉시 프리뷰는 의도적으로 후행
	// 사이클(진단 사이클 4)로 분리했다 — 버그가 아니다.
	// [R3] 만약 후행에서 편집 모드 프리뷰를 위해 이 자리(또는 스폰 시점)에서 RegisterCanvas 를
	// 하게 되면, 런타임 LayoutAll 의 bSyncExternal 경로를 타게 되어 RmlUi 텍스트가 게임 viewport
	// 로 새어나갈 수 있다. 그때는 에디터 전용 격리(bSyncExternal=false 또는 ImGui 미러)가 필요하다.
}

void AUICanvasActor::BeginPlay()
{
	Super::BeginPlay();

	// [R4] 에셋 참조 모델: UIAssetPath 가 있으면 .Scene 인라인 트리가 아니라 .uasset 에서 캔버스를
	// 재구성한다(저장 시 캔버스 서브트리를 직렬화하지 않으므로 — ShouldSerializeRootComponentTree —
	// 로드 후엔 트리가 없다). 단 이미 RootComponent 가 UUICanvas 면(편집 월드에서 빌드 후 PIE 복제로
	// 트리가 넘어온 경우) 재빌드하지 않고 재사용한다(이중 트리 방지 — R2 의 연장).
	if (!Cast<UUICanvas>(GetRootComponent()) && !UIAssetPath.IsNull())
	{
		LoadFromAsset(UIAssetPath.ToString());
	}

	InitCanvas();
	// [R1] UI 의 화면 렌더 합류는 이 등록 한 줄에 달려 있고, BeginPlay 는 PIE/런타임에서만
	// 돈다(에디터 월드는 BeginPlay 미진입). 즉 등록=PIE/런타임 한정 → 편집 모드에선 의도적으로
	// 안 보인다(메시는 프록시 기반이라 편집 모드에서도 보이는 것과 비대칭이지만 정상 동작이다).
	if (UUICanvas* C = Canvas.Get())
	{
		FUICanvasManager::Get().RegisterCanvas(C);
	}
}

bool AUICanvasActor::ShouldSerializeRootComponentTree() const
{
	// [R4] UIAssetPath 가 있으면 캔버스 트리는 .uasset 에서 재구성되므로 .Scene 에 인라인으로
	// 저장하지 않는다(인라인 트리 + 에셋 참조 동시 저장 → 중복/충돌 방지). 참조가 없으면 기존처럼 인라인.
	return UIAssetPath.IsNull();
}

void AUICanvasActor::EndPlay()
{
	if (UUICanvas* C = Canvas.Get())
	{
		FUICanvasManager::Get().UnregisterCanvas(C);
	}
	Super::EndPlay();
}

void AUICanvasActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateDataBindings();
}

void AUICanvasActor::UpdateDataBindings()
{
	// 캔버스 미구성(편집/미등록)이면 비활성. 매 프레임 호출되므로 빠른 탈출 우선.
	UUICanvas* C = Canvas.Get();
	if (!C)
	{
		return;
	}

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	// [사이클 4] 각 바인딩을 독립 적용: 소스 폰 해석(캐시) → 대상 요소 조회 → 채널별 setter.
	for (FHudBinding& B : Bindings)
	{
		if (B.TargetElement.empty())
		{
			continue;
		}

		// 소스 폰 해석 — 캐시 우선, 무효일 때만 재해석(매 프레임 선형 스캔 회피).
		APawn* Source = B.SourceCache.Get();
		if (!IsValid(Source))
		{
			Source = ResolveSourcePawn(W, B.SourceActor);
			B.SourceCache = Source;
		}

		UUIElement* El = C->FindByName(B.TargetElement);
		if (!El)
		{
			continue;
		}

		switch (B.Channel)
		{
		case EHudBindingChannel::BarWidth:
		{
			// 최초 적용 시 저작 폭을 100% 기준으로 캡처(음수 sentinel 일 때만).
			if (B.BarFullWidth < 0.0f)
			{
				B.BarFullWidth = El->GetSize().X;
			}
			const float    Ratio = ValueAsRatio(B.Value, Source);
			const FVector2 Cur   = El->GetSize();
			El->SetSize(FVector2(B.BarFullWidth * Ratio, Cur.Y));
			break;
		}
		case EHudBindingChannel::BarColor:
		{
			// draw 가 BackgroundColor 를 매 프레임 live read 하므로 relayout 불필요(D2).
			El->SetColor(HealthRatioToColor(ValueAsRatio(B.Value, Source)));
			break;
		}
		case EHudBindingChannel::Text:
		{
			// UUITextElement 계열만 텍스트 표시. OnLayoutUpdated 가 RmlUi 에 재푸시(재마운트 아님).
			if (UUITextElement* TextEl = Cast<UUITextElement>(El))
			{
				TextEl->SetText(FormatBindingValue(B.Value, Source, W));
			}
			break;
		}
		}
	}
}
