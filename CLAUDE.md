# TPS_Tutorial — 프로젝트 메모리

> 이 파일은 cwd가 이 폴더 트리 안일 때 Claude Code가 자동 로드합니다.
> 즉, **`E:\Unreal_Project\TPS_Tutorial`에서 Claude를 띄우면** 이 컨텍스트가 켜집니다.

## 한 줄 요약
Unreal Engine 5.x 기반 TPS(3인칭 슈터) 학습/튜토리얼 프로젝트. 단일 게임 모듈, GAS 사용.

## 경로
- 프로젝트 루트: `E:\Unreal_Project\TPS_Tutorial`
- uproject: `E:\Unreal_Project\TPS_Tutorial\TPS_Tutorial.uproject`
- VS 솔루션: `E:\Unreal_Project\TPS_Tutorial\TPS_Tutorial.sln`
- C++ 모듈: `Source\TPS_Tutorial\` (모듈명 `TPS_Tutorial`, Runtime / LoadingPhase Default)
- Content / Config / Docs: 루트 하위 표준 위치

## 엔진 / 플러그인
- UE5.x (엔진 연결 GUID 기반 — 정확한 버전은 에디터에서 확인)
- 의존 모듈: Engine, UMG
- 주요 플러그인: **GameplayAbilities(GAS)**, ModelingToolsEditorMode, LiveLink, RigLogic, HairStrands

## Source 구조 (기능별)
- `Actors/` — 장비(TPSEquipBase, TPSEquipSniperRifle), PickUp, Impact
- `Character/` — TPSCharacter (메인 캐릭터)
- `AnimInstance/` — TPSAnimInstance
- `Controller/` — TPSPlayerController
- `GameInstance/` — TPSGameInstance
- `GAS/` — TPSAbilitySystemComponent 등 GAS
- `Logic/` — 코어 로직, 인터페이스(ITPSInteractionActorInterface), TPSTypes
- `Manager/`, `UI/`, `Util/`, `Consts/`, `Log/`

## 네이밍 / 코딩 컨벤션
> ⚠️ **권위 기준: `Docs/CodingConventions.md`** — 코드 작성·수정·규약 점검 시 반드시 이 문서를 먼저 읽고 따른다. (실제 소스를 역공학한 단일 레퍼런스. 추론 금지)

핵심 요약 (전체는 위 문서):
- UE 표준 접두사 + 프로젝트 접두사 `TPS`: `ATPSCharacter`, `UTPSGameInstance`, `FWeaponTableData`, `ITPS...`
- 괄호·꺽쇠 **안쪽 공백 강제**: `if ( cond )`, `Func( arg )`, `Cast< T >( ptr )`, `TArray< T* >` (§3.1/§3.2) — `.cpp` 정의부 시그니처까지 적용
- 멤버 PascalCase / 지역 camelCase / **private helper만 `_PascalCase`** (§2.3~2.5)
- **기존 raw 포인터는 유지**, 신규 UObject 멤버에만 `TObjectPtr< T >` (§4.3) — 일괄 마이그레이션 금지
- 들여쓰기 탭, trailing whitespace 금지, CRLF 유지 (§3.3/§12.8)
- 선언부(.h)·정의부(.cpp) 각각 위에 한국어 `~한다.` 한 줄 주석 (§6.1)
- include: `CoreMinimal.h` 최상단 → 알파벳 → `*.generated.h` 마지막 (§5.1)
- `.clang-format` 없음 → 위 문서가 포맷 기준

## VCS — Git
- 현재 작업 브랜치: `GAS` / 기준 `master`
- `.gitignore`, `.gitattributes` 존재 (표준 UE 템플릿)
- 커밋 메시지: `<type>: <설명>` (feat/fix/refactor/docs/test/chore)

## 빌드 / 실행
- `.uproject`에서 VS 솔루션 생성 후 빌드, 또는 UE 에디터로 열기
- 작업 흐름은 `.claude/`의 UE 오케스트레이터 사용 가능

## 하네스 (.claude/ 이미 존재)
- 에이전트: `ue-planner`, `ue-plan-reviewer`, `ue-implementer`, `ue-codex-reviewer`, `ue-cl-aggregator`
- 커맨드: `ue-orchestrate`
- C++/리팩토링 작업은 `/ue5-orchestrate` 흐름(플래너→검토→구현→codex 리뷰) 권장
