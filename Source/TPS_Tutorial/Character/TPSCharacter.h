// TPS 캐릭터 클래스 헤더 파일


#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TPSCharacter.generated.h"


class UInputAction;


UCLASS()
class TPS_TUTORIAL_API ATPSCharacter : public ACharacter
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
	const FString TPSCameraCompName     = TEXT( "TPSCamera"     );
	const FString TPSZoomCameraCompName = TEXT( "TPSZoomCamera" );
	const FString FPSCameraCompName     = TEXT( "FPSCamera"     );

	UChildActorComponent*   TPSCameraComp     = nullptr;
	UChildActorComponent*   TPSZoomCameraComp = nullptr;
	UChildActorComponent*   FPSCameraComp     = nullptr;
	USkeletalMeshComponent* FaceComp		  = nullptr;

	bool IsTPSMode  = true;  // TPS 모드인가 여부
	bool IsZoomMode = false; // zoom 모드인가 여부
	bool IsLeaning  = false; // 기울이고 있는가 여부

	float TargetRollValue = 0.0f; // 목표 기울이기 값

public:
	// Sets default values for this character's properties
	ATPSCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime ) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent( class UInputComponent* PlayerInputComponent ) override;

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

	// 카메라 시점을 변경한다.
	UFUNCTION( BlueprintCallable, Category = "Camera Control" )
	void ToggleCameraMode( const FInputActionValue& Value );

	// 줌 시점을 변경한다.
	UFUNCTION( BlueprintCallable, Category = "Camera Control" )
	void ToggleZoomMode( const FInputActionValue& Value );
};
