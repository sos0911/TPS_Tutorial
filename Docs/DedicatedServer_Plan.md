# 구현 계획: TPS_Tutorial GAS 프로젝트의 Dedicated Server 구조 전환 (2~4단계)

> **대상 브랜치:** `DedicatedServer` (기준 `master`)
> **엔진:** UE 5.3 / DX12 / Lumen / Virtual Shadow Maps
> **모듈:** `TPS_Tutorial` (Runtime, `TPS_TUTORIAL_API`)
> **본 문서 저장 위치:** `E:\Unreal_Project\TPS_Tutorial\Docs\DedicatedServer_Plan.md`
> **상태:** 1단계(서버 단독 빌드 Config) 완료(commit `81c1f0c`). 본 문서는 **2~4단계**의 완결 설계 + 전체 1~4단계 그림.
> **코드 예시 규약:** `Docs/CodingConventions.md` 준수 — 괄호/꺽쇠 안쪽 공백, `TPS` 접두, 멤버 PascalCase / 로컬 camelCase / private helper `_PascalCase`, 신규 UObject 멤버는 `TObjectPtr< T >`, 한국어 `~한다.` 한 줄 주석, include 순서(`CoreMinimal.h` → 엔진/플러그인 → 프로젝트 알파벳 → `*.generated.h`).

---

## 0. 요약 (Executive Summary)

현재 프로젝트는 **싱글플레이어 전제**로 작성되어 있다. GAS는 도입되어 있으나 모든 게임 권위 로직(발사, 데미지, 재장전, 탄약, 스프린트 속도, 사망)이 **로컬에서 직접 실행**되며, 어떤 액터·어트리뷰트도 실제로 복제되지 않는다(`bReplicates` 미설정, `GetLifetimeReplicatedProps` 부재, ASC는 있으나 `ReplicationMode = Minimal` + `InitAbilityActorInfo( this, this )` 단일 전제).

전환 목표는 **서버 권위(server-authoritative)** 모델이다. 단계는 의존성 순서로 다음과 같이 구성한다.

| 단계 | 제목 | 핵심 | 의존 |
|---|---|---|---|
| 1 (완료) | 서버 단독 빌드 가능 Config | `TPS_TutorialServer.Target.cs`, Config | — |
| 2 | 네트워크 골격 + 권위 게임 상태 복제 (재장전·탄약·무기 픽업/드랍) | Actor 복제, RepNotify, Server RPC, ASC Mixed 모드, GAS ActorInfo 멀티 분기 | 1 |
| 3 | GAS 권위화 (Fire/Sprint/Damage/Death) + 태그 복제 | 어빌리티 서버 실행/예측, 어트리뷰트 복제, 사망 Multicast | 2 |
| 4 | 로코모션/코스메틱 동기화 (`bIsSprinting` 등) + 폴리시 | AnimInstance가 복제된 GAS 태그에서 파생, 코스메틱 분리 | 3 |

핵심 의존 방향: **재장전·탄약(권위 게임상태) → GAS 권위화(태그·어트리뷰트 복제) → 로코모션 cosmetic 동기화**. 로코모션의 `bIsSprinting`은 `TAG_State_Sprinting` 복제가 선행돼야 하므로 가장 마지막에 온다(§3·§4 상세).

---

## 0.5 용어 설명 (Glossary)

> 본 문서에 등장하는 약어·개념을 한 곳에 모은 것이다. 처음 읽을 때 이 절을 먼저 훑고, 본문에서 막히는 용어가 나오면 돌아와서 확인하면 된다.

### 멀티플레이어 / 네트워크 기본

- **DS (Dedicated Server, 데디케이티드 서버):** 화면 출력 없이 **게임 진행만 담당하는 전용 서버**. 플레이어는 전부 클라이언트로 접속하고, 게임의 "진짜 상태"(누가 살아있는지, 탄약이 몇 발인지)는 이 서버가 가진다. 화면이 없으므로 HUD·카메라·이펙트 같은 UI/시각 코드를 그냥 실행하면 **크래시**가 난다(본문 2-A 가드의 이유).
- **클라이언트(Client):** 플레이어가 실제로 보는 게임 인스턴스. 입력을 서버로 보내고, 서버가 알려주는 상태를 받아 화면에 그린다.
- **권위(Authority) / 서버 권위(Server-Authoritative):** 어떤 데이터의 "정답"을 누가 들고 있느냐. 서버 권위 모델에서는 **서버가 정답**이고 클라는 따라간다. `HasAuthority()`가 true면 "지금 이 코드는 권위 쪽(서버)에서 돌고 있다"는 뜻. 치트 방지의 핵심 — 탄약 차감·데미지 같은 건 클라가 멋대로 못 바꾸게 서버만 처리한다.
- **복제(Replication, 리플리케이션):** 서버가 가진 값을 **클라로 자동 전송**해 동기화하는 UE 기능. 변수에 표시만 해두면 엔진이 알아서 네트워크로 보낸다.
- **RPC (Remote Procedure Call, 원격 프로시저 호출):** 한쪽에서 호출한 함수를 **다른 쪽에서 실행**시키는 것.
  - **Server RPC:** 클라 → 서버 방향. "내가 재장전하고 싶다"를 서버에 요청할 때 사용(본문 `ServerRequestReload`).
  - **Multicast RPC:** 서버 → 모든 클라 방향. "방금 총구 화염 이펙트 틀어라"처럼 모두에게 보여줄 시각 효과에 사용.
- **RepNotify / OnRep:** 복제된 값이 클라에 도착해 **바뀌는 순간 자동 호출되는 콜백 함수**(보통 `OnRep_XXX` 이름). 예: `CurrentBullet`이 서버에서 바뀌어 클라에 도착하면 `OnRep`에서 HUD 탄약 숫자를 갱신.
- **NetMode / `NM_DedicatedServer`:** 지금 이 코드가 어떤 환경에서 도는지 구분하는 값. `NM_DedicatedServer`면 "DS에서 실행 중" → 이펙트 등 시각 코드를 건너뛰는 가드에 쓴다.
- **PIE (Play In Editor):** 에디터에서 바로 게임 실행. 플레이어 수와 Net Mode를 설정해 **에디터 안에서 멀티플레이를 흉내내 테스트**할 수 있다(본문 검증 방법).
- **대역폭(Bandwidth):** 네트워크로 보내는 데이터 양. 불필요한 값까지 복제하면 대역폭 낭비 → 가능한 건 클라에서 직접 계산하도록 한다.

### GAS (Gameplay Ability System) 관련

