# Development Log

## 2026-06-08 - 직접 공격 타겟 사망 시 재탐색

### 변경 내용
- `Unit`에 직접 공격 타겟이 사망했을 때만 남는 `attackRetarget` 요청 상태를 추가.
- AttackMove/Patrol은 기존 검색/복귀 흐름을 그대로 사용하고, 직접 Attack 명령만 타겟 사망 후 주변 적을 재탐색하도록 분리.
- 이동, 정지, 보류, 채집, 건설, 새 공격 명령 등 명시적인 명령 전환 시 오래된 재탐색 요청을 해제하도록 정리.
- `GameLogicManager`가 재탐색 요청이 있는 Idle 유닛을 검사해 가까운 적을 찾으면 즉시 새 직접 공격을 발행하고, 없으면 요청을 해제.
- `DEVELOPMENT_PLAN.md`의 기본 전투/Combat 안정화 항목에서 타겟 사망 재탐색을 완료 처리.

### 동작 결과
- 직접 공격 중 타겟이 죽어도 주변 적이 있으면 유닛이 멈추지 않고 다음 적을 이어서 공격.
- 주변에 공격 가능한 적이 없으면 기존처럼 Idle 상태로 남음.
- AttackMove, Patrol, Hold의 별도 타겟 탐색 정책은 유지.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.

## 2026-06-07 - Formation 기본 grid-slot 이동 적용

### 변경 내용
- `MovementSystem::formationTargets()`를 추가해 선택된 살아있는 유닛을 현재 위치 기준으로 정렬하고, 목적지 주변 grid slot을 유닛별 목표로 배정.
- 여러 유닛 이동, AttackMove, Patrol 명령이 한 점 대신 분산된 슬롯으로 path request를 발행하도록 변경.
- Shift 이동 큐도 같은 formation target 계산을 사용해 예약된 waypoint가 유닛별 슬롯을 보존하도록 연결.
- 슬롯이 정적 지형, 이미 예약된 셀, 또는 선택되지 않은 다른 게임 오브젝트와 겹치면 가까운 주변 셀을 검색하도록 보정.
- 근처 안전 슬롯을 찾지 못한 경우에는 기존 path failure/approach 처리에 맡기도록 요청된 formation 위치를 유지.
- `DEVELOPMENT_PLAN.md`의 Epic 3.5 Formation을 기본 구현 완료 상태로 갱신.

### 동작 결과
- 다중 선택 유닛이 Move/AttackMove/Patrol 명령을 받을 때 서로 다른 목적지 셀로 이동해 한 점에 몰리는 현상을 줄임.
- 선택된 유닛끼리는 이동 중 비워질 자리로 취급하고, 선택되지 않은 유닛/건물/자원은 슬롯 점유자로 취급해 목표 셀 중복을 피함.
- 단일 유닛 이동은 기존처럼 정확한 클릭 위치를 목표로 유지.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.

## 2026-06-07 - Local Avoidance 기본 push 적용

### 변경 내용
- `CollisionSystem`에 주변 유닛을 검색해 겹침 위험이 있는 경우 제한된 local avoidance push vector를 계산하는 `localAvoidancePush()`를 추가.
- 완전히 같은 위치에 겹친 유닛은 EntityId 시스템 도입 전까지 World 삽입 순서를 fallback 방향 결정에 사용.
- push 크기를 tick당 최대 8px로 제한해 지나치게 밀리는 현상을 줄임.
- `MovementSystem`이 이동/순찰 중인 유닛에 push를 적용하되, push 결과 위치가 여전히 blocker와 충돌하면 기존 재경로/정지 흐름을 유지.
- `updateMove()`가 도착 처리로 유닛을 Idle로 바꾼 경우에는 push를 적용하지 않도록 보호.
- `DEVELOPMENT_PLAN.md`의 Epic 3.4를 EntityId 순서 전환만 남은 상태로 갱신.

### 동작 결과
- 이동 중인 유닛이 다른 살아있는 유닛과 너무 가까워지면 작은 push로 부드럽게 비켜남.
- 안전하게 밀 수 없는 상황은 기존 collision blocker, avoidance path, stop 흐름이 처리.
- 건물/자원은 local push 대상에서 제외하고 기존 blocker 판정을 유지.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.

## 2026-06-07 - 타입별 충돌 반경 기반 겹침 방지

### 변경 내용
- `CollisionSystem`의 겹침 판정을 고정 유닛 간 거리 대신 `moving unit radius + blocker radius` 기준으로 변경.
- blocker 타입별 기본 반경을 분리: Unit 28px, ResourceNode 44px, Building 52px.
- 자원은 채집 거리 경계에서 막히도록 설정해 유닛이 자원 중심까지 파고들지 않으면서도 기존 gather 판정을 유지.
- 건물은 현재 gather/build 상호작용 거리보다 작게 설정해 충돌 정지가 작업 도착 판정보다 먼저 걸리지 않도록 조정.
- `DEVELOPMENT_PLAN.md`의 Epic 3.3 추가 Task를 완료로 갱신.

### 동작 결과
- 이동 유닛은 유닛/자원/건물의 타입별 반경을 기준으로 겹침을 피함.
- 사망한 대상은 기존처럼 충돌 후보에서 제외.
- 자원과 건물은 완전히 통과 가능한 점이 아니라 blocker로 취급되지만, 작업 상호작용 거리는 유지.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.

## 2026-06-07 - PathRequestQueue 기본 구현

