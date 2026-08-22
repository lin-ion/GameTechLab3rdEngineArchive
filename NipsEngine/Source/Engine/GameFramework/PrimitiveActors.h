#pragma once

#include "AActor.h"

class UTextRenderComponent;
class UDecalComponent;
class UFireBallComponent;
class UHeightFogComponent;
class UBillboardComponent;

//class ACubeActor : public AActor
//{
//public:
//	DECLARE_CLASS(ACubeActor, AActor)
//	ACubeActor() = default;
//
//	void InitDefaultComponents();
//};
//
//class ASphereActor : public AActor
//{
//public:
//	DECLARE_CLASS(ASphereActor, AActor)
//	ASphereActor() = default;
//};

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() = default;

	void InitDefaultComponents();
};

class ASubUVActor : public AActor
{
public:
    DECLARE_CLASS(ASubUVActor, AActor)
    ASubUVActor() = default;

    void InitDefaultComponents();
};

class ATextRenderActor : public AActor
{
public:
    DECLARE_CLASS(ATextRenderActor, AActor)
    ATextRenderActor() = default;

    void InitDefaultComponents();
};

class ABillboardActor : public AActor
{
public:
    DECLARE_CLASS(ABillboardActor, AActor)
	ABillboardActor() = default;

    void InitDefaultComponents();
};

class ADecalActor : public AActor
{
public:
	DECLARE_CLASS(ADecalActor, AActor)
	ADecalActor() = default;

	// 발제: DecalComponent를 맴버 변수로 갖는 ADecalActor를 구현한다.
	UDecalComponent* DecalComponent{ nullptr };
	UBillboardComponent* IconBillboardComponent{ nullptr };

	void InitDefaultComponents();
};

class ASpotLightActor : public AActor
{
public:
	DECLARE_CLASS(ASpotLightActor, AActor)
	ASpotLightActor() = default;

	UBillboardComponent* IconBillboardComponent{ nullptr };

	void InitDefaultComponents();
};
class AFireBallActor : public AActor
{
public:
	DECLARE_CLASS(AFireBallActor, AActor)
	AFireBallActor() = default;

	UFireBallComponent* FireBallComponent;

	void InitDefaultComponents();
};

class AHeightFogActor : public AActor
{
public:
	DECLARE_CLASS(AHeightFogActor, AActor)
	AHeightFogActor() = default;

	// 발제: DecalComponent를 맴버 변수로 갖는 ADecalActor를 구현한다.
	UHeightFogComponent* HeightFogComponent;
	void InitDefaultComponents();
};