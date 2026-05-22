# TPS Gameplay Ability System (GAS) 명세 및 구현 스펙

> **대상 브랜치:** master (working tree, uncommitted)
> **엔진:** UE 5.3
> **목적:** 단일 플레이어 TPS 튜토리얼 프로젝트에 GAS 골격 도입.
>          `Sprint` 어빌리티는 완전 동작, `Fire`는 스텁(향후 마이그레이션), `Health/Stamina` 어트리뷰트와 데미지 GE까지 포함.
> **검토 범위:** 본 문서를 좌우 분할로 띄워 두고, 아래 "검토 체크리스트"의 각 항목과 `Source/TPS_Tutorial/GAS/...` 경로의 실 구현을 1:1로 대조하면 됨.

---

## 1. 모듈 / 플러그인 의존성

### 1.1 `TPS_Tutorial.uproject`
- 플러그인 추가:
  - `GameplayAbilities` → Enabled

### 1.2 `Source/TPS_Tutorial/TPS_Tutorial.Build.cs`
- `PublicDependencyModuleNames`에 추가:
  - `GameplayAbilities`
  - `GameplayTags`
  - `GameplayTasks`

### 1.3 `Source/TPS_Tutorial/GameInstance/TPSGameInstance.cpp`
- `Init()`에서 `UAbilitySystemGlobals::Get().InitGlobalData()` 호출.
- 사유: `TargetData` 직렬화 등 일부 GAS 기능이 GlobalData 초기화에 의존.

---

## 2. 디렉터리 구조

```
Source/TPS_Tutorial/GAS/
├── TPSAbilitySystemComponent.h / .cpp   ← UASC 서브클래스 (얇은 래퍼)
├── TPSAttributeSet.h / .cpp              ← Health / MaxHealth / Stamina / MaxStamina
├── TPSGameplayTags.h / .cpp              ← 네이티브 태그 선언/정의
├── Abilities/
│   ├── TPSGameplayAbility.h / .cpp           ← 프로젝트 공용 어빌리티 베이스 (Abstract)
│   ├── TPSGameplayAbility_Sprint.h / .cpp    ← 완전 구현
│   └── TPSGameplayAbility_Fire.h / .cpp      ← 스텁 (TODO: HandleFireWeaponInteract 마이그레이션 진입점)
└── Effects/
    └── TPSGameplayEffect_Damage.h / .cpp     ← Instant 데미지 GE + ApplyDamage() 정적 헬퍼
```

---

## 3. 네이티브 게임플레이 태그 (`TPSGameplayTags`)

`UE_DECLARE_GAMEPLAY_TAG_EXTERN` / `UE_DEFINE_GAMEPLAY_TAG` 매크로 사용. 모듈 export 매크로(`TPS_TUTORIAL_API`)로 외부 모듈 참조 가능.

| 태그 | 카테고리 | 용도 |
|---|---|---|
| `Ability.Sprint` | Ability | Sprint 어빌리티 식별/활성화 키 |
| `Ability.Fire`   | Ability | Fire 어빌리티 식별/활성화 키 |
| `State.Dead`      | State | 사망 상태 — 어빌리티 활성화 차단(BlockedTags) |
| `State.Sprinting` | State | Sprint 활성 동안 캐릭터에 부여되는 상태(ActivationOwnedTags) |
| `Data.Damage`     | Data (SetByCaller) | 데미지 GE의 매그니튜드 키 |

---

## 4. AbilitySystemComponent — `UTPSAbilitySystemComponent`

- `UAbilitySystemComponent` 서브클래스, `BlueprintSpawnableComponent` 메타.
- 생성자에서 `ReplicationMode = EGameplayEffectReplicationMode::Minimal` 설정.
  - 단일 플레이어 전제. 멀티플레이어 진입 시 `Mixed`로 변경 권장 (헤더 주석에 명시).
- 편의 함수:
  - `bool TryActivateAbilityByTag(const FGameplayTag&)` — 태그 컨테이너 1개로 묶어서 `TryActivateAbilitiesByTag` 호출.
  - `void CancelAbilityByTag(const FGameplayTag&)` — `CancelAbilities(&container)` 호출.