### 변경 내용
- `MovementSystem`에 `PathRequest` 구조와 내부 요청 큐를 추가해 Move/AttackMove/Patrol 경로 계산을 즉시 실행하지 않고 예약하도록 변경.
- `MovementSystem::update()` 시작 시 queued path request를 tick당 최대 8개만 처리하도록 제한.
- path request 완료 시 기존 `issuePathOrder()` 경로 적용 로직을 호출해 Unit path 결과를 즉시 반영.
- 유닛별 최신 request id를 추적해 새 명령이 들어오면 오래된 path request가 나중에 적용되지 않도록 무효화.
- Stop/Hold/Attack/Gather/Build 같은 즉시 명령 경로에서 pending path request를 취소하도록 `GameLogicManager`와 연결.
- `DEVELOPMENT_PLAN.md`의 Epic 3.2 작업 항목을 기본 구현 완료 상태로 갱신.

### 동작 결과
- 다수 유닛에게 동시에 이동/공격이동/순찰 경로를 발행해도 한 tick에서 모든 A*를 몰아서 계산하지 않음.
- 새 이동 명령은 이전 pending request를 대체하고, Stop/Hold 등은 pending request를 취소.
- 큐 처리량을 넘는 유닛은 다음 tick들에 순차적으로 경로를 받아 이동을 시작.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.
- 한계: 100기 동시 이동의 체감 프레임 안정성은 별도 대량 스폰/수동 스트레스 검증이 필요.

## 2026-06-07 - A* 이동 정책 점검 항목 정리

### 변경 내용
- `PathOptions`에 대각선 이동, corner cutting 방지, 지형 비용 처리의 기본 의미를 코드 주석으로 명확히 고정.
- `MovementSystem`의 게임플레이 이동 정책이 대각선 이동을 허용하되 막힌 side cell 사이를 대각선으로 통과하지 못하게 한다는 점을 명시.
- 기존 `PathManager`의 `canEnterNeighbor()` 검사와 `GameWorld::isTileBlocked()`/`moveCost == 0` 규칙을 기준으로 `DEVELOPMENT_PLAN.md`의 Epic 3.1 추가 점검 항목을 완료 처리.

### 동작 결과
- 유닛 이동 경로는 기존처럼 대각선을 허용.
- 대각선 이동 중 양옆 cardinal cell 중 하나라도 막혀 있으면 해당 대각선 step은 제외.
- `moveCost == 0` 타일은 이동 불가, `moveCost > 1` 타일은 더 비싼 통과 가능 지형으로 처리.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.

## 2026-06-07 - A* 지형 이동 비용 반영

### 변경 내용
- `IGridQuery`에 타일 진입 비용 조회 API를 추가하고 `GameWorldGridQuery`가 `GameWorld`의 타일 이동 비용을 반환하도록 연결.
- `GameWorld`에 범위 밖은 0, 범위 안은 `TileMapSoA::moveCost`를 반환하는 `tileMoveCost()`를 추가.
- `PathOptions`에 `useTerrainCost` 옵션을 추가하고, `PathManager`의 A* g-score 계산이 진입 타일 비용을 곱해 누적되도록 변경.
- 경로 캐시 키에 `useTerrainCost`를 포함해 비용 적용 여부가 다른 경로 요청이 같은 캐시 결과를 공유하지 않도록 정리.
- `DEVELOPMENT_PLAN.md`의 Epic 3.1 추가 점검 항목 중 `지형 비용 처리`를 완료로 갱신.

### 동작 결과
- `moveCost == 0`인 타일은 기존처럼 이동 불가 타일로 처리.
- `moveCost > 1`인 타일은 통과 가능하지만 A*가 더 비싼 경로로 평가하므로, 우회 경로가 충분히 싸면 우회하도록 선택.
- 대각선 이동은 기존 정책대로 허용되며, 대각선 기본 비용에 진입 타일 비용을 곱해 계산.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.

## 2026-06-07 - A* Path 실패 처리 보강

### 변경 내용
- `MovementSystem`의 경로 발행 헬퍼가 성공 여부를 반환하도록 정리하고, 직접 경로와 접근 경로가 모두 실패한 경우를 명시적으로 처리.
- 경로 실패 시 유닛을 정지시켜 이전 이동/순찰 상태가 남아 반복 재시도되는 상황을 막음.
- 실패 원인을 `start_out_of_bounds`, `goal_out_of_bounds`, `goal_static_blocked`, `goal_dynamic_blocked`, `no_path`로 구분해 콘솔 로그에 남기도록 추가.
- 맵 밖 목표는 주변 접근 셀로 보정하지 않고 실패로 처리해 명령 실패 동작을 명확히 함.
- `DEVELOPMENT_PLAN.md`의 Epic 3.1 추가 점검 항목 중 `Path 실패 처리`를 완료로 갱신.

### 동작 결과
- 유효한 경로는 기존처럼 A* 결과를 사용.
- 막힌 목표 주변에 접근 가능한 셀이 있으면 기존 접근 경로 보정이 유지.
- 경로와 접근 경로가 모두 실패하면 유닛이 안전하게 Idle 상태로 돌아가고 실패 로그가 남음.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.

## 2026-06-07 - Command Queue: Shift 우클릭 이동 예약

