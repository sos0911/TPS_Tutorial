// TPS 캐릭터 클래스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logic/ITPSInteractionActorInterface.h"
#include "Logic/Common/TPSTypes.h"
#include "TPSCharacter.generated.h"


class UInputAction;
class UChildActorComponent;
class USkeletalMeshComponent;
struct FInputActionValue;


UCLASS()
class TPS_TUTORIAL_API ATPSCharacter : public ACharacter, public ITPSPickUpInteractionActorInterface
{
	GENERATED_BODY()

protected:
	// 이동 IA
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// pitch 회전값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	float Pitch = 0.0f;  

	// Roll 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	float Roll = 0.0f;

private:
	const FString TPSCameraCompName     = TEXT( "TPSCamera"     ); // TPS 카메라 컴포넌트 이름
	const FString TPSZoomCameraCompName = TEXT( "TPSZoomCamera" ); // TPS 줌 카메라 컴포넌트 이름
	const FString FPSCameraCompName     = TEXT( "FPSCamera"     ); // FPS 카메라 컴포넌트 이름

	UChildActorComponent*   TPSCameraComp     = nullptr; // TPS 카메라 컴포넌트 객체
	UChildActorComponent*   TPSZoomCameraComp = nullptr; // TPS 줌 카메라 컴포넌트 객체
	UChildActorComponent*   FPSCameraComp     = nullptr; // FPS 카메라 컴포넌트 객체
	USkeletalMeshComponent* FaceComp		  = nullptr; // 얼굴 컴포넌트 객체
	USkeletalMeshComponent* BodyComp		  = nullptr; // 몸통 컴포넌트 객체
	TPSActorPtr				CurrentWeapon	  = nullptr; // 현재 장착중인 무기 객체

	bool IsTPSMode  = true;  // TPS 모드인가 여부
	bool IsZoomMode = false; // zoom 모드인가 여부
	bool IsLeaning  = false; // 기울이고 있는가 여부
	bool IsJumping  = false; // 점프하고 있는가 여부

	float TargetRollValue = 0.0f; // 목표 기울이기 값

public:
	// 현재 Lean Roll 값을 반환한다.
	UFUNCTION( BlueprintPure, Category = "Camera Control" )
	float GetLeanRoll() const { return Roll; }

protected:
	// 이동한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void Move( const FInputActionValue& Value );

	// 시점을 이동한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void LookAround( const FInputActionValue& Value );

	// 상체를 기울인다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void Lean( const FInputActionValue& Value );

	// 점프한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void DoJump( const FInputActionValue& Value );

	// 무기를 드랍한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void Drop( const FInputActionValue& Value );

	// 카메라 시점을 변경한다.
	UFUNCTION( BlueprintCallable, Category = "Camera Control" )
	void ToggleCameraMode( const FInputActionValue& Value );

	// 줌 시점을 변경한다.
	UFUNCTION( BlueprintCallable, Category = "Camera Control" )
	void ToggleZoomMode( const FInputActionValue& Value );

public:
	// Sets default values for this character's properties
	ATPSCharacter();

	// Called every frame
	virtual void Tick(float DeltaTime ) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent( class UInputComponent* PlayerInputComponent ) override;

	// 오버랩이 시작되었음을 알리는 이벤트를 처리한다.
	UFUNCTION()
	void OnBeginOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult );

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 무기를 줍는 상호작용을 실행한다.
	virtual bool HandlePickUpWeaponInteract( AActor* OtherActor ) override;
};
