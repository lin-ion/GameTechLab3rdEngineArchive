#pragma once

#include "Component/ActorComponent.h"

#include "Source/Engine/Component/Gameplay/BulletHellDamageReceiverComponent.generated.h"

class APawn;

UCLASS()
class UBulletHellDamageReceiverComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UBulletHellDamageReceiverComponent();
	~UBulletHellDamageReceiverComponent() override = default;

	UFUNCTION(Callable, Category="Bullet Hell|Damage Receiver")
	float ApplyDamage(float DamageAmount);

	UFUNCTION(Pure, Category="Bullet Hell|Damage Receiver")
	int32 GetHitCount() const { return HitCount; }

	UFUNCTION(Pure, Category="Bullet Hell|Damage Receiver")
	float GetTotalDamageForwarded() const { return TotalDamageForwarded; }

private:
	APawn* GetOwnerPawn() const;

	UPROPERTY(Save, Category="Bullet Hell|Damage Receiver", DisplayName="Total Damage Forwarded")
	float TotalDamageForwarded = 0.0f;

	UPROPERTY(Save, Category="Bullet Hell|Damage Receiver", DisplayName="Hit Count")
	int32 HitCount = 0;
};