### 변경 내용
- `UnitOrder`/`OrderType` 구조와 `Unit` 내부 예약 큐를 추가해 이후 Attack/Gather/Build 예약으로 확장할 수 있는 명령 payload를 마련.
- `MoveCommand`와 스마트 우클릭용 `AttackCommand`에 append 플래그를 추가하고, `GameUIManager`가 Shift 눌림 상태를 보존해 우클릭 명령에 전달하도록 수정.
- Shift 우클릭이 빈 땅이나 아군/중립 접근 대상이면 선택 유닛의 Move waypoint 큐 뒤에 추가되도록 연결.
- 유닛이 Idle 상태가 되면 `GameLogicManager`가 다음 queued Move를 꺼내 `MovementSystem`의 기존 A* 이동 경로로 실행.
- 일반 Move/Attack/Gather/Build/AttackMove/Patrol 명령은 기존 큐를 비우고, Stop/Hold도 큐를 비운 뒤 즉시 상태를 전환하도록 정리.

### 동작 결과
- 일반 우클릭 이동은 기존처럼 즉시 명령을 바꾸고 예약 큐를 초기화.
- Shift 우클릭 이동은 현재 명령을 유지한 채 다음 이동 지점으로 예약.
- 현재 이동이 끝나 유닛이 Idle이 되면 예약된 다음 지점으로 자동 이동.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행했고 조기 종료 없이 유지되는 것을 확인한 뒤 종료.
- 한계: 이번 slice는 Move waypoint 큐가 대상이며, 적 공격/채집/건설의 Shift 예약 실행은 후속 Command Queue 확장 범위.

## 2026-06-07 - Patrol 기본 명령 연결

### 변경 내용
- `PatrolCommand` 라우터를 실제 게임 로직에 연결하고, `P` 핫키/HUD `Patrol` 버튼이 다음 우클릭 위치를 순찰 목적지로 발행하도록 구현.
- `Unit`에 순찰 시작점/도착점/다음 목적지 상태를 추가해 A↔B 왕복 이동을 유지.
- 순찰 중 적 발견 시 기존 공격 루프를 사용해 교전하고, 대상이 사망하면 남은 순찰 경로로 복귀하도록 연결.
- `MovementSystem`에 `issuePatrol()`을 추가해 일반 이동과 같은 A* 경로/충돌 회피를 순찰에도 적용.
- 순찰 중 충돌 회피가 발생해도 Patrol 상태와 순찰 루트가 보존되도록 회피 경로 주입을 보강.

### 동작 결과
- 선택 유닛 → `P` 또는 HUD `Patrol` → 우클릭 목적지: 현재 위치와 클릭 지점 사이를 왕복.
- 순찰 이동 중 적 유닛/건물을 발견하면 공격.
- 교전 대상이 사망하면 순찰을 재개.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행해 조기 종료 없이 유지되는 것을 확인.
- 한계: 실제 `P` 입력 후 왕복 순찰/교전 복귀 체감은 수동 플레이 검증 필요.

## 2026-06-07 - Hold Position 기본 전투 동작 연결

### 변경 내용
- 기존 `HoldPositionCommand`를 실제 전투 동작과 연결해 선택 유닛이 Hold 상태에서 위치를 고정하도록 유지.
- `Unit`에 Hold 전용 공격 루프를 추가. 사거리 안의 적만 공격하고, 적이 사거리 밖으로 나가면 추격하지 않고 타겟을 해제.
- Hold 중 피격되더라도 공격자가 사거리 안에 있을 때만 반격하고, 사거리 밖이면 위치를 유지.
- `GameLogicManager`가 매 tick Hold 유닛을 검사해 공격 사거리 안의 가장 가까운 적 유닛/건물을 배정.
- 결과 확정 후 Stop/Hold 명령도 입력 잠금 규칙을 따르도록 보강.

### 동작 결과
- 선택 유닛 → `H` 또는 HUD `Hold` → 유닛이 제자리에서 대기.
- 적이 해당 유닛의 공격 사거리 안에 들어오면 공격.
- 적이 사거리 밖으로 이동하거나 죽으면 추격하지 않고 Hold 상태 유지.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행해 조기 종료 없이 유지되는 것을 확인.
- 한계: 실제 `H` 입력 후 사거리 안/밖 전투 체감은 수동 플레이 검증 필요.

## 2026-06-07 - AttackMove 기본 명령 연결

### 변경 내용
- `AttackMoveCommand`를 `GameLogicManager` 라우터에 연결하고, 선택 유닛에게 공격이동 경로를 발행하도록 구현.
- `Unit`에 공격이동 목적지/활성 상태를 추가해 이동 중 적을 발견하면 교전하고, 대상이 사망하면 원래 목적지로 복귀하게 함.
- `MovementSystem`에 일반 이동과 같은 경로 탐색을 쓰는 `issueAttackMove()`를 추가.
- `GameUIManager`에서 `A` 핫키와 HUD `Attack` 버튼을 공격이동 모드로 연결. 기본 우클릭 스마트 명령은 기존 `AttackCommand` 흐름을 유지.
- 적 AI 웨이브가 직접 타운홀 타겟 공격 대신 AttackMove로 플레이어 기지까지 전진하도록 전환.