- 두 함수 모두 `Tag.IsValid()` 검사 포함.

---

## 5. AttributeSet — `UTPSAttributeSet`

### 5.1 어트리뷰트
| 이름 | 디폴트 | UPROPERTY |
|---|---|---|
| `Health`     | 100 | `BlueprintReadOnly, Category="Vital"` |
| `MaxHealth`  | 100 | `BlueprintReadOnly, Category="Vital"` |
| `Stamina`    | 100 | `BlueprintReadOnly, Category="Vital"` |
| `MaxStamina` | 100 | `BlueprintReadOnly, Category="Vital"` |

각 어트리뷰트마다 `ATTRIBUTE_ACCESSORS` 매크로(헤더 자체 정의)로 `Get/Set/Init/Property Getter` 4종 자동 생성:
```cpp
GAMEPLAYATTRIBUTE_PROPERTY_GETTER + VALUE_GETTER + VALUE_SETTER + VALUE_INITTER
```

### 5.2 사전/사후 처리
- `PreAttributeChange(Attr, NewValue&)`
  - `Health` → `[0, MaxHealth]`로 클램프
  - `Stamina` → `[0, MaxStamina]`로 클램프
  - `MaxHealth` 변경 시 → 이전 비율 유지하며 `Health` 재계산
  - `MaxStamina` 변경 시 → 동일 패턴으로 `Stamina` 재계산
- `PostGameplayEffectExecute(Data)` — Instant GE 적용 직후
  - `Health/Stamina` 변경분에 대해 한 번 더 `Clamp` 후 `Set`
  - 이전값 = `newValue - Magnitude` 로 복원
  - 멀티캐스트 델리게이트 브로드캐스트 (캐릭터/HUD 구독 진입점)

### 5.3 변경 알림 델리게이트
```cpp
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttributeValueChanged, float NewValue, float OldValue);
FOnAttributeValueChanged OnHealthChanged;
FOnAttributeValueChanged OnStaminaChanged;
```
> Stamina 델리게이트는 선언만 되어 있고 현재 `PostGameplayEffectExecute`에서 브로드캐스트되며, 캐릭터에서는 구독만 해제(EndPlay)하고 별도 핸들러는 미연결. (HUD 갱신용 자리)

---

## 6. 어빌리티 베이스 — `UTPSGameplayAbility` (Abstract)

생성자 디폴트:
- `InstancingPolicy = InstancedPerActor`
- `NetExecutionPolicy = LocalPredicted` (단일 플레이어 전제)

> 모든 프로젝트 어빌리티는 이 클래스를 상속하도록 컨벤션을 강제.

---

## 7. Sprint 어빌리티 — `UTPSGameplayAbility_Sprint`

### 7.1 태그 / Block
- `AbilityTags`: `Ability.Sprint`
- `ActivationOwnedTags`: `State.Sprinting`
- `ActivationBlockedTags`: `State.Dead`

### 7.2 BP 노출 프로퍼티
| 이름 | 타입 | 디폴트 | 설명 |
|---|---|---|---|
| `StaminaDrainEffect` | `TSubclassOf<UGameplayEffect>` | none | Sprint 동안 적용되는 Duration GE (BP 지정 필요) |
| `SprintSpeed` | `float` | 1000.0 | 스프린트 시 `MaxWalkSpeed` |
| `CachedWalkSpeed` (non-UPROPERTY) | `float` | 600.0 | EndAbility에서 원복용 |
| `ActiveStaminaDrainHandle` | `FActiveGameplayEffectHandle` | invalid | 즉시 Remove를 위한 핸들 |

