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
- UE 표준 접두사 + 프로젝트 접두사 `TPS`: `ATPSCharacter`, `UTPSAnimInstance`, `FTPS...`, `ITPS...`
- 헤더 스타일: `#pragma once` → `CoreMinimal.h` → 기타 include → `*.generated.h`
- 한국어 인라인 주석 사용 (예: `// TPS 캐릭터 클래스`)
- `.clang-format` 없음 → IDE/UE 기본 포맷 따름

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
