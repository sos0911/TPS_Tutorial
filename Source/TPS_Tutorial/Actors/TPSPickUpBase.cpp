// Fill out your copyright notice in the Description page of Project Settings.


#include "TPSPickUpBase.h"


// Sets default values
ATPSPickUpBase::ATPSPickUpBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ATPSPickUpBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// 무기를 줍는 상호작용을 실행한다.
void ATPSPickUpBase::HandlePickUpWeaponInteract()
{
	// TODO : 액터 디스폰 처리 및 정리
}

// Called every frame
void ATPSPickUpBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

