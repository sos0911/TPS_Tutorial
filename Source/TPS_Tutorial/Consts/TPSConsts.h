#pragma once


#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TPSConsts.generated.h"


UENUM( BlueprintType )
enum class EWeaponType : uint8
{
	Pistol,          // 권총
	AssaultRifle,    // 돌격소총
	SniperRifle,     // 저격소총
	RocketLauncher,  // 로켓 런처
	GrenadeLauncher, // 수류탄 발사기
	Shotgun,         // 샷건
	Knife,           // 칼
	Max
};

UENUM( BlueprintType )
enum class ECharacterMoveDirection : uint8
{
	Forward,  // 앞쪽 
	Backward, // 뒤쪽
	Left,     // 왼쪽
	Right,    // 오른쪽
	Max       
};

USTRUCT( BlueprintType )
struct FWeaponData : public FTableRowBase
{
	GENERATED_BODY()

	// WeaponName
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	FName WeaponName = TEXT( "" );

	// WeaponType
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	EWeaponType WeaponType = EWeaponType::Max;

	// Damage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	float Damage = 0.0f;

	// MagazineSize
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	int32 MagazineSize = 0;

	// PickUpWeapon: BP Pick Up We 클래스를 참조 (APickUpActor의 자식 클래스)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	TSubclassOf< class AActor > PickUpWeapon;

	// EquipWeapon: BP Equip We 클래스를 참조 (AEquipActor의 자식 클래스)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
	TSubclassOf< class AActor > EquipWeapon;
};