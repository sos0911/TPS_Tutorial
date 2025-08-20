### Project Development Guidelines (.junie/guidelines.md)

This document captures conventions observed in TPS_Tutorial, primarily inferred from Source/TPS_Tutorial/Character/TPSCharacter.h and TPSCharacter.cpp (UE 5.3.2). Follow these to keep the codebase consistent.

---

### File layout and high-level formatting

- File header: one-line Korean description comment at the very top.
- Blank lines: two blank lines are used to visually separate major sections.
  - After the top comment → 2 blank lines.
  - Before and after `#pragma once` (headers only) → 2 blank lines before includes.
  - After the last `#include` block → 2 blank lines before forward declarations or code.
- Brace style: Allman (opening brace on the next line).
- Indentation: tabs (as in existing files). Keep consistent with surrounding code.
- Spaces inside parentheses and templates:
  - Conditions: `if ( condition )`, `for ( ... )`, `while ( ... )`.
  - Function calls/decls: `Func( Arg1, Arg2 )` (space inside parentheses around args; overall style mirrors TPSCharacter).
  - Templates: `Value.Get< FVector2D >()` (note spaces inside `< >`).
- Trailing newline at end of file.

---

### Header (.h) file structure

- Top-of-file:
  - `// 간단한 파일 설명 (한 줄)`
  - (2 blank lines)
  - `#pragma once`
  - (2 blank lines)
  - `#include` block with engine/framework headers first, then the generated header last.
    - Example order:
      - `#include "CoreMinimal.h"`
      - `#include "GameFramework/Character.h"` (or appropriate base)
      - `#include "ThisClass.generated.h"` (always last in the header include list)
  - (2 blank lines)
  - Forward declarations as needed (e.g., `class UInputAction;`).

- UCLASS declaration:
  - `UCLASS()` on its own line.
  - `class <MODULE_API> AMyClass : public ASuperClass` on its own line.
  - Opening brace on the next line, Allman style.
  - `GENERATED_BODY()` on its own line, followed by a blank line.

- Access sections grouped and ordered:
  1) `protected:` properties for design-time configuration (UPROPERTY with EditAnywhere/BlueprintReadOnly as needed)
  2) `private:` constants, component pointers, and internal state flags
  3) `public:` ctor and Unreal overrides
  4) `protected:` or `public:` UFUNCTIONs grouped by feature (with Categories)

- Property and method annotations:
  - UPROPERTY placed immediately above the member with categories and editability:
    - `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")`
  - UFUNCTION style:
    - `UFUNCTION( BlueprintCallable, Category = "Character Control" )`
  - Use Korean one-line comments above members/functions to describe purpose.

- Naming and initialization patterns:
  - Constants: `const FString TPSCameraCompName = TEXT( "TPSCamera" );` (note spaces around `=` and inside `TEXT( "..." )`).
  - UObject pointers as raw pointers initialized to `nullptr` in-class.
  - Floats initialized inline: `float Pitch = 0.0f;`
  - Booleans initialized inline: `bool IsTPSMode = true;`
  - Member naming uses PascalCase without Hungarian notation; booleans prefixed with `Is` (e.g., `IsZoomMode`).

- Function declarations:
  - Space around parameters: `void Move( const FInputActionValue& Value );`
  - Group related functions and add Korean comments above each group and method.

---

### Source (.cpp) file structure

- Top-of-file:
  - `// 간단한 파일 설명 (한 줄)`
  - (2 blank lines)
  - Includes: own header first, then engine/framework headers.
    - Example:
      - `#include "TPSCharacter.h"`
      - `#include "EnhancedInputSubsystems.h"`
      - `#include "Kismet/KismetMathLibrary.h"`
  - (2 blank lines)

- Function layout and style:
  - Each Unreal override is preceded by a Korean comment describing the lifecycle event.
    - `// Sets default values`, `// Called when the game starts or when spawned`, `// Called every frame`, `// Called to bind functionality to input`.
  - Project-specific functions also have Korean one-line comments: `// 이동한다.`, `// 시점을 이동한다.`, `// 상체를 기울인다.`, `// 카메라 시점을 변경한다.`, `// 줌 시점을 변경한다.`
  - Early-return guards at top of functions (type checks, null checks):
    - `if ( Value.GetValueType() != EInputActionValueType::Axis2D ) return;`
    - `if ( !playerController ) return;`
    - `if ( !TPSCameraComp || !TPSZoomCameraComp || !FPSCameraComp ) return;`
  - Spacing style consistent with header:
    - `if ( !FMath::IsNearlyEqual( Roll, TargetRollValue ) )`
    - `for ( UChildActorComponent* comp : comps )`
  - Math/utility calls with explicit formatting:
    - `AddMovementInput( UKismetMathLibrary::GetRightVector( FRotator( rotator.Pitch, rotator.Yaw, 0.0f ) ), Value.Get< FVector2D >().X );`
    - Use `FMath::Clamp`, `FMath::FInterpTo`, `FMath::GetMappedRangeValueClamped`.

