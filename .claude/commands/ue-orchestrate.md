---
description: Unreal Engine 4/5 코드 수정 전용 오케스트레이션 — 구현 플래너 → 플랜 검토 → 구현 → codex 리뷰 순으로 진행합니다. 코드 수정/리팩토링/버그 수정 작업에 사용하세요.
argument-hint: [mode] <작업 설명>
---

# /ue-orchestrate

Unreal Engine 4/5 코드 작업을 위한 다단계 오케스트레이터.

## 사용법

```
/ue-orchestrate <작업 설명>             # 풀 워크플로우 (기본)
/ue-orchestrate plan <작업 설명>        # 플랜 단계만
/ue-orchestrate review                  # 현재 변경분에 대해 codex 리뷰만
/ue-orchestrate implement <작업 설명>   # 플래너→리뷰어 생략, 구현부터 (위험)
/ue-orchestrate stage                   # 현재 변경분을 CL/스테이지로 모으기만
```

## 사용자 입력

`$ARGUMENTS`

## 워크플로우

### 풀 모드 (기본)

다음 순서로 **순차 실행**합니다. 한 단계라도 차단되면 다음 단계로 넘어가지 않고 사용자에게 보고합니다.

#### 1단계: ue-planner
`Agent` 도구로 `subagent_type: "ue-planner"` 호출.

프롬프트:
```
다음 Unreal Engine 작업에 대한 구현 계획을 수립해 주세요.

작업: $ARGUMENTS

프로젝트 컨텍스트:
- UE 5.3, DX12, Lumen, Virtual Shadow Maps
- TPS 튜토리얼 프로젝트
- 브랜치: cppCompDev (또는 현재 브랜치)
- 메모리 정책: 자동 커밋 금지

조사 → 계획 산출까지 한 번에 완료해 주세요.
```

산출물(계획 문서)을 다음 단계 입력으로 보관.

#### 2단계: ue-plan-reviewer
`Agent` 도구로 `subagent_type: "ue-plan-reviewer"` 호출.

프롬프트에 1단계의 계획 문서를 통째로 첨부.

판정:
- `APPROVED` → 3단계로
- `REVISION_REQUIRED` → 1단계로 루프백 (최대 2회). 루프백 시 검토 코멘트를 ue-planner에게 전달.
- `REJECTED` → 사용자에게 보고하고 종료.

#### 3단계: ue-implementer
`Agent` 도구로 `subagent_type: "ue-implementer"` 호출.

프롬프트에 **승인된 계획 문서**를 첨부.
구현이 완료되면 변경된 파일 목록을 4단계로 전달.

#### 4단계: ue-codex-reviewer
`Agent` 도구로 `subagent_type: "ue-codex-reviewer"` 호출.

판정:
- `PASS` → 5단계로 진행.
- `NEEDS_FIX` → 사용자에게 보고. 사용자가 동의하면 3단계로 루프백 (최대 1회).
- `BLOCKED` (codex CLI 환경 문제: capacity 초과·모델 미지원·미설치·인증 실패 등):
  - **자동 Claude 폴백**으로 전환하여 동일 체크리스트로 셀프 리뷰 수행.
  - 폴백 결과의 PASS/NEEDS_FIX 판정으로 그대로 워크플로우 진행.
  - 최종 보고서에 "codex 차단 사유 + Claude 폴백 사용" 명시.
  - 사용자가 codex 회복 후 별도 재실행을 원할 수 있으므로 안내 한 줄 포함.

#### 5단계: ue-cl-aggregator
`Agent` 도구로 `subagent_type: "ue-cl-aggregator"` 호출.

프롬프트에 다음을 함께 전달:
- ue-implementer 보고서의 "단계별 적용 결과" 섹션 (커밋 메시지 본문 후보)
- 작업 한 줄 요약 (메인 제목 후보)
- 코덱스 리뷰에서 PASS 판정된 사실

이 단계의 산출물:
- VCS 모드 (GIT or P4)
- 스테이지/CL에 모인 파일 목록
- 한국어 커밋 메시지 초안 (메인 제목  - 항목1  - 항목2 형식)

