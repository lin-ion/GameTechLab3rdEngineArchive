#include "MovementComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Math/Utils.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UMovementComponent, UActorComponent)
REGISTER_FACTORY(UMovementComponent)

UMovementComponent* UMovementComponent::Duplicate()
{
	UMovementComponent* NewComp = UObjectManager::Get().CreateObject<UMovementComponent>();
	CopyMovementStateTo(NewComp);
	NewComp->DuplicateSubObjects();
	return NewComp;
}

void UMovementComponent::CopyMovementStateTo(UMovementComponent* NewComp) const
{
	if (NewComp == nullptr)
	{
		return;
	}
	NewComp->SetActive(IsActive());
	NewComp->SetOwner(nullptr);
	NewComp->SetComponentTickEnabled(IsComponentTickEnabled());
	NewComp->UpdatedComponent = nullptr;
	NewComp->Velocity = Velocity;
}

void UMovementComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (UpdatedComponent == nullptr)
	{
		AActor* OwnerActor = GetOwner();
		if (OwnerActor != nullptr)
		{
			SetUpdatedComponent(OwnerActor->GetRootComponent());
		}
	}
}

void UMovementComponent::TickComponent(float DeltaTime)
{
	UActorComponent::TickComponent(DeltaTime);
	/*
	* 해당 부분은 세부 동작을 정의하는 자식 컴포넌트에서 override해서 작성한다.
	*/
}

void UMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Velocity", EPropertyType::Vec3, &Velocity, 0.0f, 0.0f, 0.1f });
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	UpdatedComponent = NewUpdatedComponent;
	SetComponentTickEnabled(UpdatedComponent != nullptr);
	UpdateComponentVelocity();
}

bool UMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation)
{
	if (UpdatedComponent)
	{
		return UpdatedComponent->MoveComponent(Delta, NewRotation);
	}
	return false;
}

bool UMovementComponent::ShouldSkipUpdate(float DeltaTime) const
{
	return UpdatedComponent == nullptr || DeltaTime <= MathUtil::Epsilon;
}

void UMovementComponent::StopMovementImmediately()
{
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();
}

void UMovementComponent::UpdateComponentVelocity()
{
	if (UpdatedComponent)
	{
		UpdatedComponent->ComponentVelocity = Velocity;
	}
}
