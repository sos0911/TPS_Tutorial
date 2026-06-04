# UE5AIAssistant — BP/AnimBP 분석용 에디터 MCP 셋업 가이드

> 실행 중인 UE5.3 에디터에 HTTP로 붙어 **Blueprint / AnimBP 그래프(노드·스테이트머신·변수·핀·연결)를 읽고 수정**하는 에디터 플러그인.
> `.uasset`은 바이너리라 텍스트로 못 읽으므로, BP/AnimBP를 코드처럼 분석할 때 이 플러그인을 쓴다.
>
> 출처: https://github.com/1103837067/ue5-editor-control (원본은 UE 5.4+ 표방 → **본 프로젝트는 5.3용으로 패치 적용 완료**, 아래 참고).

---

## 1. git에 이미 포함된 것 (pull 하면 받아짐)

- `Plugins/UE5AIAssistant/Source/**` + `UE5AIAssistant.uplugin` — **5.3 패치가 소스에 반영된 상태**
- `TPS_Tutorial.uproject` — 플러그인 enable 항목 추가됨 (Editor 타깃 한정)
- `.claude/scripts/exec_cmd.ps1`, `.claude/scripts/ping_poll.ps1` — 호출 헬퍼
- 이 문서

> `Plugins/UE5AIAssistant/Binaries`·`Intermediate`는 `.gitignore` 대상(머신별 재빌드) → **pull 후 한 번 빌드 필요**.

## 2. 사전 요구사항

- **Unreal Engine 5.3** (에픽 런처 바이너리로 충분, 소스 빌드 불필요)
- **Visual Studio 2022** + "C++를 사용한 게임 개발" 워크로드 (또는 Build Tools)

## 3. 셋업 절차 (집 컴퓨터 / 새 체크아웃)

```
1) git pull   (위 1번 파일들이 들어옴)

2) 프로젝트 파일 재생성
   - 탐색기에서 TPS_Tutorial.uproject 우클릭 → "Generate Visual Studio project files"
     (엔진 연결을 자동으로 따라가므로 엔진 설치 경로 신경 안 써도 됨)

3) 플러그인 컴파일 — 둘 중 하나
   (a) TPS_Tutorial.uproject 더블클릭 → "modules are missing/out of date, rebuild?" → Yes
       → 에디터가 UE5AIAssistant 포함 모듈을 자동 컴파일 후 실행
   (b) 또는 VS에서 'Development Editor | Win64' 빌드
       또는 CLI:
       <UE5.3>/Engine/Build/BatchFiles/Build.bat TPS_TutorialEditor Win64 Development ^
         -Project="<경로>/TPS_Tutorial.uproject" -WaitMutex
       (<UE5.3>는 각 머신의 엔진 설치 경로. 이 작업 PC에선 E:/UnrealEngine/UE_5.3)

4) 에디터가 떠 있어야 HTTP 서버 가동
   - 플러그인은 PostEngineInit에 localhost:58080 서버를 띄움
   - MetaHuman 프로젝트라 첫 실행 시 셰이더 컴파일이 오래 걸림(수~수십 분).
     메인 창이 뜨면(셰이더 컴파일 중이어도) 보통 서버는 사용 가능.
```

## 4. 동작 확인

```
powershell -NoProfile -ExecutionPolicy Bypass -File .claude/scripts/ping_poll.ps1
```
기대 출력:
```
PING_OK: {"status":"ok","plugin":"UE5AIAssistant","version":"1.0","engineVersion":"...5.3...","commandCount":67,...}
```

## 5. 사용법 (명령 실행)

curl/WebFetch가 막힌 환경(Claude Code 훅)이 있어 **PowerShell `Invoke-RestMethod`** 로 호출하고,
쉘 이스케이프로 JSON이 깨지는 걸 막기 위해 **요청 본문을 파일로** 전달한다.

```
1) 본문 파일 작성 (예: req.json)
   {"command":"get_anim_blueprint_info","args":{"blueprint_name":"ABP_Emanuel"}}

2) 실행
   powershell -NoProfile -ExecutionPolicy Bypass -File .claude/scripts/exec_cmd.ps1 -BodyFile req.json
```

자주 쓰는 명령:
| 명령 | args | 용도 |
|---|---|---|
| `get_anim_blueprint_info` | `{"blueprint_name":"ABP_..."}` | AnimBP 스테이트머신/상태/변수 요약 |
| `read_blueprint_content` | `{"blueprint_name":"..."}` | 그래프(EventGraph/AnimGraph/함수) 노드·핀·연결 전체 |
| `list_functions` / `list_node_types` / `search_assets` | 발견(discovery) |

엔드포인트: `/api/ping`, `/api/execute`, `/api/commands`. 전체 67개 명령.

> **한계**: PropertyAccess의 바인딩 경로 문자열, 스테이트 전이(transition) 규칙 내용은 덤프되지 않음 → 그 부분은 에디터에서 직접 확인.

## 6. 5.3 포팅 패치 (이미 소스에 반영 — 플러그인 재설치/업데이트 시 재적용 필요)

원본(5.4+)을 5.3에서 컴파일하기 위한 수정:

- `Source/UE5AIAssistant/Private/HttpCommandServer.cpp`
  - `FHttpRequestHandler::CreateRaw(this, &...)` → 람다 3곳
    (5.3의 `FHttpRequestHandler`는 델리게이트가 아니라 `TFunction<bool(const FHttpServerRequest&, const FHttpResultCallback&)>`라 `CreateRaw` 없음)
  - 예: `FHttpRequestHandler( [this]( const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete ) { return HandleExecute( Req, OnComplete ); } )`
- `Source/UE5AIAssistant/Private/Commands/EditorCommandHandler.cpp` (2곳, line ~265 / ~384)
  - `bool bImported = FoundProp->ImportText_Direct(...)` → 끝에 `!= nullptr` 추가
    (반환형 `const TCHAR*` → bool 암시 변환이 C4800 warning-as-error)

## 7. 트러블슈팅

- **ping이 PING_FAIL** → 에디터가 아직 PostEngineInit 전(로딩/셰이더 컴파일 중)이거나 닫힘. 메인 창 뜬 뒤 재시도. 로그 `Saved/Logs/<Project>.log`에서 `[UE5AIAssistant] ... HTTP Server starting on port 58080` 확인.
- **포트 충돌(58080)** → 다른 에디터 인스턴스가 점유 중일 수 있음.
- **빌드 실패** → 엔진이 5.3이 아닐 가능성(다른 버전은 다시 API 차이 발생). 5.3 고정.
- **plugin not loaded** → `.uproject`의 Plugins에 `UE5AIAssistant`(Enabled, TargetAllowList=Editor)가 있는지 확인.
- **".uproject 더블클릭 시 엔진 버전 선택 창"** → `.uproject`의 `EngineAssociation`이 GUID라 머신마다 다름(정상). 집 PC에선 본인 UE5.3을 선택하면 됨. 매번 묻기 싫으면 우클릭 → "Switch Unreal Engine version"으로 고정.
