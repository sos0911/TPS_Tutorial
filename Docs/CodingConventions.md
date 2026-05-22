# TPS_Tutorial 코딩 컨벤션 & 룰

> **목적:** AI 어시스턴트 / 새 기여자가 본 프로젝트에 코드를 추가하거나 수정할 때 기존 스타일을 100% 일치시켜 작성하도록 강제하는 단일 레퍼런스. 본 문서는 실제 `Source/TPS_Tutorial/**` 트리를 역공학하여 추출한 사실 기반 룰이다.
> **엔진:** Unreal Engine 5.3
> **모듈:** `TPS_Tutorial` (export 매크로 `TPS_TUTORIAL_API`)
> **적용 범위:** 본 모듈 내 C++ 코드. BP 에셋 명명 컨벤션은 §10 참고.

---

## 0. 코드 작성 시 체크리스트 (Quick Lint)

작성 직후 아래 항목을 직접 점검할 것. 하나라도 위반하면 기존 코드와 어긋난다.

- [ ] 모든 `if`, `for`, `while`, 함수 호출의 괄호 **안쪽에 공백**: `if ( cond )`, `Func( arg )` — §3.1
- [ ] 모든 템플릿/캐스트 angle bracket **안쪽에 공백**: `Cast< UTPSHUD >( ptr )`, `TArray< T* >` — §3.2
- [ ] **들여쓰기 = 탭(Tab)**, 공백 아님 — §3.3
- [ ] 함수/클래스 `{`는 **새 줄** (Allman). 단, BP 바인딩·구조 리터럴 등 짧은 컨텍스트는 인라인 허용 — §3.4
- [ ] **멤버 변수 = PascalCase**, **로컬 변수 = camelCase**, **private 메서드 = `_PascalCase` (언더스코어 접두)** — §2.3 / §2.4 / §2.5
- [ ] 클래스/파일 모두 `TPS` 프로젝트 접두 (UE prefix 뒤에): `ATPSCharacter`, `UTPSGameInstance`, `FTPS...` X — 단순히 `F` + 도메인명도 허용 (`FWeaponTableData`). §2.1
- [ ] 함수/멤버 위에 **한국어 한 줄 주석** (`// ~한다.` 종결) — §6.1
- [ ] 모든 `UPROPERTY`에 **`Category=` 명시** — §4.2
- [ ] 로그는 `LogGameplay` 카테고리 + `[TPS]` 프리픽스 — §7
- [ ] `#include` 순서: `CoreMinimal.h` → 엔진/플러그인 → 프로젝트(알파벳) → `*.generated.h` 마지막 — §5
- [ ] 새 UObject 멤버는 `TObjectPtr< T >` 사용 (raw 포인터 신규 도입 금지, 기존 raw는 유지) — §4.3
- [ ] 헤더 `pragma once` 사용 (UE 표준) — §5
- [ ] 오버라이드 함수 본문 첫 줄은 `Super::Func(...)` — §8.2
- [ ] 델리게이트 `AddUObject` 했으면 `EndPlay`에서 `RemoveAll(this)` 페어링 — §8.4

---

## 1. 디렉터리 구조

```
Source/TPS_Tutorial/
├── TPS_Tutorial.Build.cs            ← 모듈 정의
├── TPS_Tutorial.h / .cpp            ← 모듈 엔트리
├── AnimInstance/                    ← UTPSAnimInstance
├── Actors/
│   ├── TPSPickUpBase, TPSEquipBase, TPSEquipSniperRifle, TPSShotImpactField
│   └── Components/TPSDataComponent
├── Character/TPSCharacter
├── Consts/TPSConsts                 ← 프로젝트 전역 상수
├── Controller/TPSPlayerController
├── GameInstance/TPSGameInstance     ← 매니저 보관, Init/Shutdown
├── GAS/
│   ├── TPSAbilitySystemComponent, TPSAttributeSet, TPSGameplayTags
│   ├── Abilities/TPSGameplayAbility[, _Sprint, _Fire]
│   └── Effects/TPSGameplayEffect_Damage
├── Log/TPSLog                       ← LogGameplay 카테고리
├── Logic/
│   ├── ITPSInteractionActorInterface
│   └── Common/TPSTypes
├── Manager/TPSPlayerCameraManager, TPSDataManager, TPSUIManager
├── UI/TPSHUD
└── Util/TPSUtil, TPSUtilEngine, TPSUtilPath, TPSUtilWidget
```