### 7.3 ActivateAbility 흐름
1. `CommitAbility` 실패 시 즉시 `EndAbility(cancelled=true)`
2. `ActorInfo->AvatarActor` → `ACharacter` 캐스팅, 실패 시 EndAbility
3. `GetCharacterMovement()` 검사
4. `CachedWalkSpeed = MoveComp->MaxWalkSpeed` 백업 후 `MaxWalkSpeed = SprintSpeed`
5. `StaminaDrainEffect` 지정 시:
   - `MakeEffectContext` → `AddSourceObject(this)`
   - `MakeOutgoingSpec(StaminaDrainEffect, GetAbilityLevel(), ctx)`
   - `ApplyGameplayEffectSpecToSelf` → `ActiveStaminaDrainHandle` 보관
6. `LogGameplay` 출력

### 7.4 EndAbility 흐름
1. `ActiveStaminaDrainHandle.IsValid()` → `RemoveActiveGameplayEffect`
2. 핸들 무효화 (`= FActiveGameplayEffectHandle()`)
3. 캐릭터 `MaxWalkSpeed = CachedWalkSpeed`로 원복
4. `Super::EndAbility` 호출

> **주의/검토 포인트**
> - 스태미나 0에서 자동 종료 정책은 미구현. (헤더 주석에 후속 PR 명시) — `Stamina <= 0` 시 자체 EndAbility 트리거를 어디에 둘지 결정 필요.
> - `MaxWalkSpeed` 캐시값(600)이 캐릭터 BP 디폴트와 다를 수 있음. 캐시는 ActivateAbility 시점 값으로 갱신되므로 정상이지만, **ActivateAbility 도중 다른 시스템이 MaxWalkSpeed를 변경하면 EndAbility에서 잘못된 값으로 원복** 위험. 후속 검토 대상.

---

## 8. Fire 어빌리티 — `UTPSGameplayAbility_Fire` (스텁)

- `AbilityTags`: `Ability.Fire`
- `ActivationBlockedTags`: `State.Dead`
- `ActivateAbility`: `CommitAbility` 후 로그 출력 → 즉시 `EndAbility`
- TODO 주석: `ATPSCharacter::HandleFireWeaponInteract` 본체를 이 어빌리티로 이동.

> **현 상태:** GAS 진입점만 마련된 빈 골격. 실제 트레이스/데미지 적용은 캐릭터 측에 그대로 남아 있음.

---

## 9. Damage GameplayEffect — `UTPSGameplayEffect_Damage`

### 9.1 GE 정의
- `DurationPolicy = Instant`
- 단일 Modifier:
  - `Attribute = UTPSAttributeSet::GetHealthAttribute()`
  - `ModifierOp = Additive`
  - `ModifierMagnitude = SetByCaller(TAG_Data_Damage)`

### 9.2 SetByCaller 컨벤션
- 호출자는 **음수 값**을 전달해야 Health가 감소.
- 양수 전달 시 회복으로 동작 → 위험. 헤더 주석에 명시.

### 9.3 정적 헬퍼 — `ApplyDamage(Target, Source, DamageAmount /*양수*/)`
- `Target` null 또는 `DamageAmount <= 0` → invalid handle 반환.
- `Source`가 null이면 `Target`을 `effectiveSource`로 사용.
- `MakeEffectContext` → `MakeOutgoingSpec(StaticClass(), 1.0f, ctx)`
- `SetSetByCallerMagnitude(TAG_Data_Damage, -DamageAmount)` (양→음 자동 변환)
- `ApplyGameplayEffectSpecToTarget(*spec, Target)`

> **권장:** 호출 측은 항상 이 헬퍼를 통해 데미지를 가하도록 강제. 직접 SetByCaller 사용은 디버깅 시에만.

---

## 10. 캐릭터 통합 — `ATPSCharacter`