- **GAS (Gameplay Ability System):** 언리얼의 **스킬/능력·스탯·효과 처리 표준 프레임워크**. 발사·스프린트·데미지 같은 걸 "어빌리티"와 "이펙트"로 구조화한다.
- **ASC (Ability System Component, 어빌리티 시스템 컴포넌트):** GAS의 심장. 캐릭터가 가진 어빌리티·태그·어트리뷰트를 보관·실행하는 컴포넌트. 본 프로젝트는 `TPSAbilitySystemComponent`.
- **어트리뷰트(Attribute) / AttributeSet:** 체력·스태미나 같은 **숫자 스탯**. `FGameplayAttributeData`로 표현되고, `UTPSAttributeSet`에 모여있다.
- **GE (GameplayEffect, 게임플레이 이펙트):** 어트리뷰트를 바꾸는 "효과". 예: 데미지 GE는 Health를 깎고, 스태미나 회복 GE는 Stamina를 채운다.
- **게임플레이 태그(Gameplay Tag):** `State.Sprinting` 같은 **계층형 라벨**. "지금 스프린트 중", "지금 죽음" 같은 상태를 태그로 표시한다(`TAG_State_Sprinting`, `TAG_State_Dead`).
- **Loose 태그(Loose Gameplay Tag):** ASC에 임시로 직접 붙이는 태그. **복제되지 않는다**는 함정이 있어, 멀티에서는 GE나 복제 변수로 대체해야 한다(본문 3-D).
- **복제 모드 Minimal / Mixed:** ASC가 GAS 정보를 얼마나 복제하는지 설정.
  - **Minimal:** 최소만 복제(싱글/AI용).
  - **Mixed:** 어트리뷰트/GE는 **소유한 본인 클라에만**, 태그·이펙트 큐는 **모두에게** 복제. 플레이어 캐릭터 멀티에 적합 → 본문에서 Minimal→Mixed로 변경.
- **`InitAbilityActorInfo`:** ASC에게 "이 ASC의 주인(Owner)과 아바타(Avatar)는 누구다"를 알려주는 초기화. 멀티에서는 서버/클라에서 호출 시점이 달라야 한다(본문 2-F).
- **NetExecutionPolicy (어빌리티 실행 정책):**
  - **LocalPredicted (로컬 예측):** 클라가 **먼저 실행해 즉시 반응**을 보여주고(예: 발사·스프린트), 서버가 검증해 틀리면 보정. 입력 지연 체감을 줄인다.
  - **ServerOnly / ServerInitiated:** 서버에서만 판정(데미지·사망 등 치트 민감 항목).
- **InstancingPolicy / InstancedPerActor:** 어빌리티 인스턴스를 어떻게 만들지. `InstancedPerActor`는 액터당 인스턴스 1개를 유지하는 일반적 설정.
- **`showdebug abilitysystem`:** 게임 중 콘솔에 입력하면 화면에 어트리뷰트·태그 상태를 띄워주는 **GAS 디버그 명령**.

### 언리얼 C++ / 액터 관련

- **`UPROPERTY`:** 변수에 붙이는 UE 매크로. 이걸 붙여야 **복제·GC 추적·에디터 노출** 등이 가능하다.
- **GC (Garbage Collection, 가비지 컬렉션):** UE가 안 쓰는 UObject를 자동 회수하는 메모리 관리. `UPROPERTY`로 잡힌 포인터는 GC가 "살아있다"고 인식해 회수하지 않는다.
- **`TObjectPtr< T >`:** 최신 UE에서 권장하는 UObject 포인터 타입(과거 raw 포인터 `T*` 대체). 신규 멤버에만 쓰고 기존 raw는 그대로 둔다(컨벤션 §4.3).
- **`DOREPLIFETIME` / `COND_None` / `REPNOTIFY_Always`:** `GetLifetimeReplicatedProps`에서 "이 변수를 복제 목록에 등록"하는 매크로와 옵션. `COND_None`=조건 없이 항상 복제, `REPNOTIFY_Always`=값이 같아도 OnRep을 항상 호출.
- **`HasAuthority()` / `IsLocallyControlled()` / `IsLocalController()`:** 코드가 도는 위치를 판별하는 함수. 각각 "서버인가 / 이 캐릭터를 내가 조종하나 / 이 컨트롤러가 내 로컬 것인가". 권위·예측·시각 코드를 분기하는 데 쓴다.
- **`PossessedBy`:** 컨트롤러가 Pawn(캐릭터)을 **빙의(소유)** 할 때 서버에서 호출되는 함수. 멀티 GAS 초기화의 서버 측 진입점.
- **Pawn / Character / PlayerState / Controller:**
  - **Pawn / Character:** 월드에 존재하는 **조종 가능한 몸체**(여기선 `TPSCharacter`).
  - **Controller:** Pawn을 조종하는 "두뇌"(플레이어 입력 또는 AI).
  - **PlayerState:** 죽어서 캐릭터가 사라져도 유지되는 **플레이어 영속 데이터**(점수 등). ASC를 여기 둘지 Pawn에 둘지가 본문 결정사항 #1.
- **GameMode / GameState:** 게임 규칙(스폰·승패)을 담는 클래스. GameMode는 **서버에만** 존재.
- **attach(부착):** 무기를 캐릭터 손 소켓에 붙이는 것. 부모가 복제되면 자식도 따라가지만, 소켓 미스매치 시 무기가 엉뚱한 위치에 보일 수 있다(본문 2-B 주의).
- **시뮬레이트 프록시(Simulated Proxy) / 원격 프록시:** 내 화면에 보이는 **다른 플레이어의 캐릭터**. 입력 권한이 없고 서버가 보내주는 값으로 움직인다. "원격에서 `bIsSprinting`이 맞아야 한다"가 바로 이 프록시 얘기.
- **몽타주(Montage):** 발사·재장전 같은 **단발성 애니메이션 클립**. 멀티에서는 다른 클라에도 보이게 동기화가 필요.
- **라그돌(Ragdoll):** 사망 시 캐릭터를 **물리 인형처럼 흐물흐물 쓰러뜨리는** 효과. 순수 시각 효과라 클라에서만 처리(DS에서 물리 켜면 안 됨).
- **라인트레이스(Line Trace):** 총구에서 직선을 쏴 **무엇에 맞았는지 검사**하는 충돌 질의(히트스캔 사격의 핵심).

### 기타