**룰:**
- 새 클래스는 **기능 도메인 폴더**에 둔다. 타입 종류(`Components/` 등 UE 표준 제외)별이 아닌 도메인별 분류.
- 새 도메인이 필요하면 위와 같은 한 단어 PascalCase 폴더로 생성.
- Source 루트 직속에는 파일을 더 두지 말 것 (`TPS_Tutorial.{h,cpp}`, `*.Build.cs`만 유지).

---

## 2. 명명 규칙

### 2.1 타입/파일 prefix

| 종류 | UE prefix | 프로젝트 prefix | 예시 |
|---|---|---|---|
| `AActor` 파생 | `A` | `TPS` | `ATPSCharacter`, `ATPSPickUpBase` |
| `UObject` 파생 | `U` | `TPS` | `UTPSGameInstance`, `UTPSDataManager`, `UTPSHUD` |
| `USTRUCT` | `F` | (선택) | `FWeaponTableData`, `FStringTableData`, `FOnTPSCharacterDeath` |
| `UENUM` / enum class | `E` | (선택) | `ECharacterMoveDirection`, `ERotationType` |
| `UINTERFACE` / Interface | `U` / `I` | `TPS` | `UTPSInteractionActorInterface` + `ITPSInteractionActorInterface` |
| Native gameplay tag | — | `TAG_` (UE 매크로 규칙) | `TAG_Ability_Sprint`, `TAG_State_Dead` |

- 파일명 = 클래스명에서 UE prefix 제거. `ATPSCharacter` → `TPSCharacter.{h,cpp}`.
- 헤더는 항상 자기 클래스 한 개 + 동일 도메인 enum/struct만 포함.

### 2.2 모듈 export

```cpp
UCLASS()
class TPS_TUTORIAL_API UTPSGameInstance : public UGameInstance
```

모든 `UCLASS`는 `TPS_TUTORIAL_API` 부착. 다른 모듈에서 링크할 일이 없더라도 일관성 유지.

### 2.3 멤버 변수 — PascalCase

```cpp
UPROPERTY(...)
TObjectPtr< UTPSAbilitySystemComponent > AbilitySystemComponent;

UPROPERTY(...)
TObjectPtr< UInputAction > SprintAction;

UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Components" )
USpringArmComponent* SpringArmComp = nullptr;
```

- **언더스코어/Hungarian m_/this->. 사용 금지**.
- 컴포넌트 멤버는 접미사 `Comp` 권장 (`SpringArmComp`, `SpringArmChildComp`).
- `bool` 멤버/파라미터는 `b` 접두: `bOn`, `bIsXxx`.

### 2.4 로컬 변수 — camelCase

```cpp
UTPSGameInstance* gameInstance = UTPSGameInstance::GetGameInstance();
UTPSHUD* hudUI = Cast< UTPSHUD >( uiManager->FindWidget( UTPSHUD::StaticClass() ) );
TArray< USkeletalMeshComponent* > meshComponents;
FVector movementVector = ...;
if ( UEnhancedInputComponent* eic = Cast<UEnhancedInputComponent>( PlayerInputComponent ) )
```

- 임시·축약 변수도 camelCase. (`eic`, `ftrFireOff` 등)
- 함수 파라미터는 PascalCase (UE 엔진 컨벤션 우선). 단 내부 helper의 짧은 인자에 한해 camelCase 허용.

### 2.5 메서드 — 가시성에 따른 표기 분리 (⚠ 중요)

| 가시성 | 표기 | 예시 |
|---|---|---|
| `public` / `protected` | PascalCase | `OnSprintPressed`, `GetUIManager`, `HandleFireWeaponInteract` |
| **`private` helper** | **`_PascalCase` (언더스코어 접두)** | `_InitAbilitySystem`, `_HandleHealthChanged`, `_HandleOnDeath`, `_ToggleHUDUI`, `_LoadTable` |

> **룰:** 클래스 내부에서만 호출되는 도우미를 새로 만들 때는 반드시 `_` 접두를 붙이고 `private:` 섹션에 둔다. 외부에 노출되는 콜백/오버라이드/BP 호출은 접두 없이.

### 2.6 상수 / `TEXT`

```cpp
const FString TPSCameraCompName = TEXT( "TPSCamera" );
```