### 10.1 헤더 변경
- `#include "AbilitySystemInterface.h"`, `#include "GameplayEffectTypes.h"` 추가.
- `IAbilitySystemInterface` 상속 (※ 본 명세 기준 `GetAbilitySystemComponent()` 오버라이드 구현은 cpp에 존재. 실제 클래스 선언부의 인터페이스 상속 라인은 별도 검토 필요 — diff 기준 인터페이스 헤더 include는 추가됨).
- 새 UPROPERTY:
  - `TObjectPtr<UTPSAbilitySystemComponent> AbilitySystemComponent` — `VisibleAnywhere, BlueprintReadOnly, Category="GAS"`
  - `TObjectPtr<UTPSAttributeSet> AttributeSet` — `UPROPERTY()` (GC 보호 only)
  - `TArray<TSubclassOf<UTPSGameplayAbility>> DefaultAbilities` — `EditAnywhere, BlueprintReadOnly, Category="GAS"`
  - `TArray<TSubclassOf<UGameplayEffect>> DefaultEffects` — 동일
  - `TObjectPtr<UInputAction> SprintAction` — `Category="Input"`
  - `FOnTPSCharacterDeath OnDeath` — `BlueprintAssignable, Category="GAS"`

### 10.2 생성자
- `AbilitySystemComponent = CreateDefaultSubobject<UTPSAbilitySystemComponent>(TEXT("AbilitySystemComponent"))`
- `AbilitySystemComponent->SetIsReplicated(true)`
- `AttributeSet = CreateDefaultSubobject<UTPSAttributeSet>(TEXT("AttributeSet"))`
- `GetAbilitySystemComponent()` 오버라이드 → `AbilitySystemComponent` 반환

### 10.3 BeginPlay → `_InitAbilitySystem()`
1. `AbilitySystemComponent || AttributeSet` null이면 early-return
2. `InitAbilityActorInfo(this, this)` (Owner = Avatar = self; 단일 플레이어)
3. `AttributeSet->OnHealthChanged.AddUObject(this, &ATPSCharacter::_HandleHealthChanged)`
4. `DefaultEffects` 순회 → 각 GE를 self에 적용 (MakeEffectContext + AddSourceObject + MakeOutgoingSpec + ApplyGameplayEffectSpecToSelf)
5. `DefaultAbilities` 순회 → `GiveAbility(FGameplayAbilitySpec(class, level=1, InputID=INDEX_NONE, this))`
6. `LogGameplay`로 초기 어트리뷰트 값 출력

### 10.4 EndPlay
- `AttributeSet->OnHealthChanged.RemoveAll(this)`
- `AttributeSet->OnStaminaChanged.RemoveAll(this)`
- `Super::EndPlay`

> **댕글링 방지:** `AddUObject` ↔ `RemoveAll(this)` 매칭. (raw pointer Add 금지)

### 10.5 입력 바인딩 (Enhanced Input)
`SetupPlayerInputComponent`에서:
```cpp
if (UEnhancedInputComponent* eic = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
    if (SprintAction) {
        eic->BindAction(SprintAction, ETriggerEvent::Started,   this, &ATPSCharacter::OnSprintPressed);
        eic->BindAction(SprintAction, ETriggerEvent::Completed, this, &ATPSCharacter::OnSprintReleased);
        eic->BindAction(SprintAction, ETriggerEvent::Canceled,  this, &ATPSCharacter::OnSprintReleased);
    }
}
```
- `OnSprintPressed`  → `ASC->TryActivateAbilityByTag(TAG_Ability_Sprint)`
- `OnSprintReleased` → `ASC->CancelAbilityByTag(TAG_Ability_Sprint)`

### 10.6 사망 처리
- `_HandleHealthChanged(NewValue, _)`:
  - `NewValue <= 0.0f` → `_HandleOnDeath()`
- `_HandleOnDeath()`:
  - `HasMatchingGameplayTag(State.Dead)` → 중복 처리 방지 early-return
  - `AddLooseGameplayTag(State.Dead)` (GE 없이 즉시 부여)
  - `CancelAllAbilities()` (스프린트 등 활성 어빌리티 즉시 종료)
  - `DisableInput(PlayerController)`
  - 메시 `Ragdoll` 콜리전 프로파일 + `SimulatePhysics(true)`
  - `SetLifeSpan(5.0f)` — 5초 뒤 자동 제거
  - `OnDeath.Broadcast()`