### 동작 결과
- 선택 유닛 → `A` 또는 HUD `Attack` → 우클릭 목적지: 유닛이 목적지로 이동하면서 반경 내 적 유닛/건물을 발견하면 공격.
- 공격 대상이 사망하면 남은 공격이동 목적지까지 다시 이동.
- 목적지 도착 후 추가 적이 없으면 Idle 상태로 종료.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1` 빌드 성공.
- `cmake-build-debug\RTS.exe`를 5초 동안 실행해 조기 종료 없이 유지되는 것을 확인.
- 한계: 수동 입력 기반 `A` 클릭/교전 복귀 플레이 검증은 아직 필요. 빌드와 런타임 연결 경로는 코드 레벨에서 확인.

## 2026-06-07 - Gold 자원: Gold Stone 단계별 스프라이트 + Highlight 애니메이션

### 변경 내용
- Gold 자원 노드의 스프라이트를 단일 `Gold_Resource.png`에서 **Gold Stone 더미(stage 1~6)** 로 교체.
- 남은 자원량 비율에 따라 단계 선택: `ceil(remaining/total * 6)`을 1~6로 clamp → 가득 차면 stage 6, 고갈에 가까울수록 stage 1로 더미가 줄어듦.
- 각 단계는 `Gold Stone N_Highlight.png`(768×128 = 6프레임 시트)를 사용해 **기본 상태에서 반짝이는 Highlight 애니메이션**(6프레임, 8fps)을 재생.
- Wood 자원은 기존 정적 스프라이트 유지.

### 구현
- `ResourceNodeViewModel`: `goldStoneStage()`로 단계 계산, Gold면 텍스처 id `210 + (stage-1)`에 sourceW/H=128·frameCount=6·fps=8로 DrawSprite emit. Wood는 종전대로.
- `SfmlRenderManager`: 텍스처 id 210~215를 `Gold Stones/Gold Stone {1..6}_Highlight.png`에 매핑(경로 배열). 사용하지 않게 된 `Gold_Resource.png`(200) 매핑 제거.

### 검증
- Gold Stone Highlight 6단계 파일 경로 존재 확인.
- `cmake.exe --build cmake-build-debug` 빌드 성공, `RTS.exe` 정상 구동.
- 한계: 실제 단계 전환/반짝임은 수동 플레이로 최종 확인 필요.

## 2026-06-07 - Fix: 유닛/건물/자원 스프라이트 미표시 (텍스처 매핑 누락)

### 문제
수동 플레이 검증 시 건물·자원이 화면에 보이지 않고, 모든 유닛이 파란 Warrior로만 표시됨.
- 원인: ViewModel들은 올바른 텍스처 id를 emit하지만(건물 300/301/310/311, 자원 200/201), `SfmlRenderManager::tinySwordsSpriteTexture()`의 경로 switch에는 **Blue Warrior(1~4)와 커서(100~103)만** 등록되어 있어 나머지는 `default → nullptr`로 렌더가 스킵됨.
- 추가로 `UnitViewModel`이 unitType·teamId를 무시하고 항상 Blue Warrior 스프라이트를 사용 → Worker/적 유닛 구분 불가.

### 변경 내용
- **SfmlRenderManager**: `tinySwordsSpriteTexture()` switch에 누락 텍스처 경로 추가.
  - 건물: Blue/Red Castle(TownHall)·Barracks → `Buildings/{Blue,Red} Buildings/{Castle,Barracks}.png`
  - 자원: Gold `Gold_Resource.png`, Wood `Wood Resource.png`
  - 유닛: Red Warrior(11~14), Blue Pawn(21~23), Red Pawn(31~33) 시트 경로
- **UnitViewModel**: `spriteClipFor(unitType, teamId, action)`로 확장.
  - Worker → Pawn 시트(Idle 8 / Run 6 / Interact 3, Guard 없음→Idle 폴백)
  - 그 외 전투 유닛 → Warrior 시트(Idle 8 / Run 6 / Attack 4 / Guard 6)
  - 아군(Player)=Blue, 적(Enemy)=Red 팀 컬러 구분
  - 텍스처 id 레이아웃을 SfmlRenderManager와 일치시킴(주석으로 문서화).

### 검증
- 참조한 10개 텍스처 파일 경로 존재를 파일시스템에서 확인.
- `cmake.exe --build cmake-build-debug` 빌드 성공, `RTS.exe` 정상 구동 확인.
- 한계: 실제 화면 표시는 수동 플레이로 최종 확인 필요(자동화 환경 캡처 불가). Archer/Lancer/Monk/Marine은 현재 Warrior 시트로 폴백.

### Follow-up
- 유닛 타입별 전용 스프라이트(Archer/Lancer 등), 건설 중 건물 반투명/스캐폴드 시각화.

## 2026-06-07 - Sprint 4: Simple Enemy AI + Victory/Defeat (Vertical Slice 마감)

### 변경 내용
- **GameWorld 게임 결과 상태**: `GameResult{InProgress, Victory, Defeat}` enum + `gameResult()`/`setGameResult()`. 기본 InProgress.
- **GameLogicManager 적 AI** (`updateAI`):
  - 생산: `kAiProduceInterval`(10s)마다 적 Barracks(완성·큐 비어있음)에 Warrior를 무료로 train(슬라이스 자가구동용, 비용 검사 생략).
  - 공격 웨이브: `kAiWaveInterval`(35s)마다 적 진영의 Idle 전투 유닛 전부를 플레이어 TownHall에 `attack` — 추격+교전은 기존 전투 FSM 재사용.
  - 적 유닛은 `registerBuildingSpawn` 콜백으로 생산되어 자동으로 월드에 추가됨.
- **승패 판정** (`checkVictoryDefeat`, 매 tick): `countTownHalls(team)`로 양 진영 TownHall 수 집계 → 플레이어 0이면 Defeat, 적 0이면 Victory.
- **입력 잠금**: 결과 확정 후 GameLogicManager의 6개 handle(Move/Attack/Gather/Train/Cancel/Build)이 `inputLocked()`로 조기 반환. GameUIManager의 `issueWorldOrder`·`handleGameplayInput`도 결과 시 입력 무시.
- **결과 배너**: GameUIManager `render()`에서 결과 확정 시 화면 중앙에 `DrawText`로 "VICTORY"(녹색)/"DEFEAT"(빨강) 표시. RenderCommand variant 확장 없이 기존 DrawText 재사용(GCC ICE 위험 회피).

### 동작 결과 — Vertical Slice 완성
- 일꾼 채집 → 자원으로 생산/건설 → 유닛 양성 → 적 AI가 35초 주기로 공격 웨이브 → 적/아군 TownHall 파괴로 승패 결정 → 화면에 결과 표시 + 입력 잠금까지, RTS 한 판의 전체 루프가 코드 레벨에서 연결됨.

### 검증
- `cmake.exe --build cmake-build-debug` 빌드 성공(ICE 없이 1회 통과).
- `RTS.exe` 실행 — 정상 구동 확인.
- 한계: 자동화 환경에서 35초 웨이브·승패까지 진행 관찰은 불가하여 게임플레이 결과(실제 웨이브 도달/배너 표시)는 수동 검증 필요. 로직 경로와 빌드/구동 안정성은 확인함.

### Follow-up
- 결과 화면 전용 씬/리스타트, 반투명 배경 오버레이.
- AI 생산 시 적 자원 차감(현재 무료), 적 일꾼 채집·확장 등 거시 AI.
- AttackMove 명령 도입 시 웨이브를 attack-target 대신 attack-move로 전환.

## 2026-06-07 - Sprint 3: Construction Loop (Placement, ConstructionSite, Worker Build FSM)

### 변경 내용
- **BuildingStaticData** (`include/core/data/BuildingStaticData.hpp`): 레거시 미사용 스텁을 교체. `buildingType`/`displayName`/`maxHp`/`footprintWidth,Height`(타일)/`buildTimeSeconds`/`goldCost`/`woodCost` + `cost()` 헬퍼. TownHall(4x4, 400g, 30s)·Barracks(3x3, 150g, 20s) 프리셋과 `buildingStaticDataFor()`.
- **Building 건설 상태**: `m_completed`/`m_buildTime`/`m_buildProgress` 추가. `beginConstruction(buildTime, startHp)`로 미완성 셸 시작, `advanceConstruction(dt)`로 진행(HP가 진행도에 비례해 상승, 완료 시 true 반환), `isComplete()`/`buildProgress01()`. 미완성 건물은 `tick()`에서 train 불가, `isDropOff()`·생산 비활성. `getAction()`은 미완성 시 `Build` 반환.
- **BuildCommand**: `buildingId` → `buildingTypeId`로 의미 명확화.
- **Unit Build FSM**: `ActionType::Build` 활용. `buildAt(Building* site)` 진입, `updateBuild(dt)`로 site까지 이동(`kBuildInteractRange=88`) 후 `advanceConstruction(dt)` 호출, 완료/사이트 소멸 시 Idle. `clearGatherState()`에 build target 취소를 통합해 모든 행동 전환점에서 채집/건설이 상호 배타적으로 정리되도록 함.
- **GameLogicManager 건설 로직**:
  - `handleBuildCommand()`: 선택 Worker 확인 → buildingType 해석 → 커서 타일 중심으로 footprint origin 계산 → `canPlaceBuilding()`(footprint 전 타일 walkable·비점유) → `canAfford` → 자원 차감 → `beginConstruction`한 Building 생성·`registerBuildingSpawn`·`addElement` → Worker `buildAt()`.
  - `canPlaceBuilding(originX,originY,w,h)`: PlacementValidator. 맵 경계(isTileBlocked가 경계 포함)·타일 walkability·셀 점유 검사.
  - `firstSelectedWorker()`: 선택 목록 첫 Worker.
  - `BuildCommand` 라우터 등록.
- **GameUIManager**: `WorldOrderMode::Build` 추가. `B` 핫키(기존 매핑)로 Build 모드 진입 → 다음 우클릭 위치에 기본 건물(Barracks) `BuildCommand` 발행. 배치 후 모드 리셋.
- 완성 시 `registerBuildingSpawn`이 이미 연결돼 있어 production/drop-off가 자동 활성화됨.

### 동작 결과
- Worker 선택 → `B` → 우클릭 → 배치 가능·자원 충분 시 미완성 건물 생성·자원 차감 → Worker가 이동해 건설 → 완료 시 완성 건물 전환(생산·자원 반납 가능).
- 잘못된 위치(막힘/점유/경계 밖)나 자원 부족 시 건설 거부.

### 검증
- `cmake.exe --build cmake-build-debug` 빌드 성공. (참고: GameUIManager.cpp에서 GCC 16.1.0의 std::variant 소멸자 관련 ICE(Segmentation fault)가 1회 발생했으나 동일 명령 재실행으로 통과 — 컴파일러 비결정적 버그이며 소스 문제 아님.)
- `RTS.exe` 실행 — 정상 구동 확인.
- 한계: 자동화 환경에서 키 입력·선택 시뮬레이션 불가로 in-game 건설 흐름은 수동 검증 필요.

### Follow-up
- Build Preview(배치 전 초록/빨강 프리뷰 렌더링), 건설 중 건물 진행도 UI.
- footprint를 그리드 walkability에 반영(Epic 5.3) — 현재는 단일 점유 셀 기준.
- 건물 타입 선택 UI(현재 B = Barracks 고정).

## 2026-06-07 - Sprint 2: Production Loop (Cost, Queue, Spawn Placement, RallyPoint)

### 변경 내용
- **Cost 시스템**: `PlayerResourceState.hpp`에 `Cost{gold,wood,food}` 구조체와 `canAfford()`/`pay()`/`refund()` 메서드 추가. food는 `foodUsed`/`foodCapacity` 인구 예약으로 검사. `UnitStaticData`에 `cost()` 변환 헬퍼 추가.
- **Building 확장**: RallyPoint(`setRallyPoint`/`rallyPoint`/`hasRallyPoint`) 추가. `cancelLastTrain()`이 취소된 `UnitType`을 `std::optional`로 반환하도록 변경(환불용). `UnitSpawnFn` 시그니처를 `(type, anchor, rally, hasRally, team)`로 확장하고 tick의 스폰 호출에 rally 정보 전달.
- **GameLogicManager 생산 로직**:
  - `registerBuildingSpawn()`: 디버그 건물 3개(아군 TownHall, 적 TownHall, 적 Barracks)에 spawn 콜백 연결. 콜백은 즉시 생성하지 않고 `m_pendingSpawns`에 버퍼링.
  - `flushPendingSpawns()`: `tick()`의 요소 순회가 끝난 뒤 호출 — 순회 중 `addElement`로 인한 elements 벡터 무효화를 방지. 여기서 스폰 위치 계산 + 유닛 생성 + RallyPoint 이동 발행.
  - `findFreeSpawnPosition()`: 건물 앵커 주변을 링 단위로 확장하며 walkable·비점유 타일 탐색(최대 반경 6), 없으면 앵커 반환.
  - `handleTrainCommand()`: 선택된 건물 기준(buildingId -1) → 유닛 타입 결정(unitTypeId -1이면 건물 기본 유닛: TownHall→Worker, Barracks→Warrior) → 큐 여유·`canAfford` 검사 → `trainUnit` → `pay`.
  - `handleCancelProduction()`: 선택 건물의 마지막 큐 항목 취소 + 비용 환불.
  - `handleMoveCommand()`: 선택된 게 건물이면 이동 대신 RallyPoint 설정.
  - `TrainUnitCommand`/`CancelProductionCommand` 라우터 등록.
- **GameUIManager**: `T` 키를 `TrainUnit` 핫키로 매핑. `TrainUnit` 액션이 `TrainUnitCommand(-1,-1)`(선택 건물 + 기본 유닛)을 발행하도록 연결(기존 빈 처리에서 변경).

### 동작 결과
- 건물 선택 후 `T` → 자원/인구 충분 시 비용 차감하고 생산 큐에 추가, 시간 경과 후 건물 옆 빈 타일에 유닛 스폰.
- 건물 선택 후 우클릭 → RallyPoint 설정. 생산 완료 유닛이 RallyPoint로 자동 이동.
- `Esc`(CancelProduction) → 마지막 큐 항목 취소 + 비용 환불.
- 자원/인구 부족 또는 큐(최대 5) 가득 시 생산 거부.

### 검증
- `cmake.exe --build cmake-build-debug` 빌드 성공(오류 없음).
- `RTS.exe` 실행 — 정상 구동(exit code 0, 크래시 없음) 확인.
- 한계: 자동화 환경에서 키 입력·건물 선택 시뮬레이션이 어려워 in-game train 흐름은 수동 검증 필요. 펜딩 스폰 버퍼링으로 iterator 무효화 위험은 코드 레벨에서 제거함.

### Follow-up
- HUD 명령 카드에 건물 타입별 생산 가능 유닛 버튼 노출(현재는 핫키 + 기본 유닛만).
- 생산/취소 시 사운드·실패 피드백, RallyPoint UI 표시.
- 향후 건설된(런타임 생성) 건물에도 `registerBuildingSpawn` 자동 연결 필요.

## 2026-06-07 - Sprint 1: Worker Gather Redirect (MaxGatherers & Drop-off Rebuild)

### 변경 내용
- `GatherPhase`에 `NeedNewResource`, `NeedNewDropOff` 두 상태 추가.
- `updateGather()` 수정: drop-off가 파괴되면서 자원을 운반 중일 때 Idle 대신 `NeedNewDropOff`로 전환. MoveToResource 단계에서 슬롯 예약 실패 시 Idle 대신 `NeedNewResource`로 전환.
- `takeReadyResourceDelivery()` 수정: 다음 채집 사이클 시작 실패 시 Idle 대신 `NeedNewResource`로 전환하고 `carryingType`을 보존.
- `Unit` 공개 API 추가: `isNeedingResourceRedirect()`, `isNeedingDropOffRedirect()`, `targetGatherType()`, `redirectToDropOff(Building*)`.
- `GameLogicManager`에 `handleGatherRedirects()` 추가: 매 tick 워커를 순회하여 redirect 상태인 워커에게 같은 타입의 빈 슬롯이 있는 가장 가까운 자원 또는 새 drop-off를 자동 재배정.
- `findClosestAvailableResource()` 추가: 자원 타입별 슬롯 여유가 있는 가장 가까운 ResourceNode 탐색.

### 동작 결과
- Worker가 maxGatherers(3)가 꽉 찬 자원 노드로 이동할 때 → 같은 타입의 다른 자원 노드로 자동 재배정.
- Worker가 자원을 운반하다가 drop-off(TownHall)가 파괴되면 → 가장 가까운 다른 drop-off 건물로 자동 전환.
- 두 경우 모두 대안이 없으면 Worker는 Idle로 정지.

### 검증
- `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug` 빌드 성공 (오류 없음).

## 2026-06-07 - Unit Static Data Runtime Stats

- Reworked `UnitStaticData` into a runtime-ready unit stat model with display name, HP, attack damage, range, cooldown, move speed, armor, and basic economy costs.
- Added default Warrior, Archer, Worker, and Marine stat presets, and changed `Unit` construction to apply static data instead of hardcoded combat stats.
- Added armor mitigation to unit damage handling and exposed unit stat getters for UI snapshots.
- Updated the selection HUD snapshot and overlay so selected units show their real name, attack, armor, range, HP, action, and position.
- Marked `DEVELOPMENT_PLAN.md` Phase 1-1 as complete while leaving factory/type registry integration under Phase 2-1.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.

## 2026-06-07 - Cursor State Variation

- Implemented dynamic cursor state variations for different UI contexts (`Cursor_02.png` for attack, `Cursor_03.png` for dragging, `Cursor_04.png` for move).
- Updated `GameUIManager` to track drag state via mouse events and determine the correct cursor texture ID based on order mode (Move vs Attack) and hovering enemy units.
- Added `distanceTo` to `Vector2D` to support distance calculations when hovering over enemies.
- Added `UpdateHudCursor` to `RenderCommandData` to propagate cursor state to `SfmlRenderManager`.
- Verification: User requested to verify the build manually.

## 2026-06-06 - Team-Aware Right Click Attack Orders

- Added team ownership to gameplay elements and debug units so command resolution can distinguish local player units from enemy units.
- Updated right-click attack command handling to first inspect the clicked unit: same-team or neutral targets now resolve as move/approach orders, while opposing-team targets resolve as attacks.
- Split unit command action from render animation action so attack-chasing units keep the run animation until the target enters weapon range, then switch to the attack animation while dealing damage.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.

## 2026-06-06 - StarCraft Command Hotkeys

- Added StarCraft-style gameplay command hotkeys in `GameUIManager`: Move `M`, Attack `A`, Stop `S`, Hold `H`, Patrol `P`, Gather `G`, Build `B`, Repair `R`, and Cancel `Escape`.
- Routed hotkeys through the same `GameplayInputCommand` handling path as the HUD command buttons so keyboard and UI command input stay aligned with the current `LogicCommand` bridge.
- Preserved modifier combinations such as control-group shortcuts by ignoring gameplay command hotkeys when Ctrl, Alt, or System modifiers are held.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.

## 2026-06-07 - Temporary Resource Fixture Cleanup

- Adjusted the temporary debug world so player and enemy resource states are explicitly initialized instead of relying on default HUD placeholder values.
- Repositioned the temporary Worker, Town Hall, and ResourceNode fixtures so gathering can be tested quickly, and added an enemy Town Hall so enemy building/resource ownership is represented consistently.
- Fixed command target picking so a ResourceNode that was included in drag selection can still be used as the gather target for selected Workers.
- Added total resource amount tracking to `ResourceNode` and used it in the selection HUD so resource remaining and resource capacity are no longer the same value.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: move the temporary fixture setup into a scenario/test-map loader once production/building placement begins owning initial world state.

## 2026-06-07 - Worker Resource Gathering Loop

- Implemented the Sprint 1 resource gathering loop from `DEVELOPMENT_PLAN.md`: workers can gather from `ResourceNode`, carry resources to a friendly Town Hall drop-off, and increase `PlayerResourceState`.
- Extended `ResourceNode` with gather amount, gather duration, max gatherers, depletion checks, and reservation tracking so gather orders have real resource state to operate on.
- Added Worker gather state to `Unit`, including resource/drop-off targets, carrying data, gather phases, reservation release on command changes/death, and automatic repeat gathering after delivery.
- Wired smart right-click and `GatherCommand` world-target input through `GameLogicManager`, filtering non-workers and routing ready deliveries back into `GameWorld` resources.
- Updated `DEVELOPMENT_PLAN.md` to mark the completed resource-gathering tasks and leave the remaining stabilization items visible.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: add behavior for MaxGatherers overflow, Drop-off destruction retargeting, and targeted tests/debug traces for the gather loop.

## 2026-06-06 - Render Lock Reentry Fix

- Fixed a render-thread stall where `GameUIManager::render()` tried to acquire `GameWorld`'s read lock while `GameScene::render()` already held the same non-recursive `shared_mutex` read lock.
- Kept scene-level locking as the owner for UI sync/update/render world reads and removed the nested HUD snapshot lock from `GameUIManager::render()`.
- Added comments in `GameScene` to make the single-lock ownership clear for future HUD/world read changes.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: longer term, HUD/world snapshots should be collected through an explicit read-only scene snapshot API so render code does not depend on implicit outer locks.

## 2026-06-06 - Selected Unit HUD Details

- Added `UpdateHudSelection` render data so the HUD selection panel can receive the current selected-unit snapshot from gameplay UI code.
- `GameUIManager` now scans selected live units while rendering, sends selected count plus the first selected unit's name, action, HP, and world position to the HUD.
- Replaced hardcoded selection-panel text in `SfmlHudOverlay` with the selected unit snapshot while preserving the existing portrait and command panel layout.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: unit stats such as armor, damage, range, and display name should move into real unit/static data getters before the panel can show a full StarCraft-style stat card.

## 2026-06-06 - Player Resource HUD State

- Added `PlayerResourceState` as the shared model for player economy values: gold, wood, food usage/capacity, and army.
- Added player-id keyed resource storage to `GameWorld`, initialized the local player resource state, and exposed read/update APIs for future economy systems.
- Added `UpdateHudResources` render data so `GameUIManager` can forward the local player's current resource snapshot through `RenderQueue` each frame.
- Replaced hardcoded SFML HUD resource numbers with values from the resource snapshot while preserving the existing top-right resource pill layout and number formatting.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: economy commands still need to call `GameWorld::setPlayerResources(...)` or a future resource system when gathering, spending, supply, and army counts change.

## 2026-06-06 - Movement Stops At Occupied Targets

- Updated movement collision handling so a moving unit stops when the blocker is occupying the unit's final requested target instead of repeatedly generating avoidance paths around it.
- Added an approach-path fallback for occupied goals: if the exact clicked cell cannot be pathfound because it is blocked by a unit or future gameplay element, the mover paths to a nearby free cell and stops there.
- Broadened runtime move-blocker checks from `Unit` only to any live `IGameElement`, keeping the collision contract ready for building-like gameplay elements.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: building entities still need a runtime `IGameElement` representation and a size/radius source before buildings can block movement with building-specific footprints.

## 2026-06-06 - HUD Gameplay Input Commands

- Added `GameplayInputCommand` and `GameplayInputAction` so HUD/gameplay UI inputs have a command layer aligned with the current `LogicCommand` action set.
- Wired the SFML HUD command buttons to emit gameplay UI commands through `UICommandBus`, then translated supported inputs in `GameUIManager`: Move and Attack set the next world-click order, while Stop, Hold, and Cancel forward matching logic commands.
- Updated `SOURCE_STRUCTURE.md` to note that `SfmlHudOverlay` now publishes gameplay UI input and `GameUIManager` owns the UI-to-logic translation.
- Verification: built `RTS` with `C:\Program Files\JetBrains\CLion 2026.1.2\bin\cmake\win\x64\bin\cmake.exe --build cmake-build-debug --target RTS -- -j1`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: Patrol, Train, Build, Gather, Return, Repair, and Ability UI actions are represented as UI input commands, but they still need selected unit, target, building, resource, or ability payloads before they can create complete logic commands.

## 2026-06-05 - Repair Logic Command

- Added `RepairCommand` so the logic command set now covers the HUD command list's repair action.
- Verification: built `RTS` with the CLion-bundled CMake/Ninja using `C:\msys64\ucrt64\bin` on `PATH`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: repair still needs gameplay handling once workers, repairable targets, and command UI dispatch are implemented.

## 2026-06-05 - HUD Selected Unit Portrait

- Added a selected-unit HUD portrait path that reuses the chosen unit's current sprite texture, animation frame, and transparent-trim bounds.
- The SFML renderer now scans world sprite commands for the selected unit, draws the ImGui HUD, then overlays the unit sprite inside the existing selection portrait panel before drawing the custom cursor.
- Verification: built `RTS` with the CLion-bundled CMake/Ninja using `C:\msys64\ucrt64\bin` on `PATH`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: the HUD text is still placeholder data and can be wired to real selection stats next.

## 2026-06-05 - Sprite Mouse Cursor

- Replaced the native OS cursor with a Tiny Swords cursor sprite rendered by the SFML renderer.
- The cursor now uses `UI Elements/UI Elements/Cursors/Cursor_01.png`, is drawn after the HUD so it remains visible over UI, and applies a hotspot offset so the arrow tip tracks the actual mouse position.
- Verification: built `RTS` with the CLion-bundled CMake/Ninja using `C:\msys64\ucrt64\bin` on `PATH`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: cursor state variants such as attack/selection can be mapped later to `Cursor_02.png` through `Cursor_04.png`.

## 2026-06-05 - Unit Attack Orders And Combat Tick

- Added right-click attack intent: game UI now sends an attack order with the clicked world position, and game logic attacks a living unselected unit near that point or falls back to normal movement on empty ground.
- Implemented selected-unit attack dispatch, attack target picking, attack chase, cooldown damage, retaliation, and dead-unit command filtering.
- Dead units are ignored by selection/collision and no longer render as active units after death.
- Verification: built `RTS` with the CLion-bundled CMake/Ninja using `C:\msys64\ucrt64\bin` on `PATH`, then launched `cmake-build-debug\RTS.exe` and confirmed it stayed running for 5 seconds.
- Follow-up: team ownership is still absent, so "enemy" currently means any living game element that is not part of the current selection.

## 2026-06-05 - Source Structure Documentation Rule

- Updated `AGENTS.md` to require source-structure documentation updates when modules, folders, entry points, runtime systems, or build/runtime ownership changes.
- Added `SOURCE_STRUCTURE.md` with the current top-level layout, core/game/platform layer responsibilities, build entry points, and maintenance notes.
- Verification: reviewed repository file layout with `rg --files`, inspected `CMakeLists.txt`, and left unrelated local source changes untouched.

## 2026-06-05 - Shared AI Documentation Rule

- Updated `AGENTS.md` to require Markdown development notes for completed tasks.
- Future agents should record what changed, why it changed, how it was verified, and any remaining follow-up.
- Verification: reviewed the `AGENTS.md` diff and left unrelated local changes untouched.