- 클래스 내부 상수는 `private:` 섹션의 `const FString`/`const FName` 멤버로 선언, PascalCase.
- 문자열 리터럴은 항상 `TEXT( "..." )`로 감쌀 것 (공백 안쪽 일관 적용).
- 빈 문자열도 명시: `FString StringValue = TEXT( "" );`

---

## 3. 포매팅

### 3.1 괄호 내부 공백 (강제)

```cpp
if ( !Object ) return nullptr;
Func( a, b, c );
UE_LOG( LogGameplay, Log, TEXT( "msg" ) );
```

- `if`/`for`/`while`/`switch`/함수호출/매크로 모두 적용.
- 단, `Cast<UEnhancedInputComponent>( ... )` 같이 angle bracket이 직접 붙는 경우 외부 괄호엔 공백 유지.

### 3.2 Angle bracket 내부 공백

```cpp
TArray< USkeletalMeshComponent* > meshComponents;
TSubclassOf< class AActor > EquipWeapon;
template< typename RowT >
const RowT* FindRow( ... ) const;
Cast< UTPSHUD >( ptr );
```

- 템플릿 선언/사용, `TArray`, `TMap`, `TSubclassOf`, `TObjectPtr`, `Cast<>` 모두 내부 공백.
- `<class T>`처럼 공백 없는 형도 일부 존재하지만 **신규 코드는 공백 있는 형으로 통일**.

### 3.3 들여쓰기

- **탭(Tab)** 사용. 스페이스 들여쓰기 금지.
- 탭 너비 = 4 가정 (Rider/VS 기본).
- 줄 끝 trailing whitespace 금지. CRLF 유지 (Windows 프로젝트).

### 3.4 중괄호 / 줄바꿈

```cpp
void ATPSCharacter::SetupPlayerInputComponent( UInputComponent* PlayerInputComponent )
{
    Super::SetupPlayerInputComponent( PlayerInputComponent );

    if ( UEnhancedInputComponent* eic = Cast<UEnhancedInputComponent>( PlayerInputComponent ) )
    {
        if ( SprintAction )
        {
            eic->BindAction( SprintAction, ETriggerEvent::Started,   this, &ATPSCharacter::OnSprintPressed  );
            eic->BindAction( SprintAction, ETriggerEvent::Completed, this, &ATPSCharacter::OnSprintReleased );
        }
    }
}
```

- 함수/클래스/제어문 본문 `{`는 **새 줄** (Allman).
- 한 줄 `if ( !X ) return;` 가드는 중괄호 생략 허용.
- 람다·짧은 익명 블록은 한 줄 인라인 허용.
- 동일 패턴의 호출이 반복될 때 **인자를 공백으로 수직 정렬**해도 좋다 (위 `BindAction` 예시처럼).

### 3.5 기본값 초기화

선언부에서 초기화. 별도 생성자 라인 금지 (단, `CreateDefaultSubobject` 같은 UE 패턴 제외).

```cpp
USpringArmComponent* SpringArmComp = nullptr;
FString StringValue = TEXT( "" );
float SprintSpeed = 1000.0f;
```

---

## 4. UE 매크로 사용

### 4.1 `UCLASS` / `USTRUCT` / `UENUM` / `UINTERFACE`

```cpp
UCLASS()
class TPS_TUTORIAL_API UTPSDataManager : public UObject
{
    GENERATED_BODY()
    ...
};

USTRUCT( BlueprintType )
struct FWeaponTableData : public FTableRowBase
{
    GENERATED_BODY()
    ...
};

UENUM( BlueprintType )
enum class ECharacterMoveDirection : uint8
{
    Forward,
    Backward,
    Left,
    Right,
    Max
};
```

- `enum class`는 `: uint8` 명시, BP 노출 시 `BlueprintType`.
- enum 마지막 멤버는 **`Max` 센티넬** 추가.
- 데이터 테이블 row 구조체는 `FTableRowBase` 파생 + `BlueprintType`.

### 4.2 `UPROPERTY` — 항상 `Category` 지정

```cpp
UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Components" )
USpringArmComponent* SpringArmComp = nullptr;

UPROPERTY( VisibleAnywhere, BlueprintReadOnly, Category = "GAS" )
TObjectPtr< UTPSAbilitySystemComponent > AbilitySystemComponent;

UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "GAS" )
TArray< TSubclassOf< UTPSGameplayAbility > > DefaultAbilities;
```