> **검토 포인트**
> - `OnDeath`는 `DECLARE_DYNAMIC_MULTICAST_DELEGATE`(BlueprintAssignable) 형태일 것 — 시그니처(`FOnTPSCharacterDeath`) 정의 위치 확인 필요.
> - 라그돌 시 카메라(스프링암/카메라컴포넌트)가 캐릭터와 함께 시뮬레이션될 가능성 점검.

---

## 11. BP / 에디터 작업 체크리스트

문서 검토 시 다음 항목이 BP 측에 갖춰져 있는지 확인:

- [ ] **InitAttributes GE** (Instant): `Health=100, MaxHealth=100, Stamina=100, MaxStamina=100` 세팅 → `BP_TPSCharacter.DefaultEffects[0]`에 등록
- [ ] **StaminaDrain GE** (Duration, infinite or N초): `Stamina` Additive `-X/sec` (Period 사용 또는 Modifier+Period 조합) → `BP_Sprint.StaminaDrainEffect`에 지정
- [ ] **BP_Sprint** (`UTPSGameplayAbility_Sprint` 자식): `SprintSpeed` 조정, `StaminaDrainEffect` 지정
- [ ] **BP_TPSCharacter**: `DefaultAbilities`에 `BP_Sprint` 추가, `DefaultEffects`에 `InitAttributes` 추가
- [ ] **IA_Sprint** (Enhanced Input): 키 매핑 후 `BP_TPSCharacter.SprintAction`에 지정
- [ ] **Damage GE 사용처:** 기존 데미지 경로를 `UTPSGameplayEffect_Damage::ApplyDamage(...)`로 일원화
- [ ] **GAS Globals(Init):** GameInstance `Init()` 한 번만 호출되는지 확인 (PIE 재진입 OK)

---

## 12. 알려진 한계 / 후속 PR 후보

1. **Fire 어빌리티 본체 부재** — 트레이스/데미지/탄약 소비/HUD 갱신을 어빌리티로 이관.
2. **스태미나 자동 종료 정책 없음** — `Stamina <= 0` 시 Sprint 자동 EndAbility + 회복 GE 적용.
3. **스태미나 회복 GE 미정의** — Sprint 종료 후 자연 회복 Duration/Periodic GE 추가.
4. **OnStaminaChanged 핸들러 미연결** — HUD 갱신용 진입점 결정 필요.
5. **Replication 정책** — 멀티 진입 시 ASC `ReplicationMode=Mixed`, 어빌리티 `NetExecutionPolicy` 재검토.
6. **MaxWalkSpeed 캐시 위험** — Sprint 활성 중 다른 시스템이 속도 변경 시 EndAbility에서 잘못된 값 복원 가능성.
7. **GameplayCue 미사용** — VFX/SFX 일원화 위해 후속에서 도입 권장.

---

## 13. 호출/의존 다이어그램 (텍스트)

```
[Input: SprintAction]
   ├─ Started   → ATPSCharacter::OnSprintPressed   → ASC::TryActivateAbilityByTag(Ability.Sprint)
   └─ Completed → ATPSCharacter::OnSprintReleased  → ASC::CancelAbilityByTag(Ability.Sprint)

[ASC::TryActivate]
   └─ UTPSGameplayAbility_Sprint::ActivateAbility
        ├─ CommitAbility
        ├─ Cache MaxWalkSpeed → set SprintSpeed
        └─ Apply StaminaDrainEffect (Duration) → ActiveStaminaDrainHandle

[Damage path]
   └─ UTPSGameplayEffect_Damage::ApplyDamage(Target, Source, +X)
        └─ SetByCaller(Data.Damage, -X) → GE Instant → AttributeSet.Health 감소
             └─ AttributeSet::PostGameplayEffectExecute
                  └─ OnHealthChanged.Broadcast
                       └─ ATPSCharacter::_HandleHealthChanged
                            └─ (Health<=0) → _HandleOnDeath
                                 ├─ AddLooseGameplayTag(State.Dead)
                                 ├─ CancelAllAbilities
                                 └─ Ragdoll + SetLifeSpan(5) + OnDeath.Broadcast
```
