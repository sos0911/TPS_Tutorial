#pragma once


#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TPSConsts.generated.h"


UENUM( BlueprintType )
enum class EWeaponType
{
	Pistol,          // 권총
	AssaultRifle,    // 돌격소총
	SniperRifle,     // 저격소총
	RocketLauncher,  // 로켓 런처
	GrenadeLauncher, // 수류탄 발사기
	Shotgun,         // 샷건
	Knife,           // 칼
};

USTRUCT( BlueprintType )
struct FWeaponData : public FTableRowBase
{
	GENERATED_BODY()

	// WeaponName
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	FName WeaponName;

	// WeaponType
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	EWeaponType WeaponType;

	// Damage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	float Damage;

	// MagazineSize
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	int32 MagazineSize;

	// PickUpWeapon: BP Pick Up We 클래스를 참조 (APickUpActor의 자식 클래스)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	TSubclassOf< class AActor > PickUpWeapon;

	// EquipWeapon: BP Equip We 클래스를 참조 (AEquipActor의 자식 클래스)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	TSubclassOf< class AActor > EquipWeapon;
};