#include "BossPatternSelectorComponent.h"

#include "Component/Gameplay/BulletHellComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/GameMode/GameplayStatics.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"

#include <algorithm>
#include <cstdlib>

namespace
{
	FVector SafeDirection(const FVector& Direction, const FVector& Fallback)
	{
		return Direction.IsNearlyZero() ? Fallback : Direction.Normalized();
	}

	float RandomUnitFloat()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}
}

UBossPatternSelectorComponent::UBossPatternSelectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBossPatternSelectorComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	RefreshPatternComponents();
	bSelectionStarted = bAutoStart;
}

void UBossPatternSelectorComponent::RefreshPatternComponents()
{
	PatternComponents.clear();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		if (UBossPatternComponentBase* Pattern = Cast<UBossPatternComponentBase>(Component))
		{
			PatternComponents.push_back(Pattern);
		}
	}
}

void UBossPatternSelectorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	(void)TickType;
	(void)ThisTickFunction;

	if (!bSelectionStarted || !bEnablePatternSelection)
	{
		UpdateDebugStateFromActive();
		return;
	}

	FBossPatternContext Context = BuildContext(DeltaTime);
	TickPatternCooldowns(DeltaTime);
	TickFallbackIdle(DeltaTime);
	TickActivePattern(DeltaTime, Context);

	if (!ActivePattern.Get() && FallbackIdleRemaining <= 0.0f)
	{
		TrySelectNextPattern(Context);
	}

	UpdateDebugStateFromActive();
}

FBossPatternContext UBossPatternSelectorComponent::BuildContext(float DeltaTime)
{
	FBossPatternContext Context;
	Context.BossActor = GetOwner();
	Context.TargetActor = ResolveTargetActor();
	Context.BulletHell = ResolveBulletHellComponent();
	Context.DeltaTime = DeltaTime;
	Context.BossPhase = 0;
	Context.BossHealthRatio = 1.0f;

	if (Context.BossActor)
	{
		Context.BossLocation = Context.BossActor->GetActorLocation();
		Context.BossForward = SafeDirection(Context.BossActor->GetActorForward(), FVector::ForwardVector);
		Context.BossRight = SafeDirection(Context.BossActor->GetActorRight(), FVector::RightVector);
		Context.BossUp = SafeDirection(Context.BossActor->GetActorUp(), FVector::UpVector);
	}

	if (Context.TargetActor)
	{
		Context.TargetLocation = Context.TargetActor->GetActorLocation();
		Context.DistanceToTarget = FVector::Distance(Context.BossLocation, Context.TargetLocation);
		Context.DirectionToTarget = SafeDirection(Context.TargetLocation - Context.BossLocation, Context.BossForward);
	}
	else
	{
		Context.TargetLocation = Context.BossLocation + Context.BossForward;
		Context.DistanceToTarget = 0.0f;
		Context.DirectionToTarget = Context.BossForward;
	}

	return Context;
}

AActor* UBossPatternSelectorComponent::ResolveTargetActor() const
{
	if (AActor* ExplicitTarget = TargetActor)
	{
		return ExplicitTarget;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (!TargetActorName.empty())
	{
		for (AActor* Actor : World->GetActors())
		{
			if (Actor && Actor->GetName() == TargetActorName)
			{
				return Actor;
			}
		}
	}

	if (TargetTag.IsValid() && TargetTag != FName::None)
	{
		if (AActor* TaggedActor = FGameplayStatics::FindFirstActorByTag(World, TargetTag))
		{
			return TaggedActor;
		}
	}

	if (bAutoResolvePlayerTarget)
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			return PlayerController->GetPossessedPawn();
		}
	}

	return nullptr;
}

UBulletHellComponent* UBossPatternSelectorComponent::ResolveBulletHellComponent() const
{
	AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetComponentByClass<UBulletHellComponent>() : nullptr;
}

void UBossPatternSelectorComponent::TickPatternCooldowns(float DeltaTime)
{
	for (TWeakObjectPtr<UBossPatternComponentBase>& PatternRef : PatternComponents)
	{
		if (UBossPatternComponentBase* Pattern = PatternRef.Get())
		{
			Pattern->TickCooldown(DeltaTime);
		}
	}
}

void UBossPatternSelectorComponent::TickActivePattern(float DeltaTime, const FBossPatternContext& Context)
{
	UBossPatternComponentBase* Pattern = ActivePattern.Get();
	if (!Pattern)
	{
		return;
	}

	Pattern->TickPattern(DeltaTime, Context);
	if (Pattern->IsPatternFinished())
	{
		LogSelectionEvent("finished", Pattern, nullptr);
		ActivePattern.Reset();
	}
}

void UBossPatternSelectorComponent::TrySelectNextPattern(const FBossPatternContext& Context)
{
	if (PatternComponents.empty())
	{
		DebugState.LastRejectedReason = "no pattern components";
		EnterFallbackIdle();
		return;
	}

	UBossPatternComponentBase* SelectedPattern = SelectWeightedPattern(Context);
	if (!SelectedPattern)
	{
		EnterFallbackIdle();
		return;
	}

	StartPattern(SelectedPattern, Context);
}