- Camera/view management patterns:
  - Toggle TPS/FPS view with `APlayerController::SetViewTargetWithBlend( Actor, BlendTime )`.
  - Use `TWeakObjectPtr` + timer to defer visibility changes until after blend:
    - Capture `this` carefully in lambdas and check `thisPtr.IsValid()` before use.
  - Store and compare component names via `FString::Equals` with constants.

- Component discovery:
  - Use `GetComponents( UChildActorComponent::StaticClass(), Array )` and loop with null checks.
  - Identify components by `GetName().Equals( ConstantName )`.

- Input handling (Enhanced Input):
  - Validate `FInputActionValue` type before reading values.
  - Use `Value.Get< FVector2D >()` / `Value.Get< float >()` according to action type.

---

### Comments and localization

- Comments are in Korean and concise.
- Place a one-line Korean description above each method.
- Inline TODOs are acceptable and kept brief, e.g.:
  - `// TODO : Roll 이 RightVector 계산에 필요한가 검토`

---

### Naming conventions

- Classes: PascalCase (e.g., `ATPSCharacter`).
- Members: PascalCase (e.g., `MoveAction`, `TPSCameraCompName`, `FaceComp`).
- 함수 내 지역 변수명은 소문자로 시작할 것
- Booleans: `Is` prefix (e.g., `IsTPSMode`, `IsZoomMode`, `IsLeaning`).
- Categories: Title Case English strings (e.g., `"Character Control"`, `"Camera Control"`).
- TEXT strings: `TEXT( "Literal" )` with spaces inside parentheses.

---

### Property and ownership considerations

- Current code uses raw UObject pointers (e.g., `UChildActorComponent*`) as private members without UPROPERTY.
  - This mirrors the existing style but note: without UPROPERTY, GC will not track these members. In this project, components are owned by the actor so they remain valid, but if you store references to arbitrary UObjects, prefer `UPROPERTY()` or `TWeakObjectPtr` as appropriate.

---

### Error handling and defensive programming

- Prefer early returns for invalid states.
- Use `KINDA_SMALL_NUMBER` and `FMath::IsNearlyEqual` for float comparisons.
- Clamp and interpolate floats using `FMath` helpers.

---

### Example header template (follow spacing exactly)

```cpp
// <파일 설명 한 줄>


#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyActor.generated.h"


class UInputAction;


UCLASS()
class TPS_TUTORIAL_API AMyActor : public AActor
{
	GENERATED_BODY()

protected:
	// 이동 IA
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

private:
	const FString SomeName = TEXT( "SomeComponent" );
	USceneComponent* SomeComp = nullptr;
	bool IsActive = false;
	float Speed = 0.0f;

public:
	// Sets default values for this actor's properties
	AMyActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick( float DeltaTime ) override;

protected:
	// 이동한다.
	UFUNCTION( BlueprintCallable, Category = "Character Control" )
	void Move( const FInputActionValue& Value );
};
```

---

### Example source template

```cpp
// <파일 설명 한 줄>


#include "MyActor.h"
#include "Kismet/KismetMathLibrary.h"


// Sets default values
AMyActor::AMyActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMyActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AMyActor::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );
}

// 이동한다.
void AMyActor::Move( const FInputActionValue& Value )
{
	if ( Value.GetValueType() != EInputActionValueType::Axis2D ) return;
	AddActorWorldOffset( FVector( Value.Get< FVector2D >().X, Value.Get< FVector2D >().Y, 0.0f ) );
}
```

---

### Unreal-specific tips observed in this project

- View blending:
  - Use small blend times (e.g., 0.2f) and defer mesh visibility toggles with a timer to avoid popping mid-blend.
- Leaning mechanic:
  - Maintain a target roll and interpolate (`FInterpTo`) when leaning, reset toward 0 when not leaning.
- Movement vectors:
  - Derive forward from yaw-only rotator; right may include pitch in code, but there is a TODO questioning necessity—follow design decisions accordingly.

---

### Checklist for new classes

- [ ] Add a one-line Korean file description at the top of both .h and .cpp.
- [ ] Respect the two-blank-line separation around major sections and includes.
- [ ] Put `#include "<Class>.generated.h"` last in header includes.
- [ ] Use Allman braces and project spacing rules (spaces inside parentheses and templates).
- [ ] Group members by access level as in TPSCharacter.
- [ ] Use Korean one-line comments above each method and logical group.
- [ ] Validate `FInputActionValue` types before using.
- [ ] Prefer early returns and `FMath` helpers for floats.
- [ ] Consider UPROPERTY for UObject members if ownership/GC requires it.

---

### Version and environment

- Unreal Engine: 5.3.2
- Language: C++ (Unreal style adapted to project-specific spacing and comments)

This guideline will be updated as more modules are added. If you touch other files (e.g., TPSPlayerController, TPSPlayerCameraManager, TPSLog), align with the same spacing and comment practices demonstrated here.