표준 카테고리:
- `Components` — 컴포넌트 서브오브젝트
- `GAS` — Ability System 관련
- `Input` — `UInputAction` 등
- `"Weapon Data"`, `"String Data"` — Data Table 필드 (공백 포함, 큰따옴표)

GC 보호용 멤버 (BP/Editor 노출 불필요)는 옵션 없이:
```cpp
UPROPERTY()
TObjectPtr< UTPSAttributeSet > AttributeSet;
```

### 4.3 포인터 타입

- **신규 UObject 멤버: `TObjectPtr< T >`** (UE 5.x 표준).
- 기존 raw 포인터는 손대지 않는 한 유지. **신규 코드에서 raw 도입 금지**.
- 비-UObject (컴포넌트 raw 포인터 등 일부 레거시 `USpringArmComponent*`)는 디폴트 `= nullptr` 강제.
- Weak ref: `TWeakObjectPtr< T >` (예: 람다 캡처 시).

### 4.4 `UFUNCTION`

```cpp
UFUNCTION( BlueprintCallable, Category = "Interaction Control" )
virtual bool HandleFireWeaponInteract() override;
```

- BP 노출 함수만 `UFUNCTION` 부착. C++ 전용 helper에는 붙이지 않는다.
- BP 콜러블 함수는 `Category` 필수.

### 4.5 델리게이트

```cpp
// 사망 시 브로드캐스트되는 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FOnTPSCharacterDeath );

UPROPERTY( BlueprintAssignable, Category = "GAS" )
FOnTPSCharacterDeath OnDeath;
```

- BP에서 바인딩 가능해야 하면 `DYNAMIC` + `BlueprintAssignable`.
- C++ 전용은 `DECLARE_MULTICAST_DELEGATE_TwoParams` 등 비-dynamic.
- 시그니처 typedef는 `F` 접두 + `OnXxx` 명명.

---

## 5. `#include` 정책

### 5.1 헤더 (.h)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Consts/TPSConsts.h"
#include "GameFramework/Character.h"
#include "GameplayEffectTypes.h"
#include "Logic/ITPSInteractionActorInterface.h"
#include "Logic/Common/TPSTypes.h"
#include "TPSCharacter.generated.h"
```

순서:
1. `#pragma once`
2. **`CoreMinimal.h` 최상단**
3. 엔진/플러그인 헤더 + 프로젝트 헤더를 **알파벳 순으로 한 블록**에 정렬 (실제 코드가 이렇게 섞어 두고 있음)
4. **`*.generated.h`는 반드시 마지막**

### 5.2 소스 (.cpp)

1. **자기 짝 헤더 최상단**: `#include "TPSCharacter.h"`
2. 한 줄 띄우거나 곧바로 알파벳 순으로 나머지 헤더

### 5.3 전방 선언

헤더에서 포인터/참조로만 쓰는 타입은 `class Foo;` 전방선언 + 인자 위치에서 `class AActor*` 인라인 선언도 허용:

```cpp
TSubclassOf< class AActor > EquipWeapon;
virtual void SetupPlayerInputComponent( class UInputComponent* PlayerInputComponent ) override;
```

---

## 6. 주석 & 문서화

### 6.1 한국어 주석 종결 규칙

함수·멤버 선언 바로 위에 **한국어 한 줄 주석** 필수. 동작 설명은 **종결어미 `~한다.`** 통일.

```cpp
// 데이터 테이블에서 Row를 찾아 반환한다.
template< typename RowT >
const RowT* FindRow( const FName& TableName, const FName& RowName, ... ) const;

// UI 관리자 객체를 얻는다.
UTPSUIManager* GetUIManager() const;

// HUD UI를 토글한다.
void _ToggleHUDUI( const bool bOn );
```

- 의문문/명사형 종결(`~함`) 금지. 항상 평서·서술 `한다.` 끝맺음.
- 명백한 getter (`GetX()`)도 짧게 한 줄 붙이는 것이 본 프로젝트 표준.

### 6.2 멤버 인라인 주석

```cpp
USpringArmComponent* SpringArmComp = nullptr; // 스프링암 컴포넌트
TMap< UClass*, UUserWidget* > WidgetMap;      // UI 맵
```

타입과 변수명만으로 의도가 분명하지 않으면 우측에 짧은 한국어 주석.

### 6.3 섹션 디바이더

`.h` 파일 내부에서 선언/구현을 함께 둘 때 ASCII 박스 디바이더 사용:

```cpp
//////////////////
/// Implements ///
//////////////////
namespace TPSUtil
{
    template <class T>
    T* GetValueForObjProp( const UObject* Object )
    { ... }
}
```

### 6.4 `TODO` / 후속 작업

- 헤더 주석에 명시 (예: `// TODO: HandleFireWeaponInteract 본체 마이그레이션`).
- 별도 이슈/문서로 옮긴 뒤에도 코드 주석은 남겨 둔다.

---

## 7. 로깅

### 7.1 카테고리

`Log/TPSLog.h`:
```cpp
DECLARE_LOG_CATEGORY_EXTERN( LogGameplay, Log, All );
```
`Log/TPSLog.cpp`:
```cpp
DEFINE_LOG_CATEGORY( LogGameplay );
```

### 7.2 사용

**모든 게임플레이 로그는 `LogGameplay` + `[TPS]` 프리픽스**.

```cpp
UE_LOG( LogGameplay, Log,     TEXT( "[TPS] GAS Initialized — Health=%.1f / MaxHealth=%.1f" ), h, mh );
UE_LOG( LogGameplay, Warning, TEXT( "[TPS] UI Manager invalid" ) );
UE_LOG( LogGameplay, Log,     TEXT( "[TPS] OnDeath Broadcast" ) );
```

- 다른 카테고리 신규 도입 금지 (필요하면 본 문서 갱신).
- `UE_LOG` 매크로 호출도 §3.1 공백 규칙 적용.
- 포맷 문자열은 영어, 인게임 상태 단어는 PascalCase.

---

## 8. 클래스 작성 패턴

### 8.1 헤더 멤버 순서

```cpp
UCLASS()
class TPS_TUTORIAL_API ATPSXxx : public AParent, public IXxxInterface
{
    GENERATED_BODY()

public:
    // 생성자
    ATPSXxx();

    // AActor 오버라이드
    virtual void BeginPlay() override;
    virtual void Tick( float DeltaTime ) override;
    virtual void EndPlay( const EEndPlayReason::Type Reason ) override;
    virtual void SetupPlayerInputComponent( UInputComponent* PIC ) override;

    // 인터페이스 구현 / public API
    ...

public:
    // BP 노출 UPROPERTY (컴포넌트, 데이터, 델리게이트)
    UPROPERTY(...) ...;

protected:
    // 보호 API / 오버라이드 가능 훅
    ...

private:
    // private helpers (_PascalCase)
    void _InitAbilitySystem();
    void _HandleHealthChanged( float NewValue, float OldValue );

    // 내부 enum/state
    enum class ERotationType { Pitch, Roll, Yaw };

    // 내부 상수
    const FString TPSCameraCompName = TEXT( "TPSCamera" );

    // 내부 상태 멤버 (UPROPERTY 또는 raw)
    ...
};
```

### 8.2 생성자 / `BeginPlay`

- 생성자: 컴포넌트 `CreateDefaultSubobject`, 기본값 세팅, `PrimaryActorTick.bCanEverTick` 설정.
- `BeginPlay`는 첫 줄에 `Super::BeginPlay();`.
- **초기화 로직은 `_InitXxx()` private 헬퍼로 분리**해 `BeginPlay`에서 호출. (예: `_InitAbilitySystem()`)

### 8.3 매니저 패턴

- `UTPSGameInstance`가 `UTPSDataManager`, `UTPSUIManager` 등 매니저 UObject를 보관.
- 접근: `UTPSGameInstance::GetGameInstance()->GetXxxManager()` — GameInstance에 `GetGameInstance()` static helper, 매니저별 `GetXxxManager() const` getter.
- 매니저 객체에 `nullptr` 가드 + `UE_LOG Warning` 출력 후 반환.

### 8.4 델리게이트 라이프사이클

```cpp
// BeginPlay
AttributeSet->OnHealthChanged.AddUObject( this, &ATPSCharacter::_HandleHealthChanged );

// EndPlay
AttributeSet->OnHealthChanged.RemoveAll( this );
AttributeSet->OnStaminaChanged.RemoveAll( this );
Super::EndPlay( ... );
```

- `AddUObject` ↔ `RemoveAll(this)` **반드시 페어링**.
- `AddRaw` 신규 도입 금지.

### 8.5 인터페이스 패턴