오류:
- `[NO CHANGES]` 또는 `[NO VCS]` → 6단계로 진행하되 보고서에 명시.
- 의심 파일(.env, Saved/, 100MB+ 등) 감지 → 보고서의 "사용자 확인 필요" 섹션에 표시.

#### 6단계: 최종 보고서

```markdown
# 오케스트레이션 보고

작업: <한 줄 요약>
모드: full
결과: [완료 | 부분 완료 | 차단]

## 단계별 결과
1. ue-planner: [완료] - 계획 문서 N단계
2. ue-plan-reviewer: [APPROVED in N차 시도]
3. ue-implementer: [완료] - 파일 N개 변경
4. ue-codex-reviewer: [PASS / NEEDS_FIX]
5. ue-cl-aggregator: [완료 / NO CHANGES / NO VCS] - VCS=<GIT|P4>

## 변경 파일
- ...

## codex 리뷰 핵심 코멘트
- CRITICAL: ...
- HIGH: ...

## 스테이징 / CL 상태
- VCS: <GIT | P4>
- 모인 파일 수: N
- 제외/경고 항목: ...

## 커밋 메시지 초안 (사용자가 직접 커밋)
```
<메인 제목>  - 항목1  - 항목2  - 항목3
```

## 사용자 권장 액션
- [ ] PIE 검증
- [ ] 빌드 (Visual Studio / Rider)
- [ ] `git diff --cached` 또는 `p4 describe -s <CL>` 로 검토
- [ ] 위 메시지로 커밋/체크인 (Co-Authored-By 미포함)
```

### plan 모드

1단계와 2단계만 실행. 산출물은 사용자에게 마크다운으로 출력.

### review 모드

4단계만 실행. 1~3, 5단계 건너뜀.
- `git status --porcelain`이 비어 있으면 즉시 안내하고 종료.

### implement 모드

3, 4, 5단계 실행. 1~2단계 건너뜀.
사용자가 이미 계획을 갖고 있고 빠르게 진행하길 원할 때.
**경고:** 검토되지 않은 변경이 들어갈 수 있음 — 첫 메시지에 경고 출력.

### stage 모드

5단계(ue-cl-aggregator)만 실행. 코드 변경/리뷰 없이 현재 작업 트리 변경분을 VCS에 모으고 커밋 메시지 초안만 생성.
- `git status --porcelain` 또는 `p4 status`가 비어 있으면 즉시 안내하고 종료.

## 병렬화

기본은 순차 실행. 단, 다음은 병렬화 가능:
- 풀 모드 4단계 직후, 추가로 자체 리뷰가 필요하면 `code-reviewer` (있다면)와 `ue-codex-reviewer`를 병렬 호출.

## 오류 처리

- 어떤 에이전트라도 `[BLOCKED]` 보고를 하면 즉시 사용자에게 정지 사유 노출.
- codex CLI 미설치 시: `winget install OpenAI.Codex` 또는 `npm i -g @openai/codex` 안내 후 종료.
- 각 단계 결과는 한국어로 요약해서 사용자에게 진행 상황을 알린다.

## 정책

- **자동 커밋 금지**: 모든 변경은 사용자가 직접 검토 후 P4/Git에 체크인.
- **커밋 메시지 스타일**: 한국어 메인 제목 + `  - ` 구분자. Co-Authored-By 금지.
- **노션 정리**: 사용자가 별도 요청 시에만, "AI 테스트 공간" DB(data_source: `31040b42-4f0d-80a0-87d9-000b6802d5ad`)에 이전 항목(QAENN-19686) 형식으로 작성.

## 시작

지금 즉시 `$ARGUMENTS`를 파싱해 모드를 결정하고, 위 워크플로우대로 첫 단계를 시작하세요.
- 첫 토큰이 `plan` / `review` / `implement` / `stage` 중 하나면 해당 모드로 진입, 나머지를 작업 설명으로 사용.
- 그렇지 않으면 풀 모드로 진입, 전체를 작업 설명으로 사용.