UBossPatternComponentBase* UBossPatternSelectorComponent::SelectWeightedPattern(const FBossPatternContext& Context)
{
	TArray<UBossPatternComponentBase*> UsablePatterns;
	float TotalWeight = 0.0f;
	FString LastRejectReason = "no usable pattern";
	int32 CandidateCount = 0;

	for (TWeakObjectPtr<UBossPatternComponentBase>& PatternRef : PatternComponents)
	{
		UBossPatternComponentBase* Pattern = PatternRef.Get();
		if (!Pattern)
		{
			continue;
		}

		++CandidateCount;
		FString RejectReason;
		if (!Pattern->GetCanUse(Context, &RejectReason))
		{
			if (!RejectReason.empty())
			{
				LastRejectReason = RejectReason;
			}
			continue;
		}

		if (IsBlockedByRecentPattern(Pattern))
		{
			LastRejectReason = Pattern->GetPatternName() + ": repeat blocked";
			continue;
		}

		UsablePatterns.push_back(Pattern);
		TotalWeight += (std::max)(0.0f, Pattern->GetWeight());
	}

	DebugState.CandidateCount = CandidateCount;
	DebugState.UsableCandidateCount = static_cast<int32>(UsablePatterns.size());

	if (UsablePatterns.empty() || TotalWeight <= 0.0f)
	{
		DebugState.LastRejectedReason = LastRejectReason;
		LogSelectionEvent("fallback", nullptr, LastRejectReason.c_str());
		return nullptr;
	}

	float Pick = RandomUnitFloat() * TotalWeight;
	for (UBossPatternComponentBase* Pattern : UsablePatterns)
	{
		Pick -= (std::max)(0.0f, Pattern->GetWeight());
		if (Pick <= 0.0f)
		{
			return Pattern;
		}
	}

	return UsablePatterns.back();
}

bool UBossPatternSelectorComponent::IsBlockedByRecentPattern(const UBossPatternComponentBase* Pattern) const
{
	if (!Pattern || !Pattern->BlocksImmediateRepeat() || RepeatBlockCount <= 0)
	{
		return false;
	}

	const FString& PatternName = Pattern->GetPatternName();
	return std::find(RecentPatternNames.begin(), RecentPatternNames.end(), PatternName) != RecentPatternNames.end();
}

void UBossPatternSelectorComponent::RecordRecentPattern(const UBossPatternComponentBase* Pattern)
{
	if (!Pattern || RepeatBlockCount <= 0)
	{
		return;
	}

	RecentPatternNames.push_back(Pattern->GetPatternName());
	while (static_cast<int32>(RecentPatternNames.size()) > RepeatBlockCount)
	{
		RecentPatternNames.erase(RecentPatternNames.begin());
	}
}

void UBossPatternSelectorComponent::EnterFallbackIdle()
{
	ActivePattern.Reset();
	FallbackIdleRemaining = (std::max)(0.0f, FallbackIdleDuration);
	++DebugState.FallbackCount;
	DebugState.ActivePatternName = "FallbackIdle";
	DebugState.ActiveStep = EBossPatternStep::None;
	DebugState.ActiveStepElapsed = 0.0f;
	DebugState.ActivePatternElapsed = 0.0f;
}

void UBossPatternSelectorComponent::TickFallbackIdle(float DeltaTime)
{
	if (FallbackIdleRemaining <= 0.0f)
	{
		return;
	}

	FallbackIdleRemaining = (std::max)(0.0f, FallbackIdleRemaining - DeltaTime);
}

void UBossPatternSelectorComponent::StartPattern(UBossPatternComponentBase* Pattern, const FBossPatternContext& Context)
{
	if (!Pattern)
	{
		return;
	}

	Pattern->NotifySelected();
	Pattern->StartPattern(Context);
	ActivePattern = Pattern;
	RecordRecentPattern(Pattern);
	DebugState.LastSelectedPatternName = Pattern->GetPatternName();
	++DebugState.SelectionCount;
	LogSelectionEvent("selected", Pattern, nullptr);
}

void UBossPatternSelectorComponent::LogSelectionEvent(
	const char* EventName,
	const UBossPatternComponentBase* Pattern,
	const char* Reason) const
{
	if (!bLogPatternSelection)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	UE_LOG(
		"BossPattern %s. Owner=%s Pattern=%s Reason=%s Candidates=%d Usable=%d",
		EventName ? EventName : "event",
		OwnerActor ? OwnerActor->GetName().c_str() : "None",
		Pattern ? Pattern->GetPatternName().c_str() : "None",
		Reason ? Reason : "None",
		DebugState.CandidateCount,
		DebugState.UsableCandidateCount);
}

void UBossPatternSelectorComponent::UpdateDebugStateFromActive()
{
	if (UBossPatternComponentBase* Pattern = ActivePattern.Get())
	{
		DebugState.ActivePatternName = Pattern->GetPatternName();
		DebugState.ActiveStep = Pattern->GetCurrentStep();
		DebugState.ActiveStepElapsed = Pattern->GetStepElapsed();
		DebugState.ActivePatternElapsed = Pattern->GetPatternElapsed();
		return;
	}

	if (FallbackIdleRemaining > 0.0f)
	{
		DebugState.ActivePatternName = "FallbackIdle";
		DebugState.ActiveStep = EBossPatternStep::None;
		DebugState.ActiveStepElapsed = FallbackIdleDuration - FallbackIdleRemaining;
		DebugState.ActivePatternElapsed = DebugState.ActiveStepElapsed;
		return;
	}

	DebugState.ActivePatternName = "None";
	DebugState.ActiveStep = EBossPatternStep::None;
	DebugState.ActiveStepElapsed = 0.0f;
	DebugState.ActivePatternElapsed = 0.0f;
}