```cpp
UINTERFACE( MinimalAPI )
class UTPSInteractionActorInterface : public UInterface
{
    GENERATED_BODY()
};

class ITPSInteractionActorInterface
{
    GENERATED_BODY()
public:
    virtual bool HandleFireWeaponInteract() { return false; }
};

UINTERFACE( MinimalAPI )
class UTPSPickUpInteractionActorInterface : public UTPSInteractionActorInterface
{ GENERATED_BODY() };

class ITPSPickUpInteractionActorInterface : public ITPSInteractionActorInterface
{ GENERATED_BODY() };
```

- 인터페이스 파생도 `U` + `I` 쌍 동시에 정의.
- 메서드는 PascalCase, `Handle...` 또는 `On...` 형식.

---

## 9. 모듈 의존성 (`TPS_Tutorial.Build.cs`)

현재 `PublicDependencyModuleNames`:
```
Core, CoreUObject, Engine, InputCore, EnhancedInput,
FieldSystemEngine, Chaos, ChaosSolverEngine, GeometryCollectionEngine,
GameplayAbilities, GameplayTags, GameplayTasks
```

**새 의존 추가 시 룰:**
- 알파벳 순 아닌 **도메인 그룹** 단위 유지 (코어 → 입력 → 시뮬레이션 → GAS).
- 플러그인 신규 도입 시 `.uproject` `Plugins` 배열 동기화 (참고: `GAS_Spec.md` §1.1).
- `PrivateDependencyModuleNames`는 현재 미사용. private 의존이 필요해지면 별도 블록으로 추가.

---

## 10. Blueprint / 에셋 명명 (참고)

C++가 BP 클래스를 가리키는 부분에서 추론된 규칙:
- BP 클래스 prefix `BP_` (예: `BP_TPSCharacter`, `BP_Sprint`).
- Input Action prefix `IA_` (`IA_Sprint`).
- BP를 로드할 때 경로 끝에 `_C` 자동 부착 (`UTPSUIManager::CreateAndAddViewport`).

C++ 측에서 BP 경로를 다룰 때는 `Util/TPSUtilPath` 사용.

---

## 11. GAS 관련 규칙 (요약)

자세한 스펙은 `Docs/GAS_Spec.md` 참고. 핵심만:
- 모든 어빌리티는 `UTPSGameplayAbility` (abstract) 상속.
- 네이티브 태그는 `TPSGameplayTags.{h,cpp}`에 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` / `UE_DEFINE_GAMEPLAY_TAG`로 선언, prefix `TAG_`.
- 데미지는 **반드시** `UTPSGameplayEffect_Damage::ApplyDamage(Target, Source, +X)` 헬퍼 경유. 직접 `SetByCaller` 호출 금지 (디버깅 외).
- ASC 편의 함수 `TryActivateAbilityByTag` / `CancelAbilityByTag` 사용. 직접 `TryActivateAbilitiesByTag` 호출은 ASC 클래스 내부 한정.

---

## 12. 안티 패턴 (절대 금지)

1. ❌ 멤버 변수에 `m_`/`this->` 접두 (UE 컨벤션 위반).
2. ❌ 새 `printf`/`std::cout`/`GLog` 직접 사용 (`UE_LOG` + `LogGameplay`만).
3. ❌ `dynamic_cast` (`Cast<>` 사용).
4. ❌ `new`/`delete` 직접 호출 (UObject는 `NewObject` / `CreateDefaultSubobject`).
5. ❌ `using namespace ...;` 헤더 노출 (소스 파일 내부 한정).
6. ❌ `UPROPERTY` 없이 `UObject*` 멤버 보관 (GC dangling).
7. ❌ `AddRaw`로 델리게이트 바인딩 (`AddUObject` 사용).
8. ❌ 빈 줄 끝 trailing whitespace.
9. ❌ `*.generated.h`를 헤더 중간에 배치.
10. ❌ public/protected에서 `_PascalCase` 이름 사용 (해당 prefix는 private 한정 신호).

---

## 13. 이 문서의 변경 절차

- 본 문서는 **실제 코드 ↔ 룰 sync** 상태를 전제로 한다.
- 새로운 패턴을 도입할 때는: ① 해당 디렉터리 한 곳에 적용해 컴파일 확인 → ② 본 문서에 룰 명시 → ③ 점진적 마이그레이션.
- 기존 룰과 충돌하는 코드를 발견하면, 기존 코드를 표준으로 보고 본 문서를 우선 수정한 뒤 일괄 정합화 (한 사례 수정 시 유사 패턴 전체 수정 원칙).
