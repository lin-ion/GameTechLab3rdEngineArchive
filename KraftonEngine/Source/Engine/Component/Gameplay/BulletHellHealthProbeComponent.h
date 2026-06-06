#pragma once

#include "Component/ActorComponent.h"

#include "Source/Engine/Component/Gameplay/BulletHellHealthProbeComponent.generated.h"

// 탄막에 맞았을 때 이벤트 받아서 체력 감소시키는 컴포넌트
// TODO: 임시 체력값 대신 실제 플레이어 체력을 감소시키도록 컴포넌트 연결
UCLASS()
class UBulletHellHealthProbeComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UBulletHellHealthProbeComponent();
	~UBulletHellHealthProbeComponent() override = default;

	void BeginPlay() override;

	UFUNCTION(Callable, Category="Bullet Hell|Health Probe")
	float ApplyDamage(float DamageAmount);

	UFUNCTION(Callable, Category="Bullet Hell|Health Probe")
	void ResetHealth();

	UFUNCTION(Pure, Category="Bullet Hell|Health Probe")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(Pure, Category="Bullet Hell|Health Probe")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(Pure, Category="Bullet Hell|Health Probe")
	int32 GetHitCount() const { return HitCount; }

private:
	UPROPERTY(Edit, Save, Category="Bullet Hell|Health Probe", DisplayName="Max Health", Min=0.0f, Max=1000000.0f, Speed=1.0f)
	float MaxHealth = 10.0f;	// 테스트용 임시 최대체력값

	UPROPERTY(Edit, Save, Category="Bullet Hell|Health Probe", DisplayName="Current Health", Min=0.0f, Max=1000000.0f, Speed=1.0f)
	float CurrentHealth = 10.0f;	// 테스트용 임시 현재체력값

	UPROPERTY(Edit, Save, Category="Bullet Hell|Health Probe", DisplayName="Reset Health On Begin Play")
	bool bResetHealthOnBeginPlay = true;

	UPROPERTY(Save, Category="Bullet Hell|Health Probe", DisplayName="Total Damage Taken")
	float TotalDamageTaken = 0.0f;

	UPROPERTY(Save, Category="Bullet Hell|Health Probe", DisplayName="Hit Count")
	int32 HitCount = 0;
};