- **Chaos / FieldSystem / `AFieldSystemActor`:** 언리얼의 물리·파괴 시스템(Chaos)과 그 "힘의 장(field)"을 만드는 액터. 본 프로젝트의 탄착 효과(`TPSShotImpactField`)가 이것 — 시각 효과라 복제하지 않는다.
- **SceneCapture / `SceneCaptureComponent2D`:** 별도 카메라로 장면을 **렌더 텍스처에 캡처**하는 컴포넌트(스나이퍼 스코프 화면 구현). GPU 비용이 커서 줌 중인 본인 클라에서만 켠다.
- **TDR (Timeout Detection and Recovery):** GPU가 너무 오래 멈추면 드라이버가 강제 리셋하는 윈도우 기능. 불필요한 SceneCapture가 서버/원격에서 돌면 유발될 수 있어 경계한다.
- **Iris:** UE5의 **차세대 리플리케이션 시스템**(5.3 기준 실험적). 본 계획은 기본 리플리케이션을 가정(결정사항 #7).

---

## 1. 현황 분석 (Investigation Findings)

실제 코드를 읽어 확인한 사실만 기재한다.

### 1.1 네트워크/권위 현재 상태 표

| 요소 | 위치 | 현재 복제 여부 | 권위 위치 | 비고 |
|---|---|---|---|---|
| 위치/속도/회전 | `ACharacter` 기본 `UCharacterMovementComponent` | **복제됨 (엔진 기본)** | 서버(이동 예측+보정) | 거의 "공짜". `MaxWalkSpeed` 자체는 복제 안 됨(아래 참고) |
| ASC 서브오브젝트 | `TPSCharacter.cpp:40-41` `CreateDefaultSubobject` + `SetIsReplicated( true )` | **복제됨** | — | 단 `ReplicationMode = Minimal` (`TPSAbilitySystemComponent.cpp:12`) |
| ASC ActorInfo 초기화 | `TPSCharacter.cpp:312` `InitAbilityActorInfo( this, this )` | — | **클라/서버 양쪽 동일 호출(BeginPlay)** | 멀티 분기 없음. 서버/클라 모두에서 BeginPlay 시 호출됨 → 멀티에서 부적절 |
| 어트리뷰트 Health/MaxHealth/Stamina/MaxStamina | `TPSAttributeSet.h:42-58` | **복제 안 됨** | 로컬 | `GetLifetimeReplicatedProps` 없음, `ReplicatedUsing`/`REPNOTIFY` 없음 |
| 어트리뷰트 변경 델리게이트 | `OnHealthChanged`/`OnStaminaChanged` (`TPSAttributeSet.h:61-62`), `PostGameplayEffectExecute`에서 Broadcast (`.cpp:53-80`) | 로컬 브로드캐스트 | 로컬 | 현재 GE가 로컬에서만 실행되므로 로컬에서만 호출 |
| `TAG_State_Sprinting` | `TPSGameplayAbility_Sprint.cpp:21` `ActivationOwnedTags` | ASC 복제에 묶여있으나 Minimal+단일ActorInfo로 사실상 로컬 | 로컬 | AnimInstance가 이 태그를 읽음 |
| 스프린트 속도 변경 | `TPSGameplayAbility_Sprint.cpp:77-78` `moveComp->MaxWalkSpeed = SprintSpeed` | 복제 안 됨 | 로컬 | `MaxWalkSpeed`는 RepProp 아님 — 서버에서 변경해야 이동 예측에 반영됨 |
| 재장전 상태 `IsReloading` | `TPSCharacter.h:135` private bool | **복제 안 됨** | 로컬 | 클라 로컬에만 존재 |
| 재장전 타이머 `ReloadTimerHandle` | `TPSCharacter.h:136` | **복제 안 됨** | 로컬 | `GetWorldTimerManager().SetTimer(...)` (`.cpp:855`) |
| 탄약 `CurrentBullet` | `TPSCharacter.h:134` private int32 | **복제 안 됨** | 로컬 | HUD에만 반영 |
| 발사 로직 `HandleFireWeaponInteract` | `TPSCharacter.cpp:681-827` | 로컬 실행 | 로컬 | 라인트레이스, 임팩트 액터 스폰, 몽타주, 반동 모두 로컬 |
| 무기 픽업/장착 | `TPSCharacter.cpp:127-211` `HandlePickUpWeaponInteract` + `OnBeginOverlap`(`.cpp:430`) | 로컬 SpawnActor + Attach | 로컬 | `CurrentWeapon` raw/약참조(`TPSActorPtr`), 복제 안 됨 |
| 무기 드랍 | `TPSCharacter.cpp:545-593` | 로컬 SpawnActor + Destroy | 로컬 | — |
| 장비 액터 `ATPSEquipBase` | `.h`/`.cpp:10-13` | **복제 안 됨** (`bReplicates` 미설정) | — | 컴포넌트 raw 포인터, `SceneCapture`(스나이퍼) 포함 |
| 픽업 액터 `ATPSPickUpBase` | `.cpp:8-11` | **복제 안 됨** | — | `HandlePickUpWeaponInteract`가 `Destroy()` 직접 호출(`.cpp:27`) |
| 임팩트 필드 `ATPSShotImpactField` (Chaos `AFieldSystemActor`) | `.h:13` | **복제 안 됨** | — | 발사 시 로컬 스폰 + 0.1초 후 로컬 Destroy(`TPSCharacter.cpp:779-790`) |
| 사망 처리 `_HandleOnDeath` | `TPSCharacter.cpp:391-427` | 로컬 (라그돌+입력차단+`SetLifeSpan`) | 로컬 | `AddLooseGameplayTag( TAG_State_Dead )`, `DisableInput`, `SetSimulatePhysics` |
| HUD 생성 | `TPSPlayerController.cpp:19-22` `BeginPlay`에서 무조건 생성 | — | 로컬 | LocalController 가드는 없으나 **`if ( !hud ) return;` 널체크가 있어 DS에서도 크래시는 안 남**. 불필요 작업/패키징 빌드 견고성 차원에서 가드 권장(2-A) |
| HUD 접근 | `TPSCharacter` `_GetHUDUI()` (`.cpp:243`) → `UTPSGameInstance::GetGameInstance()` 싱글턴 | — | 로컬 | 어트리뷰트 핸들러에서 직접 호출 → DS에서 가드 필요 |
| 입력 바인딩 | `SetupPlayerInputComponent` **전체 주석 처리**(`TPSCharacter.cpp:271-290`); Sprint/Reload 콜백 존재하나 미바인딩 | — | — | BP에서 바인딩 중으로 추정. 멀티 전환 시 입력→RPC 경로 재정의 필요 |

### 1.2 빌드/타깃 현황

- `TPS_Tutorial.Build.cs`: `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `Chaos`/`FieldSystemEngine`/`GeometryCollectionEngine` 등 이미 포함. **네트워킹용 추가 모듈 불필요**(복제는 Engine 기본 제공). 향후 OnlineSubsystem은 주석으로 비활성(이번 범위 외).
- `TPS_TutorialServer.Target.cs`: `Type = TargetType.Server` 존재(1단계 산출물). `Game`/`Editor` 타깃과 동일하게 `TPS_Tutorial` 모듈만 링크.
- `DefaultEngine.ini`: 네트워크 관련 키(`NetMode`, `bReplicates`, `ReplicationDriver`, RPC 등) **검색 결과 없음** → 1단계 Config 변경은 맵/모드/빌드 설정 쪽이었을 가능성. 본 계획은 `DefaultEngine.ini`의 `[/Script/Engine.GameNetworkManager]` 등은 기본값 가정.

### 1.3 결론 (현황 한 줄 요약)

> **이동(Transform)만 엔진 기본으로 복제되고, 그 외 모든 게임 권위 상태(탄약/재장전/무기 소유/체력/스태미나/스프린트 속도/사망)와 코스메틱(`bIsSprinting`, 임팩트, 몽타주)은 로컬 전용이다.** ASC는 복제 플래그만 켜져 있을 뿐 `Minimal` 모드 + 단일 ActorInfo로 멀티 미대응.

---

## 1.5 테스트 충실도 전략 (L1~L4)

> **엔진 소스 빌드는 필요 없다.** 런처(바이너리) 엔진 + 기존 `TPS_TutorialServer.Target.cs`만으로 서버 타깃 빌드·실행·패키징이 가능하다(C++ 프로젝트 + Server 타깃 조건 충족). 엔진 소스 빌드는 엔진 코드 자체를 고칠 때만 필요하며, 본 작업 범위에는 해당 없음.

**핵심 원칙:** PIE 단일 프로세스는 에디터 안이라 렌더/UMG/Slate 모듈이 전부 살아있어 **진짜 패키징 DS를 충실히 재현하지 못한다.** 따라서 평소엔 빠른 PIE로 개발하되, **단계가 끝날 때마다 충실도를 한 단계 올려** 검증해서 문제가 막판에 한꺼번에 터지는 것을 막는다.

| 레벨 | 방법 | 잡아내는 문제 | 빈도 |
|---|---|---|---|
| **L1** | PIE, Players=2, Net Mode `Play As Client`, **Run Under One Process 체크** | 복제 로직 대부분, RPC, RepNotify, 권위 분기 | 매 반복(평상시) |
| **L2** | 위와 동일하되 **Run Under One Process 체크 해제** | 별도 프로세스/별도 메모리에서만 드러나는 문제, 실제 서버-클라 분리 | 각 단계 마무리 |
| **L3** | IDE 빌드 구성을 `TPS_TutorialServer`로 빌드 → 서버 실행파일(`-log`) + standalone 클라 접속 | 쿡/모듈/에셋 누락, 클라 모듈 부재로 갈리는 DS 전용 경로 | 단계 마일스톤 |
| **L4** | 풀 패키징 서버 빌드 | 최종 배포 검증 | 막판 1~2회 |

**권장 운영:**
- **평상시 개발·반복 = L1** (가장 빠름).
- **각 단계(2/3/4) 종료 시 = L2 또는 L3 1회** 회귀 검증 → 그 단계 변경분을 실제 서버 분리 환경에서 확인. 문제를 단계별로 격리.
- **L4는 막판**에만.
- 각 단계의 "검증 방법" 항목은 기본 L1 기준이며, 단계 종료 시 L2/L3로 1회 승격해 재확인한다.

---

## 2. 단계별 계획

> 각 단계는 한 PR(여러 커밋) 단위. 커밋 메시지는 사용자 스타일(`제목  - 항목1  - 항목2`, Co-Authored 금지)을 따른다.

---

### 2단계 — 네트워크 골격 + 권위 게임 상태 복제 (무기/탄약/재장전)

#### 목표 / 범위
DS 안전 가드 + 액터 복제 기반 마련 + **재장전/탄약/무기 픽업·드랍**을 서버 권위로 전환. GAS 본격 권위화(3단계)의 토대.

#### 구체적 변경 대상

**(2-A) DS 안전 가드 (선행, 작은 커밋) — 크래시 방지 아님, 견고성·낭비 제거 목적**
- **전제 정정:** 현재 코드는 **이미 방어적으로 짜여 있어** DS에서도 하드 크래시는 나지 않는다(실측 확인). `TPSPlayerController.cpp:19-22`는 `UTPSHUD* hud = UTPSHUD::Create(); if ( !hud ) return; hud->Init();`로 결과 널체크가 있고, `_GetHUDUI()`(`.cpp:242-252`)도 `GameInstance`→`UIManager`→위젯 각 단계마다 null이면 `nullptr` 반환한다. DS에서 `Create()`가 (로컬 플레이어 부재로) null을 주더라도 역참조 지점이 없다. → **따라서 2-A는 "필수 크래시 가드"가 아니다.**
- **그럼에도 넣는 이유:** ① 서버에서 무의미한 클라 작업(위젯 생성 시도)을 안 하게 해 낭비/혼선 제거, ② `Create()`가 우연히 null을 주는 엔진 동작에 **의존하지 않도록** 의도를 명시(견고성), ③ **패키징된 서버 전용 빌드**에서는 UI 모듈/에셋이 쿡 단계에서 빠져 동작이 PIE와 달라질 수 있으므로 사전 차단.
- 변경: `Controller/TPSPlayerController.cpp:15-23` `BeginPlay`의 HUD 생성을 `if ( IsLocalController() )`로 감싸고, `Character/TPSCharacter.cpp`의 `_GetHUDUI()`(`.cpp:242`) 및 호출부(`_ToggleHUDUI`/`_RefreshWeaponHUD`/`_HandleStaminaChanged`) 진입부에 `if ( !IsLocallyControlled() ) return;` 추가. `UTPSGameInstance::GetGameInstance()` 싱글턴은 DS에도 존재하나 UIManager/HUD는 의미 없음.
- 우선순위: **권장(선행이면 좋음)**, 크래시 차단용 필수 아님. 함수 시그니처 변경 없음, 약 8~12 LOC.

**(2-B) 액터 복제 활성화**
- `Actors/TPSEquipBase.cpp` 생성자(`.cpp:10`): `bReplicates = true;` 추가. 컴포넌트 Transform이 부착(Attach) 기반이므로 `SetReplicateMovement( false )` 후 부모(캐릭터)에 attach 복제로 따라가게 함. 스나이퍼 `SceneCaptureComponent2D`/렌즈/스코프(`TPSEquipSniperRifle.h:19-22`)는 **코스메틱** — 복제 대상에서 제외, 클라에서만 활성(`SetSceneCaptureEnabled`는 줌과 함께 로컬 호출 유지).
- `Actors/TPSPickUpBase.cpp` 생성자: `bReplicates = true;`. `HandlePickUpWeaponInteract`의 `Destroy()`(`.cpp:27`)는 **서버에서만** 호출되도록 가드(`if ( !HasAuthority() ) return false;`). 오버랩 처리(`OnBeginOverlap`)도 서버에서만 픽업 확정.
- `Actors/TPSShotImpactField`(Chaos `AFieldSystemActor`): **복제하지 않는다.** Chaos 파괴/필드는 DS 복제가 불안정(외부 사례 다수)하고 본 프로젝트에선 순수 시각 효과. 3단계에서 발사가 서버 권위화되면 임팩트 스폰은 **Multicast(코스메틱)** 로 각 클라가 로컬 생성. DS 빌드에선 필드 시스템이 쿡되지 않을 수 있으므로 `if ( GetNetMode() != NM_DedicatedServer )` 가드.

**(2-C) 무기 소유권 복제**
- `TPSCharacter`: 현재 `CurrentWeapon`은 `TPSActorPtr`(약참조 추정) private 멤버(`.h:127`)라 **복제 불가**. 신규 복제 멤버 추가:
  ```cpp
  // 현재 장착 무기 액터 (서버 권위, 클라 동기화).
  UPROPERTY( ReplicatedUsing = OnRep_CurrentWeapon, VisibleAnywhere, Category = "State" )
  TObjectPtr< AActor > RepCurrentWeapon = nullptr;
  ```
  - 신규 UObject 멤버이므로 `TObjectPtr< T >` 사용(컨벤션 §4.3). 기존 `CurrentWeapon`(raw/약참조)은 **로컬 캐시로 유지**하고, `OnRep_CurrentWeapon`에서 동기화(기존 raw 일괄 마이그레이션 금지 §4.3).
- `GetLifetimeReplicatedProps` 신규 구현(`TPSCharacter.cpp`): `RepCurrentWeapon`, `CurrentWeaponType`, `CurrentBullet`, `IsReloading` 등록.
- `CurrentBullet`(`.h:134`)/`IsReloading`(`.h:135`)을 `UPROPERTY( ReplicatedUsing=... )` 또는 단순 `Replicated`로 승격. `CurrentBullet`은 RepNotify로 클라 HUD 갱신(`_RefreshWeaponHUD`).

**(2-D) 픽업/드랍 서버 권위화**
- `OnBeginOverlap`(`.cpp:430`)→`HandlePickUpWeaponInteract`(`.cpp:127`): 액터 스폰(`SpawnActor`)과 attach, `CurrentBullet = MagazineSize` 설정을 **서버에서만** 실행하도록 `HasAuthority()` 가드. 오버랩은 서버에서 권위 판정. `RepCurrentWeapon` 세팅 → 클라는 `OnRep_CurrentWeapon`에서 로컬 캐시/카메라/HUD 동기화.
- `Drop`(`.cpp:545`): 입력은 클라 → **Server RPC**(`UFUNCTION( Server, Reliable )` `ServerDrop()`)로 서버가 PickUp 스폰 + 무기 Destroy + 상태 초기화. 줌 카메라 원복 등 **카메라/HUD는 클라 로컬**(코스메틱).

**(2-E) 재장전 서버 권위화**
- `OnReload`(`.cpp:840`): 입력 콜백은 클라 로컬 → **Server RPC** `ServerRequestReload()`로 위임. 서버에서만 `IsReloading = true`, `GetWorldTimerManager().SetTimer( ReloadTimerHandle, ... _FinishReload, ReloadTime )`(`.cpp:855`) 구동.
- `_FinishReload`(`.cpp:861`): 서버에서 실행, `CurrentBullet = MagazineSize`(복제됨)·`IsReloading=false`(복제됨). 클라는 RepNotify로 HUD 갱신 + (4단계 연계) 재장전 몽타주 cosmetic.
- 타이머 핸들(`ReloadTimerHandle`)은 **서버에만 존재** — 클라에는 타이머가 돌지 않음. 무기 드랍 시 타이머 클리어(`.cpp:589`)도 서버 경로로.

#### 서버 권위 vs 클라 예측 처리
- **권위(서버):** 픽업/드랍 스폰·소멸, 무기 소유, 탄약 카운트, 재장전 타이머/완료.
- **복제(서버→클라):** `RepCurrentWeapon`, `CurrentBullet`, `IsReloading`, `CurrentWeaponType` (RepNotify로 HUD/카메라 동기화).
- **로컬(코스메틱):** HUD, 카메라 전환/줌, 스코프 SceneCapture.
- 이 단계에서는 **클라 예측 없음**(재장전/픽업은 약간의 지연 허용). Fire 예측은 3단계에서 GAS LocalPredicted로 처리.

#### 의존성
1단계(서버 빌드) 위에 올라간다. 2-A(가드)는 나머지의 선행. 3단계 GAS 권위화는 2-C(ASC ActorInfo 멀티 분기, 아래 2-F)에 의존.

**(2-F) ASC ActorInfo 멀티 분기 (3단계 진입 토대, 이 단계에서 준비)**
- `TPSCharacter.cpp:307-312` `_InitAbilitySystem`의 `InitAbilityActorInfo( this, this )`는 현재 BeginPlay에서 무조건 호출. 멀티에서는:
  - 서버: `PossessedBy()` 오버라이드에서 `InitAbilityActorInfo`.
  - 클라: `OnRep_PlayerState()`/`OnRep_Controller` 또는 (ASC가 Pawn에 있으므로) `OnRep_Owner`/`PossessedBy` 클라 미러 시점에서 재초기화.
- ASC가 **Pawn(캐릭터)** 에 있으므로 PlayerState 이전 불필요(튜토리얼 규모 적절). 단 Owner를 Controller로 설정해야 Mixed 모드가 정상 동작(아래 3단계). 본 단계에서는 분기 골격만 추가하고, 실제 어빌리티 권위화는 3단계.
- `TPSAbilitySystemComponent.cpp:12` `ReplicationMode = Minimal` → **`Mixed`** 로 변경(주석에도 "멀티 진입 시 Mixed 권장" 명시됨). Mixed: 소유 클라에 GE 복제, 태그/큐는 전체 복제.

#### 위험 요소 / GC·생명주기
- **GC:** 신규 `TObjectPtr< AActor > RepCurrentWeapon`은 `UPROPERTY`이므로 GC 추적됨. 기존 약참조 `CurrentWeapon`과 이중 보유 시 수명 혼선 주의 — `OnRep`/`Drop`에서 동시 정리.
- **타이머 람다:** 임팩트 필드 제거 람다(`.cpp:785`)는 `TWeakObjectPtr` 캡처로 이미 안전. 신규 RPC 경로에서 람다 캡처 금지(멤버 함수 콜백 사용).
- **DS 안전(정정):** 현재 코드는 HUD/싱글턴 접근부에 널체크가 있어 **2-A 미적용이어도 DS 크래시는 나지 않음**(실측 확인). 2-A는 크래시 방지가 아니라 낭비 제거·패키징 빌드 견고성 목적의 **권장 사항**. 단, 향후 신규로 추가하는 UI/카메라/이펙트 코드는 널체크에 의존하지 말고 `IsLocallyControlled()`/`GetNetMode()` 가드를 처음부터 둘 것.
- **Attach 복제:** 무기 attach가 클라에서 소켓 미스매치 시 무기가 원점에 보일 수 있음 — `OnRep_CurrentWeapon`에서 재attach 보장.

#### 검증 방법 (멀티 PIE)
- PIE: `Number of Players = 2`, **Net Mode = Play As Client**(DS 월드 1개 + 클라 2개), **Use Single Process** 체크.
- 시나리오: 클라1이 픽업 → 클라2 화면에서도 무기 장착 보임 / 클라1 재장전 → `[TPS] Reload started`·`Reload finished` 로그가 **서버 윈도우에만** 출력, 클라 HUD 탄약은 RepNotify로 갱신 / 드랍 시 양쪽에서 픽업 액터 생성·무기 소멸.
- 로그 키워드: `LogGameplay`, `[TPS]`. `HasAuthority()` 분기 진입 확인용 임시 로그.
- DS 빌드 스모크: `TPS_TutorialServer` 타깃 빌드 후 콘솔 실행 → HUD/위젯 크래시 없이 부팅되는지.

---

### 3단계 — GAS 권위화 (Fire / Sprint / Damage / Death) + 태그·어트리뷰트 복제

#### 목표 / 범위
GAS 게임 권위(데미지, 사망, 스프린트 속도, 발사)를 서버 권위로 전환하고 **어트리뷰트·태그를 복제**한다. 4단계 로코모션 동기화의 **전제**(`TAG_State_Sprinting` 복제).

#### 구체적 변경 대상

**(3-A) 어트리뷰트 복제**
- `TPSAttributeSet.h:42-58`: 각 어트리뷰트에 `ReplicatedUsing` 추가:
  ```cpp
  UPROPERTY( BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vital" )
  FGameplayAttributeData Health;
  ```
- `TPSAttributeSet.cpp` 신규: `GetLifetimeReplicatedProps`에서 `DOREPLIFETIME_CONDITION_NOTIFY( UTPSAttributeSet, Health, COND_None, REPNOTIFY_Always )` (4종). `OnRep_Health` 등에서 `GAMEPLAYATTRIBUTE_REPNOTIFY( UTPSAttributeSet, Health, OldHealth )` 매크로 호출.
- `OnHealthChanged`/`OnStaminaChanged` 델리게이트 Broadcast(`PostGameplayEffectExecute`, `.cpp:53-80`)는 **서버에서 GE 실행 시** 발생하고, 클라에서는 OnRep을 통해 별도 갱신 경로 필요 — HUD 스태미너 바는 클라 OnRep에서도 갱신되도록 `OnRep_Stamina`에서 `OnStaminaChanged.Broadcast` 추가.

**(3-B) Sprint 어빌리티 권위화**
- `TPSGameplayAbility_Sprint`: `NetExecutionPolicy = LocalPredicted`, `InstancingPolicy = InstancedPerActor` 명시(현재 미설정 → 기본값). 생성자(`.cpp:14`)에 추가.
- `MaxWalkSpeed` 변경(`.cpp:77-78`)은 RepProp이 아니므로 **서버에서 변경** → 이동 예측에 자동 반영. LocalPredicted라 소유 클라도 즉시 속도 체감(예측), 서버 보정.
- StaminaDrain GE 적용(`.cpp:81-94`)/제거(`.cpp:108-115`)는 서버 권위. Mixed 모드에서 소유 클라에 복제.
- `ActivationOwnedTags`의 `TAG_State_Sprinting`(`.cpp:21`)이 **이제 전체 클라에 복제**됨(Mixed: 태그는 전원 복제) → 4단계의 `bIsSprinting`이 정확히 동작할 전제 완성.

**(3-C) Fire 어빌리티로 발사 권위화**
- `TPSCharacter::HandleFireWeaponInteract`(`.cpp:681-827`) 본체를 `UTPSGameplayAbility_Fire::ActivateAbility`(현재 스텁, `.cpp:18-34`)로 이관(스텁 TODO `TPSGameplayAbility_Fire.cpp:30`에 명시된 계획).
  - **권위(서버):** 라인트레이스, 탄약 차감(`--CurrentBullet`, `.cpp:696`), 데미지 적용(피격 대상 ASC에 `UTPSGameplayEffect_Damage::ApplyDamage`).
  - **예측(LocalPredicted):** 발사 입력 즉시 클라 반응(반동 입력 `.cpp:747-748`은 로컬 코스메틱).
  - **코스메틱(Multicast/로컬):** 몽타주(`.cpp:735`), 임팩트 필드 스폰(`.cpp:779`), 반동. Multicast RPC `MulticastPlayFireFX(ImpactPoint)`로 각 클라 로컬 생성. DS에선 FX 스킵(`NM_DedicatedServer` 가드).
- `Fire( bool )`(`.cpp:830`)/입력 경로 → `ASC->TryActivateAbilityByTag( TAG_Ability_Fire )`로 변경.

**(3-D) Damage / Death 권위화**
- 데미지: `UTPSGameplayEffect_Damage`(Instant, SetByCaller 음수 컨벤션) 적용을 **서버에서만**. `PostGameplayEffectExecute`(`.cpp:53`)는 서버에서 Health 변경 → 복제.
- 사망 `_HandleHealthChanged`(`.cpp:363`)/`_HandleOnDeath`(`.cpp:391`):
  - **권위(서버):** `AddLooseGameplayTag( TAG_State_Dead )`, `CancelAllAbilities`, `SetLifeSpan( 5.0f )`. Loose 태그는 복제 안 되므로 **Replicated 사망 플래그**(`bIsDead` RepNotify) 또는 GE 기반 `TAG_State_Dead`(Mixed 복제) 사용 권장. → GE/Replicated 방식 채택해 클라가 사망 인지.
  - **코스메틱(클라):** 라그돌(`SetSimulatePhysics`, `.cpp:415-419`). **DS에서 물리 시뮬레이션 비활성이 기본**이며 라그돌 복제는 불안정 → **Multicast(`MulticastOnDeath`)** 로 각 클라 로컬 라그돌. 입력 차단 `DisableInput`(`.cpp:411`)은 소유 클라/서버에서.

#### 서버 권위 vs 클라 예측
- **LocalPredicted:** Sprint(속도 체감), Fire(즉시 반응). **ServerOnly/ServerInitiated:** Damage, Death 판정. **복제:** 어트리뷰트(RepNotify), `TAG_State_Sprinting`/`TAG_State_Dead`(Mixed). **Multicast 코스메틱:** Fire FX, 라그돌.

#### 의존성
2단계(ASC Mixed 모드 + ActorInfo 멀티 분기 2-F, DS 가드 2-A)에 **강하게 의존**. 어트리뷰트 복제(3-A)와 Sprint 태그 복제(3-B)는 4단계의 직접 전제.

#### 위험 요소 / GC·생명주기
- **예측 미스 보정:** LocalPredicted Fire에서 탄약 차감을 클라 예측까지 하면 디싱크 가능 → 탄약 차감은 **서버 권위만**(예측 안 함), 발사 FX만 예측.
- **GE Minimal→Mixed 전환:** 기존 Minimal 가정 코드 부작용 점검(현재 GE는 self-target 위주라 영향 적음).
- **라그돌 DS:** `bEnablePhysicsOnDedicatedServer`를 켜지 말 것(서버 비용/불안정). 라그돌은 순수 클라 코스메틱.
- **Loose 태그 비복제 함정:** `AddLooseGameplayTag( TAG_State_Dead )`는 복제 안 됨 — 반드시 복제 경로 별도 마련.

#### 검증 방법
- 멀티 PIE 2클라: 클라1이 클라2 사격 → **서버 권위 데미지**로 클라2 Health 감소가 양쪽 HUD/디버그에 반영, 0이 되면 양쪽 화면에서 라그돌. 스프린트 시 양쪽에서 속도 증가 관찰.
- `showdebug abilitysystem` 콘솔로 어트리뷰트/태그 복제 확인.
- 로그: `[TPS] GAS Fire Ability triggered`, `[TPS] GAS Sprint Activated`, `[TPS] OnDeath Broadcast`가 **서버에서** 1회, 클라 코스메틱 별도.

---

### 4단계 — 로코모션 / 코스메틱 동기화 + 폴리시 마감

#### 목표 / 범위
원격 클라(다른 플레이어)에게 **애니메이션 상태가 정확히 보이도록** 한다. 특히 `bIsSprinting`이 복제된 GAS 태그에서 파생되도록 보장하고, 재장전/발사 몽타주 cosmetic을 동기화한다.

#### 구체적 변경 대상
- `AnimInstance/TPSAnimInstance.cpp:11-19` `NativeUpdateAnimation`: 현재 `UAbilitySystemGlobals::GetAbilitySystemComponentFromActor( GetOwningActor() )` → `HasMatchingGameplayTag( TAG_State_Sprinting )`로 `bIsSprinting` 산출. **이 로직은 그대로 두되**, 3단계에서 태그가 Mixed로 전체 복제되므로 **원격 프록시에서도 자동으로 올바른 값**이 된다(코드 변경 최소 — 핵심은 3단계 선행).
  - 보강: ASC 캐싱(매 틱 `GetAbilitySystemComponentFromActor` 호출 비용) — `NativeInitializeAnimation`에서 1회 캐시.
- 재장전 몽타주: `IsReloading` RepNotify(2단계)에서 클라가 재장전 몽타주 재생(`OnReload` 주석 `.cpp:852` "무기 타입별 재장전 몽타주" 계획). 원격 클라도 보이도록 Multicast 또는 RepNotify 기반.
- `MovingDirection`(`.h:69`)/`Roll`/`Pitch`(`.h:45,48`): 원격 프록시 애니에 필요하면 복제 추가 검토(스트레이프 애니/lean). 단 `Move`(`.cpp:451`)는 소유 클라에서만 계산되므로 `MovingDirection`은 원격에선 속도 벡터 기반 재계산이 더 저렴 — **AnimInstance에서 `GetVelocity()` 기반 로컬 산출** 권장(복제 비용 절감).

#### "로코모션을 먼저 하면 안 되는 이유" (단계 순서 근거 — 명시 요청 항목)
사용자는 로코모션 동기화를 앞당기고 싶어한다. 분리해서 판단한다.

- **앞당길 수 있는 부분 (거의 공짜):** **위치/속도/회전**은 `UCharacterMovementComponent`가 엔진 기본으로 복제한다(§1.1). 따라서 "원격 캐릭터가 걷고 뛰고 점프하는 위치/이동 애니의 속도 기반 파라미터"는 **GAS와 무관하게 이미 복제**되어 보인다. 별도 작업 거의 불필요.
- **앞당길 수 없는 부분 (GAS 복제 선행 필수):** `bIsSprinting`은 `TAG_State_Sprinting`(GAS `ActivationOwnedTags`)에서 파생된다(`TPSAnimInstance.cpp:17`). 이 태그는 현재 ASC `Minimal` 모드 + 단일 ActorInfo라 **원격 클라에 정확히 복제되지 않는다.** 즉 4단계의 `bIsSprinting` 동기화는 **3단계(ASC Mixed + 태그 복제)** 가 반드시 선행돼야 한다. 또한 스프린트 "속도"(`MaxWalkSpeed`) 자체도 3단계에서 서버 권위로 바뀌어야 원격 위치 예측이 맞는다.
- **재장전이 로코모션보다 먼저인 이유:** 재장전·탄약은 **권위 게임 상태**(승패/탄 소모에 직결)로, 디싱크 시 게임플레이가 깨진다. 반면 로코모션 `bIsSprinting`은 **코스메틱**(틀려도 게임 결과 불변, 잠깐 어색할 뿐)이다. 권위 게임 상태를 먼저 견고히 한 뒤, 그 위에서 파생되는 코스메틱을 맞추는 것이 의존성·리스크 양면에서 옳다. 또한 `bIsSprinting`의 데이터 소스(`TAG_State_Sprinting`)가 3단계 산출물이므로 **데이터 의존 방향상으로도** 로코모션이 마지막이다.

> **결론:** 위치/속도 기반 이동 애니는 이미 복제되어 "지금도 어느 정도 보인다". 하지만 `bIsSprinting` 같은 **GAS 태그 파생 cosmetic은 3단계(태그 복제) 없이는 원격에서 옳게 동기화될 수 없다.** 따라서 로코모션 "전체"를 앞당기는 것은 불가하며, 앞당길 수 있는 것은 GAS 비의존 부분뿐이다.

#### 의존성
3단계(태그·속도 복제)에 전적으로 의존. 단독 선행 불가.

#### 위험 요소 / 성능
- **매 틱 ASC 조회:** `NativeUpdateAnimation`의 `GetAbilitySystemComponentFromActor`(`.cpp:15`)는 매 프레임 호출 → 캐싱으로 비용 절감(원격 프록시 다수 시 누적).
- **불필요한 복제:** `MovingDirection`/`Roll` 복제는 대역폭 낭비 가능 — 가능하면 클라 로컬 산출.
- **몽타주 중복:** 소유 클라 예측 재생 + Multicast 재생 이중 재생 방지(소유 클라 Multicast 스킵).

#### 검증
- 멀티 PIE 2클라: 클라1이 스프린트 → **클라2 화면의 클라1 캐릭터**가 달리기 애니로 전환되는지(가장 중요한 회귀 포인트). 재장전 시 원격에서 재장전 모션. `bIsSprinting` 디버그 출력으로 원격 프록시 값 확인.

---

## 3. UE 특화 위험 항목 체크리스트

- [ ] **UPROPERTY/GC:** 신규 `RepCurrentWeapon`은 `TObjectPtr` + `UPROPERTY`로 GC 추적(컨벤션 §4.3). 기존 약참조 `CurrentWeapon`과 수명 이중관리 정리.
- [ ] **use-after-free / 댕글링:** RPC 콜백에 람다 캡처 금지(멤버 함수 사용). 기존 타이머 람다(`.cpp:728`,`.cpp:785`)는 `TWeakObjectPtr` 캡처로 안전 — 패턴 유지. `EndPlay`의 델리게이트 `RemoveAll(this)`(`.cpp:117-121`) 페어링 유지(컨벤션 §8.4).
- [ ] **Tick 비용:** AnimInstance 매 틱 ASC 조회 캐싱. `TPSCharacter::Tick`(`.cpp:255`) lean/jump는 소유 클라만 의미 → 원격 프록시 Tick 비용 점검.
- [ ] **SceneCapture/RenderTarget GPU:** 스나이퍼 `SceneCaptureComponent2D`(`TPSEquipSniperRifle.h:19`)는 **클라 줌 시에만** 활성(`SetSceneCaptureEnabled`) 유지. 서버/원격에서 절대 활성 금지(TDR 위험).
- [ ] **Chaos FieldSystem:** `ATPSShotImpactField` 복제 금지 + DS(`NM_DedicatedServer`)에서 스폰 스킵. DS 쿡 시 GeometryCollection 누락 가능성 점검.
- [ ] **Replication / NetMode 분기:** `HasAuthority()` / `IsLocallyControlled()` / `GetNetMode()` 가드를 권위·예측·코스메틱 경로별로 명확히. ASC `Mixed` 모드 + Owner=Controller 보장.
- [ ] **PIE vs Standalone vs DS:** PIE Single Process로 1차 검증 후, 실제 `TPS_TutorialServer` 빌드 스모크 별도 수행(PIE는 일부 DS 경로를 우회).
- [ ] **모듈 순환 의존:** 신규 모듈 추가 없음 — 순환 위험 없음. `GameplayAbilities`/`GameplayTags` 이미 PublicDependency.

## 4. 롤백 전략

- **변경 단위:** 각 단계 = 1 PR, 각 소항목(2-A~2-F 등) = 개별 커밋. 사용자 정책상 **커밋은 사용자가 직접 수행**(본 문서/구현은 코드만, 커밋 메시지 초안 제공).
- **되돌리기:** 액터 복제는 생성자 1줄(`bReplicates`) 토글로 격리 가능. ASC `Mixed↔Minimal`은 `TPSAbilitySystemComponent.cpp:12` 한 줄. 어트리뷰트 RepNotify는 `UPROPERTY` 지정자만 제거하면 단일 플레이어 동작 복귀.
- **영향 격리:** 2-A(DS 가드)는 단일 플레이어에 영향 없음(항상 LocalController) → 안전하게 선반영 가능. GAS 권위화(3단계)는 BP 어빌리티 에셋의 NetExecutionPolicy/InstancingPolicy 설정과 동기화 필요 — C++ 기본값으로 강제하되 BP 오버라이드 점검.
- **피처 플래그:** 위험 큰 3단계 Death/Damage는 `bUseServerAuthoritativeCombat` 같은 임시 토글로 단계적 활성 고려(선택).

## 5. 미해결 질문 / 결정 필요 사항 (검토자/사용자 답변 요망)

1. **ASC 위치:** ASC를 현행대로 **Pawn(캐릭터)** 에 유지할지, **PlayerState**로 이전할지? 튜토리얼 규모상 Pawn 유지 권장하나, 리스폰 지속성/관전 요구가 있으면 PlayerState 필요. (PlayerState 이전 시 NetUpdateFrequency 상향 필요.)
2. **1단계 Config 실제 내용:** commit `81c1f0c`가 정확히 어떤 Config 키를 바꿨는가? `DefaultEngine.ini`에 네트워크 키가 없어 확인 필요(기본값 가정으로 진행 중).
3. **입력 경로:** `SetupPlayerInputComponent` 전체 주석 처리(`.cpp:271-290`) — 현재 입력 바인딩은 BP에서 수행 중인가? 멀티 전환 시 입력→Server RPC 경로를 C++로 회수할지 BP 유지할지 결정 필요.
4. **GameMode/GameState:** 현재 `AGameMode`/`AGameStateBase` 커스텀 클래스가 보이지 않음(Controller만 존재). 멀티 스폰/리스폰 규칙을 담을 GameMode 신설 필요 여부.
5. **Fire 데미지 대상:** 현재 발사는 라인트레이스 후 **임팩트 필드만** 스폰하고 실제 캐릭터 데미지 적용 코드가 없음(`HandleFireWeaponInteract`). 데미지 대상이 캐릭터인지(PvP) 환경 파괴만인지 — 3-D 범위 확정에 필요.
6. **재장전/발사 몽타주 동기화 정책:** Multicast vs RepNotify 기반 중 선택(대역폭 vs 신뢰성).
7. **Iris 사용 여부:** UE5.3 실험적 Iris 리플리케이션 시스템 도입 의사 — 기본(generic replication) 가정 중.

---

**근거 파일(모두 직접 Read 확인):** `Character/TPSCharacter.h/.cpp`, `AnimInstance/TPSAnimInstance.h/.cpp`, `GAS/TPSAbilitySystemComponent.h/.cpp`, `GAS/TPSAttributeSet.h/.cpp`, `GAS/TPSGameplayTags.h`, `GAS/Abilities/TPSGameplayAbility_Sprint.h/.cpp`, `GAS/Abilities/TPSGameplayAbility_Fire.h/.cpp`, `GAS/Effects/TPSGameplayEffect_Damage.h`, `Actors/TPSEquipBase.h/.cpp`, `Actors/TPSEquipSniperRifle.h`, `Actors/TPSPickUpBase.h/.cpp`, `Actors/TPSShotImpactField.h`, `Controller/TPSPlayerController.h/.cpp`, `GameInstance/TPSGameInstance.h`, `UI/TPSHUD.h`, `Source/*.Target.cs`, `TPS_Tutorial.Build.cs`, `Docs/CodingConventions.md`.
