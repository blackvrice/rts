# RTS 개발 마스터 로드맵 상세 분할안

> 본 문서는 `RTS_GAME_BLUEPRINT.md` 아키텍처를 기준으로 RTS 게임 개발 로드맵을 실제 개발 티켓 단위까지 세분화한 문서입니다.  
> 각 항목은 GitHub Issue, GitHub Project, Notion, Jira 등에 바로 옮길 수 있도록 **Epic → Feature → Task → 완료 기준** 형태로 구성합니다.

---

# 0. 문서 목적

본 문서의 목적은 다음과 같습니다.

```text
1. 현재 개발 상태를 명확히 정리한다.
2. 앞으로 구현할 기능을 작은 작업 단위로 분할한다.
3. 우선순위가 높은 Vertical Slice를 먼저 완성한다.
4. RTS 핵심 루프를 실제 런타임에서 동작하게 만든다.
5. 추후 AI, Fog of War, Save/Replay, Network 확장 기반을 마련한다.
```

---

# 1. 전체 우선순위

현재 가장 중요한 개발 순서는 다음과 같습니다.

```text
1. 자원 채집 루프 완성
2. 자원 소비 + 생산 큐 완성
3. 건물 배치 + 건설 루프 완성
4. 단순 AI 완성
5. 승패 조건 완성
6. Command Queue / AttackMove / Hold 추가
7. Fog of War / Minimap / Command Card 추가
8. Save / Replay / World Hash 추가
```

현재 목표는 모든 RTS 기능을 한 번에 만드는 것이 아니라,  
**작은 RTS 한 판이 끝까지 플레이되는 Vertical Slice**를 먼저 완성하는 것입니다.

---

# 2. 상태 표기 기준

```text
[100%] : 기능이 실제 런타임 동작으로 연결되어 사용 가능
[1~99%] : 일부 모델, 명령, UI, 렌더링은 있으나 전체 로직이 연결되지 않음
[0%] : 아직 구현이 없거나 핵심 시스템에 연결되지 않음
```

---

# 3. Phase 0. Vertical Slice

## 목표

```text
일꾼이 자원을 캐고,
건물을 짓고,
병사를 생산하고,
적 기지를 공격해서,
승리/패배가 발생하는 최소 RTS 완성
```

---

## Epic 0.1 유닛 기본 제어

현재 상태: `[100%]`

### 완료된 항목

```text
- 마우스 드래그 선택
- 단일 선택
- 우클릭 이동
- 선택 UI 동기화
- 기본 부대 지정
```

### 추가 점검 Task

```text
[x] 선택된 유닛 사망 시 선택 목록에서 자동 제거
[ ] 선택 중인 유닛이 맵 밖/비활성 상태가 되면 선택 해제
[ ] 드래그 선택 시 건물/유닛 우선순위 적용
[ ] 같은 타입 더블클릭 선택 준비
[ ] Ctrl + 번호 부대 지정 안정성 테스트
```

### 완료 기준

```text
- 유닛 1기 선택 가능
- 유닛 다중 선택 가능
- 선택된 유닛만 명령을 받음
- 죽은 유닛은 더 이상 선택/명령 불가
```

---

## Epic 0.2 기본 전투

현재 상태: `[100%]`

### 완료된 항목

```text
- 사거리 내 적 인지
- 적 추격
- 데미지 적용
- HP 감소
- 사망 처리
- 기본 공격 애니메이션 연동
```

### 추가 점검 Task

```text
[x] 공격 대상이 죽었을 때 다음 대상 탐색
[ ] 공격 대상이 시야/사거리 밖으로 나갔을 때 추격/중단 처리
[ ] 공격 불가능 대상 필터링
[ ] 아군 오인 공격 방지
[ ] 사망한 유닛의 충돌 제거
[ ] 사망한 유닛의 렌더 오브젝트 제거
[ ] 공격 중 Stop 명령 처리
[ ] 공격 중 Move 명령 처리
```

### 완료 기준

```text
- 유닛 A가 유닛 B를 공격 가능
- HP가 0 이하가 되면 사망
- 사망한 유닛은 더 이상 타겟팅되지 않음
- 전투 중 이동/정지 명령이 정상 작동
```

---

## Epic 0.3 자원 채집 루프

현재 상태: `[95% — 엣지케이스(EntityId 핸들·MaxGatherers 재탐색·drop-off 재탐색) 완료 / 전용 디버그 로그만 후속]`

### 현재 구현된 항목

```text
- ResourceNode 모델 존재
- ResourceNode 렌더링 존재
- ResourceNode 자원량/채집량/채집 시간/예약 데이터 존재
- WorkerGatherState 기초 구현
- 자원 우클릭/GatherCommand 런타임 연결
- Worker Gather FSM 기초 구현
- Drop-off 탐색 및 PlayerResource 증가 연결
- 자동 반복 채집 기초 구현
```

### 남은 안정화 항목

```text
- (완료) EntityId 핸들 기반 gather 대상 참조 (targetResourceId/targetDropOffId, Epic 1.3)
- (완료) MaxGatherers 초과/예약 실패 시 다른 자원 재탐색 (handleGatherRedirects: NeedNewResource)
- (완료) Drop-off 건물 파괴 시 재탐색 (NeedNewDropOff → findClosestDropOffFor)
- 채집 루프 전용 테스트/디버그 로그 (후속)
```

---

### Feature 0.3.1 ResourceNode 데이터 확장

#### Task

```text
[x] ResourceType 추가
[x] ResourceAmount 추가
[x] GatherAmountPerTrip 추가
[x] GatherDurationTick 추가
[x] MaxGatherers 추가
[x] ReservedWorkers 목록 추가
[x] IsDepleted 상태 추가
```

#### 예시 구조

```cpp
struct ResourceNode
{
    EntityId id;
    ResourceType type;
    int amount;
    int gatherAmountPerTrip;
    int gatherDurationTick;
    int maxGatherers;
    std::vector<EntityId> reservedWorkers;
};
```

#### 완료 기준

```text
- 자원 노드가 남은 자원량을 가진다.
- 자원량이 0이 되면 고갈 상태가 된다.
- 고갈된 자원은 채집 대상이 될 수 없다.
```

---

### Feature 0.3.2 Worker 자원 운반 데이터 추가

#### Task

```text
[x] Worker가 현재 운반 중인 자원 타입 저장
[x] Worker가 현재 운반 중인 자원량 저장
[x] Worker의 최대 운반량 저장
[x] Worker가 채집 중인 ResourceNode 저장
[x] Worker가 돌아갈 Drop-off 건물 저장
```

#### 예시 구조

```cpp
struct WorkerGatherState
{
    ResourceType carryingType;
    int carryingAmount;
    int maxCarryAmount;

    EntityId targetResource;
    EntityId targetDropOff;

    int gatherProgressTick;
};
```

#### 완료 기준

```text
- Worker가 자원을 들고 있는지 알 수 있다.
- Worker가 어떤 자원을 채집 중인지 알 수 있다.
```

---

### Feature 0.3.3 GatherCommand 구현

#### Task

```text
[x] 자원 노드 우클릭 시 GatherCommand 생성
[x] 선택된 유닛 중 Worker만 필터링
[x] Worker가 아니면 명령 무시
[x] 고갈된 자원 노드면 명령 거부
[x] ResourceNode까지 이동 명령 연결
```

#### 완료 기준

```text
- Worker를 선택하고 자원을 우클릭하면 자원 쪽으로 이동한다.
- 전투 유닛은 GatherCommand를 받지 않는다.
```

---

### Feature 0.3.4 Worker Gather FSM 구현

#### 상태 흐름

```text
Idle
 ↓
MoveToResource
 ↓
ReserveGatherSlot
 ↓
Gathering
 ↓
CarryResource
 ↓
MoveToDropOff
 ↓
DropResource
 ↓
MoveToResource
```

#### Task

```text
[x] MoveToResource 상태 추가
[x] ReserveGatherSlot 상태 추가
[x] Gathering 상태 추가
[x] CarryResource 상태 추가
[x] MoveToDropOff 상태 추가
[x] DropResource 상태 추가
[x] 자동 반복 채집 처리
```

#### 완료 기준

```text
- Worker가 자원으로 이동한다.
- 일정 시간 채집한다.
- 자원을 들고 타운홀로 돌아간다.
- 타운홀 도착 시 PlayerResource가 증가한다.
- 다시 자원으로 이동한다.
```

---

### Feature 0.3.5 Drop-off 건물 탐색

#### Task

```text
[x] Drop-off 가능한 건물 타입 정의
[x] 완성된 아군 건물만 후보로 인정
[x] 가장 가까운 Drop-off 건물 탐색
[x] Drop-off 건물이 파괴되었을 때 재탐색
[x] Drop-off 건물이 없으면 Worker Idle 처리
```

#### 완료 기준

```text
- Worker가 가장 가까운 타운홀로 자원을 반납한다.
- 타운홀이 파괴되면 다른 Drop-off를 찾는다.
- Drop-off가 없으면 채집을 중단한다.
```

---

### Feature 0.3.6 자원 예약 시스템

#### Task

```text
[x] ResourceNode별 예약 Worker 목록 관리
[x] MaxGatherers 초과 시 다른 자원 탐색 또는 대기
[x] Worker 사망/명령 변경 시 예약 해제
[x] ResourceNode 고갈 시 예약 전체 해제
```

#### 완료 기준

```text
- 너무 많은 Worker가 하나의 자원 노드에 몰리지 않는다.
- 명령 취소 시 예약이 해제된다.
```

---

## Epic 0.4 유닛 생산 루프

현재 상태: `[95% — 생산/비용/스폰/RallyPoint 로직 완료(방향 우선·공간없으면 대기·적 rally면 AttackMove) / HUD 명령카드·사운드·RallyPoint UI만 후속]`

### 현재 구현된 항목

```text
- Building 모델 존재
- HUD 연동 일부 존재
- 생산 큐 (Building train queue, 최대 5)
- 비용 차감 / 환불 (Cost + PlayerResourceState canAfford/pay/refund)
- 인구(food) 검사
- 생산 진행도 Tick 처리
- 유닛 스폰 위치 계산 (건물 주변 빈 타일 링 탐색)
- RallyPoint 설정(건물 우클릭) + 생산 완료 유닛 자동 이동
- T 핫키로 선택 건물 기본 유닛 생산
```

### 남은 항목

```text
- (완료) HUD 명령 카드에 생산 버튼 노출 (건물 선택 시 Train, Epic 6.4)
- 생산/취소 사운드 및 자원 부족 피드백
- RallyPoint UI 표시
- 런타임 건설 건물에 spawn 콜백 자동 연결
```

---

### Feature 0.4.1 생산 가능 건물 데이터 추가

#### Task

```text
[x] BuildingStaticData에 생산 가능 유닛 목록 추가 (produces, Epic 1.2)
[x] ProductionQueueSize 추가 (Building::kMaxTrainQueue = 5)
[~] RallyPoint 기본값 추가 (런타임 설정 지원 / 정적 기본값은 미사용)
[x] 생산 시간 데이터 연결 (UnitStaticData.buildTimeSeconds → Building::currentTrainTime)
```

#### 예시 구조

```cpp
struct BuildingStaticData
{
    BuildingTypeId id;
    std::vector<UnitTypeId> producibleUnits;
    int maxQueueSize;
};
```

#### 완료 기준

```text
- 특정 건물이 어떤 유닛을 생산할 수 있는지 데이터로 알 수 있다.
```

---

### Feature 0.4.2 ProductionQueue 구현

#### Task

```text
[x] ProductionQueueComponent 추가 (Building 내장 m_trainQueue로 구현)
[x] ProductionItem 구조 추가 (UnitType deque + 단일 진행 타이머로 단순화)
[x] Queue 삽입 함수 추가 (Building::trainUnit)
[x] Queue 취소 함수 추가 (Building::cancelLastTrain)
[x] 현재 생산 중인 항목 진행도 증가 (Building::tick m_trainTimer)
[x] 생산 완료 이벤트 발생 (spawnFn 콜백 호출)
```

#### 예시 구조

```cpp
struct ProductionItem
{
    UnitTypeId unitType;
    int progressTick;
    int requiredTick;
};

struct ProductionQueue
{
    std::deque<ProductionItem> items;
    int maxQueueSize;
};
```

#### 완료 기준

```text
- 건물에 생산 명령을 넣으면 큐에 쌓인다.
- 시간이 지나면 생산이 완료된다.
```

---

### Feature 0.4.3 생산 비용 처리

#### Task

```text
[x] UnitStaticData에 Gold/Wood/Supply 비용 추가 (goldCost/woodCost/foodCost + cost())
[x] 생산 시작 전 자원 검사 (canAfford)
[x] 자원 부족 시 생산 거부
[x] 생산 큐 추가 시 비용 즉시 차감 (pay)
[x] 생산 취소 시 환불 (refund)
[x] 인구수 부족 시 생산 거부 (canAfford가 foodUsed+cost <= foodCapacity 검사)
```

#### 완료 기준

```text
- 자원이 부족하면 유닛 생산이 되지 않는다.
- 생산 시 자원이 차감된다.
- 취소 시 자원이 환불된다.
```

---

### Feature 0.4.4 유닛 스폰 위치 계산

#### Task

```text
[x] 건물 footprint 주변 타일 검색 (findFreeSpawnPosition 링 탐색)
[x] 이동 가능한 타일 필터링 (isTileBlocked/isCellOccupied)
[x] RallyPoint 방향 우선 배치 (링 내 rally 최근접 빈 타일 선택)
[x] 스폰 위치가 없으면 생산 완료 대기 (findFreeSpawnPosition nullopt → m_pendingSpawns 재큐)
[x] 스폰 후 충돌 등록 (addElement → onCollisionChanged)
```

#### 완료 기준

```text
- 생산 완료된 유닛이 건물 주변 빈 공간에 생성된다.
- 건물 내부나 막힌 타일에는 생성되지 않는다.
```

---

### Feature 0.4.5 RallyPoint

#### Task

```text
[x] 건물 선택 후 우클릭으로 RallyPoint 설정
[ ] RallyPoint UI 표시
[x] 생산 완료 유닛에게 MoveCommand 자동 발행 (flushPendingSpawns에서 moveTo)
[x] RallyPoint가 적이면 AttackMove 처리 (isEnemyNear → issueAttackMove)
```

#### 완료 기준

```text
- 생산된 유닛이 RallyPoint로 자동 이동한다.
```

---

## Epic 0.5 건설 루프

현재 상태: `[90% — Build Preview·건설 진행도 UI 완료 / footprint walkability(5.3)·건물 타입 선택 UI는 후속]`

### 현재 구현된 항목

```text
- BuildCommand (buildingTypeId + position)
- BuildingStaticData (footprint/cost/buildTime/maxHp)
- 배치 가능 여부 검사 (canPlaceBuilding: footprint walkable·비점유)
- 자원 차감 (canAfford/pay)
- ConstructionSite (Building beginConstruction/advanceConstruction)
- Worker 건설 FSM (buildAt → updateBuild)
- 완성 건물 전환 (advanceConstruction 완료 시 production/drop-off 활성화)
- B 핫키 → 우클릭 배치 (Barracks 고정)
```

### 남은 항목

```text
- (완료) 건물 배치 프리뷰 (초록/빨강 footprint 고스트 — 0.5.2)
- (완료) 건설 중 진행도 UI (BuildingViewModel 청록 진행도 바)
- footprint를 그리드 walkability에 반영 (Epic 5.3, 현재 단일 점유 셀 기준)
- 건물 타입 선택 UI (현재 B = Barracks 고정)
```

---

### Feature 0.5.1 BuildCommand 세분화

#### Task

```text
[x] BuildCommand에 BuildingTypeId 포함
[x] BuildCommand에 targetTile 포함 (position → 커서 타일 변환)
[x] Worker만 BuildCommand 가능하게 제한 (firstSelectedWorker)
[x] 건설 비용 검사 (canAfford)
[x] 건설 위치 검사 (canPlaceBuilding)
```

#### 완료 기준

```text
- Worker에게 특정 위치에 특정 건물을 짓는 명령을 내릴 수 있다.
```

---

### Feature 0.5.2 Build Preview

#### Task

```text
[x] 건설 버튼 클릭 시 프리뷰 모드 진입 (B → Build 모드, render()가 프리뷰 emit)
[x] 마우스 위치를 Grid 좌표로 변환 (worldToGrid + footprint origin)
[x] 배치 가능하면 초록색 표시 (canPlaceBuilding 미러)
[x] 배치 불가능하면 빨간색 표시
[x] ESC로 취소 (우클릭은 배치 — 기존 인터랙션 유지)
```

#### 완료 기준

```text
- 건설 전 배치 위치를 시각적으로 확인할 수 있다. ✅
```

---

### Feature 0.5.3 PlacementValidator

#### Task

```text
[x] 맵 경계 밖인지 검사 (isTileBlocked가 경계 포함)
[x] 타일이 이동 가능한지 검사 (isTileBlocked moveCost)
[x] 다른 건물 footprint와 겹치는지 검사 (isCellOccupied, 단일 점유 셀 기준)
[x] 자원 노드와 겹치는지 검사 (isCellOccupied)
[x] 유닛과 겹치는지 검사 (isCellOccupied)
[ ] 지형 타입이 건설 가능한지 검사 (현재 walkability로 근사)
[ ] 필요한 경우 아군 건물 근처인지 검사
```

#### 완료 기준

```text
- 잘못된 위치에는 건물을 지을 수 없다.
```

---

### Feature 0.5.4 ConstructionSite 생성

#### Task

```text
[x] 건설 명령 승인 시 자원 차감 (pay)
[x] ConstructionSite 엔티티 생성 (beginConstruction한 Building)
[x] 임시 건물 렌더링 (건설 중 청록 진행도 바 + HP 램프 시각화)
[x] HP를 낮은 상태로 시작 (startHp = maxHp * 0.1)
[x] 진행도 progressTick 관리 (m_buildProgress)
```

#### 예시 구조

```cpp
struct ConstructionComponent
{
    BuildingTypeId buildingType;
    int progressTick;
    int requiredTick;
    std::vector<EntityId> builders;
};
```

#### 완료 기준

```text
- 건설 시작 시 미완성 건물이 맵에 생성된다.
```

---

### Feature 0.5.5 Worker Build FSM

#### 상태 흐름

```text
MoveToBuildSite
 ↓
Building
 ↓
Completed
 ↓
Idle
```

#### Task

```text
[x] Worker가 건설 위치로 이동 (updateBuild moveToward)
[x] 건설 가능 거리 도달 시 작업 시작 (kBuildInteractRange)
[x] 매 Tick 건설 진행도 증가 (advanceConstruction)
[x] Worker가 멀어지면 진행 정지 (범위 밖이면 진행 호출 안 함)
[x] 건설 완료 시 Building completed 처리
```

#### 완료 기준

```text
- Worker가 실제로 건물을 완성시킬 수 있다.
```

---

### Feature 0.5.6 건물 완성 처리

#### Task

```text
[x] ConstructionComponent 제거 (m_completed=true 전환으로 대체)
[x] Building.completed = true 처리
[x] 완성된 건물 HP 설정 (m_hp = m_maxHp)
[x] 생산 가능 건물이면 ProductionQueue 활성화 (완성 후 tick에서 train 가능)
[x] Drop-off 건물이면 자원 반납 후보 등록 (isDropOff = 완성 + TownHall)
[ ] 타일 walkability 최종 반영 (Epic 5.3 후속)
```

#### 완료 기준

```text
- 미완성 건물이 완성 건물로 전환된다.
- 완성 후 생산/반납/테크 기능이 작동한다.
```

---

## Epic 0.6 단순 적 AI

현재 상태: `[90% — 0.6.1/0.6.2 완료, 0.6.3 경제 루프 구현(일꾼 생산·채집·유료 생산·병력 기반 웨이브) / 신규 병영 건설만 후속]`

### 목표

복잡한 AI가 아니라, Vertical Slice에서 게임이 끝날 수 있도록 하는 최소 AI를 구현합니다.

---

### Feature 0.6.1 AI PlayerState 생성

#### Task

```text
[x] AI 플레이어 리소스 생성 (PlayerId::Enemy 자원)
[x] AI 소유 건물 생성 (enemy TownHall/Barracks)
[x] AI 소유 유닛 생성 (enemy units + Barracks 생산)
[x] AI 업데이트 루프 추가 (GameLogicManager::updateAI)
```

#### 완료 기준

```text
- AI가 별도 PlayerId를 가진다.
```

---

### Feature 0.6.2 Wave Attack AI

#### Task

```text
[x] 일정 시간마다 공격 웨이브 생성 (kAiWaveInterval 35s)
[x] AI 전투 유닛 목록 수집 (Idle 적 전투 유닛)
[x] 플레이어 타운홀 위치 탐색 (findTownHall)
[x] AttackMove 명령 발행 (AttackMoveCommand 런타임 연결)
[x] 웨이브 쿨다운 적용 (m_aiWaveTimer)
```

#### 초기 구현 추천

```text
게임 시작 60초 후
AI 병사 5기 생성
플레이어 타운홀로 AttackMove
이후 90초마다 반복
```

#### 완료 기준

```text
- AI가 일정 시간 후 플레이어 기지로 공격해온다.
```

---

### Feature 0.6.3 AI 생산 루프

후순위 작업입니다.

#### Task

```text
[x] AI 일꾼 생산 (enemy TownHall이 워커를 cap(6)까지 유료 train)
[x] AI 자원 채집 (유휴 enemy 워커를 최근접 가용 자원으로 gather 배정)
[ ] AI 병영 건설 (신규 병영 건설은 후속 — 현재 적은 시작 병영 보유)
[x] AI 병사 생산 (Barracks Warrior 주기적 유료 train — 비용 차감, 더 이상 무료 아님)
[x] 일정 수 이상 모이면 공격 (유휴 병사 ≥ kAiWaveArmySize 또는 타임아웃 시 웨이브)
```

Vertical Slice 단계에서는 Wave Attack AI만 먼저 구현해도 충분합니다.

---

## Epic 0.7 승패 조건

현재 상태: `[95% — 승/패 판정·배너·입력잠금·결과화면 재시작 완료 / "모든 건물 파괴" 대체 패배조건만 후속]`

---

### Feature 0.7.1 주요 건물 등록

#### Task

```text
[x] TownHall 타입 정의 (BuildingType::TownHall)
[x] Player별 주요 건물 목록 관리 (countTownHalls 집계로 대체)
[x] 건물 사망 이벤트 감지 (매 tick getAction==Dead 폴링)
```

---

### Feature 0.7.2 패배 조건

#### Task

```text
[x] 플레이어의 모든 TownHall 파괴 시 패배
[ ] 또는 모든 건물 파괴 시 패배 (TownHall 기준만 사용)
[x] 패배 UI 표시 (DEFEAT 배너)
[x] 입력 잠금 (inputLocked)
```

---

### Feature 0.7.3 승리 조건

#### Task

```text
[x] 모든 적 주요 건물 파괴 시 승리 (적 TownHall 0)
[x] 승리 UI 표시 (VICTORY 배너)
[x] 결과 화면 + 재시작 (배너 + "Press Enter to restart" → RestartCommand로 매치 리셋. 전용 씬 전환은 선택)
```

### 완료 기준

```text
- 적 타운홀을 파괴하면 승리한다.
- 내 타운홀이 파괴되면 패배한다.
```

---

# 4. Phase 1. Foundation

## 목표

```text
결정론적 시뮬레이션과 데이터 주도 설계 기반을 다진다.
```

---

## Epic 1.1 명령 버스 체계

현재 상태: `[100%]`

### 추가 점검 Task

```text
[ ] LogicCommandBus와 UICommandBus 책임 분리 문서화
[ ] UI 명령이 직접 World를 수정하지 않도록 검증
[ ] Command 처리 순서 고정
[ ] Tick별 명령 처리 로그 추가
```

### 완료 기준

```text
- UI는 명령만 발행한다.
- Simulation은 정해진 Tick에서 명령을 처리한다.
```

---

## Epic 1.2 데이터 주도 설계 확장

현재 상태: `[100% 완료]`

---

### Feature 1.2.1 UnitStaticData 확장

#### Task

```text
[x] maxHp
[x] attackDamage
[x] armor
[x] moveSpeed
[x] attackRange
[x] attackCooldown
[x] sightRange (Unit에 저장·getter, fog 연결은 후속 — revealCircle 소비처 미구현)
[x] collisionRadius (CollisionSystem이 유닛별 반경 사용)
[x] costGold
[x] costWood
[x] supplyCost
[x] buildTime (buildTimeSeconds — 생산 시 건물이 유닛별 값 사용)
[x] weaponType (enum, JSON 문자열 매핑 — 데미지 테이블은 후속)
[x] armorType (enum, JSON 문자열 매핑 — 데미지 테이블은 후속)
```

---

### Feature 1.2.2 BuildingStaticData 추가

#### Task

```text
[x] maxHp
[x] footprintWidth
[x] footprintHeight
[x] costGold
[x] costWood
[x] buildTime (buildTimeSeconds)
[x] produces (데이터 목록, defaultUnitFor가 produces.front() 사용)
[x] providesSupply (완성 건물 합산으로 팀 foodCapacity 산정)
[x] isDropOff (BuildingStaticData 필드, Building::isDropOff가 데이터 참조)
[x] requirements (건설 시 선행 건물 완성 여부 검사)
```

---

### Feature 1.2.3 ResourceStaticData 추가

#### Task

```text
[x] resourceType
[x] initialAmount
[x] gatherAmountPerTrip
[x] gatherDurationTick (gatherDurationSeconds로 구현 — ResourceNode 런타임 단위와 정렬)
[x] maxGatherers
```

---

### Feature 1.2.4 DataRegistry 도입

#### Task

```text
[x] UnitTypeId 관리
[x] BuildingTypeId 관리
[x] ResourceTypeId 관리
[x] 문자열 ID → 내부 ID 변환 (unitById/buildingById/resourceById)
[x] 데이터 로드 실패 처리 (파일 누락·파싱 오류 시 빌트인 기본값 유지 + 로그)
[x] 데이터 검증 로그 출력 (미지 ID·범위 위반 경고, 로드 요약 출력)
```

#### 완료 기준

```text
- 유닛/건물/자원 데이터가 코드 수정 없이 변경 가능하다. ✅
  data/units.json·buildings.json·resources.json 편집 → 재실행만으로 반영.
```

---

## Epic 1.3 EntityId 시스템

현재 상태: `[100% 완료]`

---

### Feature 1.3.1 EntityId 타입 추가

#### Task

```text
[x] EntityId 구조체 추가 (core/ecs/EntityId.hpp)
[x] InvalidEntityId 정의 ({0xFFFFFFFF,0} 센티넬)
[x] index/generation 구조 결정 (uint32 index + uint32 generation)
[x] 비교 연산자 추가 (operator== = default)
[x] Hash 함수 추가 (std::hash<EntityId> 특수화)
```

#### 예시 구조

```cpp
struct EntityId
{
    uint32 index;
    uint32 generation;
};
```

---

### Feature 1.3.2 EntityManager 추가

#### Task

```text
[x] CreateEntity (core/ecs/EntityManager.hpp)
[x] DestroyEntity
[x] IsAlive
[x] GetGeneration
[x] Reuse index with generation increment (free list + generation bump on destroy)
```

---

### Feature 1.3.3 기존 IGameElement와 연결

#### Task

```text
[x] IGameElement에 EntityId 부여 (GameWorld::addElement가 부여, entityId()/setEntityId())
[x] 기존 포인터 참조를 EntityId 참조로 점진 교체 (attack target·gather 자원/드롭오프·build 대상 모두 핸들화)
[x] Target 포인터를 TargetEntityId로 변경 (Unit m_attackTarget/targetResource/targetDropOff/buildTarget → EntityId + resolver)
[x] Command 대상도 EntityId 사용 (AttackCommand가 대상 EntityId를 싣고, 로직이 검증 후 우선 사용·위치 폴백)
```

#### 완료 기준

```text
- 죽은 유닛을 참조해도 IsAlive로 검증 가능하다. ✅ (GameWorld::isAlive(EntityId) + pruneDeadEntities)
- 명령과 타겟팅이 EntityId 기반으로 동작한다. ✅ (런타임 타겟팅 전부 EntityId; AttackCommand가 대상 EntityId 전달)
```

---

## Epic 1.4 고정 틱 / 결정론

현재 상태: `[~82% — 1.4.1/1.4.2 완료, Fixed 토대+이동 무버 커널 전환 시작 / position 저장·충돌·사거리·투사체 Fixed 마이그레이션 후속]`

---

### Feature 1.4.1 Fixed Tick 정리

#### Task

```text
[x] Logic Tick Rate 상수화 (core/sim/SimClock.hpp: kLogicTickHz/kFixedDeltaSeconds/kLogicTickInterval)
[x] Render Delta와 Logic Delta 분리 (Logic=고정 dt via SimClock, Render=GameApp 루프 별도)
[x] Tick 번호 currentTick 관리 (GameWorld::currentTick()/advanceTick(), 매 tick 증가)
[x] 모든 Simulation Update가 Tick 기반으로 동작하는지 점검 (감사: sim 경로에 wall-clock/RNG 없음, 전부 고정 dt)
```

---

### Feature 1.4.2 Float 사용 구역 분리

#### Task

```text
[x] Simulation에서 float 사용 위치 조사 (Vector2D 위치/속도, moveSpeed, attackRange, 쿨다운/빌드/훈련 타이머, std::sqrt)
[x] Rendering 전용 float와 Simulation float 구분 (Render: animationSeconds·카메라 / Sim: 위 항목)
[x] 위치/속도/거리 계산 타입 통일 (현재 전부 float Vector2D로 통일 / Fixed 전환은 1.4.3)
```

---

### Feature 1.4.3 FixedPoint 도입 준비

처음부터 전체를 교체하지 말고 단계적으로 적용합니다.

#### Task

```text
[x] Fixed 타입 추가 (core/sim/Fixed.hpp — 16.16 고정소수, 컴파일타임 static_assert 검증)
[x] FixedVec2 추가
[x] Grid 좌표와 World 좌표 변환 함수 추가 (worldToGrid/gridToWorldCenter)
[~] 이동 계산부터 Fixed 적용 (라이브 경로추종 updateMove를 Fixed stepToward 커널로 전환 / position 저장·충돌 push·attack chase·moveToward는 후속 증분)
[ ] 공격 사거리 계산에 Fixed 적용 (후속)
[ ] 투사체 계산에 Fixed 적용 (후속)
```

#### 완료 기준

```text
- 같은 입력을 두 번 실행했을 때 결과가 동일하다. (미충족 — float 수학 잔존. 틱/입력 경로는 결정적이며, Fixed 마이그레이션 완료 시 달성)
```

---

# 5. Phase 2. Input / Command

## 목표

```text
플레이어의 조작을 유닛의 명령 큐로 변환하는 시스템을 완성한다.
```

---

## Epic 2.1 마우스 선택

현재 상태: `[100%]`

### 추가 Task

```text
[x] 선택 우선순위 적용 (플레이어 유닛 > 임의 유닛 > 건물/자원)
[x] 드래그 시 유닛 우선 / 건물 후순위 (혼합 드래그 시 유닛만 선택)
[x] Shift 선택 추가/제거 (드래그=추가, 클릭=토글)
[x] Ctrl 클릭 동일 타입 선택 (selectSameType)
[x] 더블클릭 동일 타입 선택 (350ms·16px 내 재클릭)
[x] 선택 가능 최대 개수 제한 (kMaxSelection=24)
[x] (보너스) 클릭 선택 픽박스 — degenerate Rect 클릭이 커서 아래 요소를 잡도록 수정
```

---

## Epic 2.2 기본 명령

현재 상태: `[100%]`

### 추가 Task

```text
[ ] MoveCommand validation
[ ] AttackTargetCommand validation
[ ] StopCommand validation
[ ] 명령 실패 사운드/커서 이벤트
[ ] 선택 유닛 중 명령 수행 가능 유닛만 필터링
```

---

## Epic 2.3 Smart Command Resolver

현재 상태: `[70%]`

### Task

```text
[x] 우클릭 대상이 땅이면 MoveCommand
[x] 우클릭 대상이 적이면 AttackTargetCommand
[x] 우클릭 대상이 자원이면 GatherCommand (handleAttackCommand의 resource 분기)
[ ] 우클릭 대상이 미완성 건물이면 Build/RepairCommand
[ ] 우클릭 대상이 아군 건물이면 RallyPoint 또는 Repair
[ ] 우클릭 불가 대상이면 InvalidCommand
```

### 완료 기준

```text
- 플레이어는 대부분 우클릭만으로 상황에 맞는 명령을 내릴 수 있다.
```

---

## Epic 2.4 Command Queue

현재 상태: `[100% 완료]` (Move/Attack/Gather 혼합 체인)

---

### Feature 2.4.1 UnitOrder 구조 추가

#### Task

```text
[x] UnitOrder 타입 정의
[x] OrderType 정의
[x] TargetEntityId 추가 (ecs::EntityId — Attack/Gather 큐 대상이 generation 핸들로 해석)
[x] TargetPosition 추가
[x] AbilityId 추가
[x] BuildingTypeId 추가
```

---

### Feature 2.4.2 Queue 동작

#### Task

```text
[x] 일반 명령 시 기존 Queue Clear (Move/Attack/Gather/Build/AttackMove/Patrol)
[x] Shift 명령 시 Queue 뒤에 추가 (Move waypoint + 우클릭 스마트 Attack/Gather)
[x] 현재 명령 완료 시 다음 명령 Pop (issueNextQueuedOrder가 Move/AttackMove/Patrol/Attack/Gather 디스패치)
[x] Stop 시 Queue Clear
[x] Hold 시 Queue Clear 후 Hold 상태
[x] 큐가 있으면 자동 retarget보다 큐 우선 (대상 사망 시 다음 명령으로 진행)
```

#### 완료 기준

```text
- Shift 우클릭으로 여러 이동 지점을 예약할 수 있다. ✅
- 이동/공격/채집을 섞어 체인 예약할 수 있다(우클릭 스마트 명령). ✅
  Attack/Gather 큐 대상은 EntityId로 해석되어, 죽거나 재사용된 대상은 건너뛰고 큐가 진행된다.
```

---

## Epic 2.5 고급 명령

현재 상태: `[100% 기초]`

---

### Feature 2.5.1 AttackMove

#### Task

```text
[x] AttackMoveCommand 추가
[x] 목적지로 이동
[x] 이동 중 적 발견 시 공격
[x] 적 사망 후 원래 목적지로 복귀
[x] 목적지 도착 시 Idle 또는 Guard 상태
```

---

### Feature 2.5.2 Patrol

#### Task

```text
[x] PatrolCommand 추가
[x] A 지점과 B 지점 저장
[x] A↔B 왕복 이동
[x] 이동 중 적 발견 시 공격
[x] 적 사망 후 순찰 복귀
```

---

### Feature 2.5.3 Hold

#### Task

```text
[x] HoldCommand 추가 (기존 HoldPositionCommand 런타임 동작 연결)
[x] 위치 고정
[x] 사거리 내 적만 공격
[x] 사거리 밖 적 추격 금지
```

---

# 6. Phase 3. Movement

## 목표

```text
다수의 유닛이 충돌 없이 자연스럽게 목적지에 도달하도록 한다.
```

---

## Epic 3.1 A* 길찾기

현재 상태: `[100%]`

### 추가 점검 Task

```text
[x] 대각선 이동 가능 여부 정책 확정
[x] Corner cutting 방지
[x] 이동 불가 타일 처리
[x] 지형 비용 처리
[x] Path 실패 처리
```

---

## Epic 3.2 PathRequestQueue

현재 상태: `[80%]` (기본 큐 구현 완료, 100기 스트레스 수동 검증 필요)

### Task

```text
[x] PathRequest 구조 추가
[x] 요청 Queue 추가
[x] 한 Tick당 처리량 제한
[x] 요청 완료 콜백 또는 결과 저장
[x] 오래된 요청 취소
```

### 완료 기준

```text
- 유닛 100기가 동시에 이동해도 한 프레임이 급격히 멈추지 않는다.
```

---

## Epic 3.3 기본 충돌/정지

현재 상태: `[100%]`

### 추가 Task

```text
[x] 목적지 주변 빈 위치 찾기
[x] 충돌 반경 기반 겹침 방지
[x] 사망 유닛 충돌 제거
[x] 건물 충돌 반영
[x] ResourceNode 충돌 정책 결정
```

---

## Epic 3.4 Local Avoidance

현재 상태: `[80%]` (EntityId 기반 결정 순서 전환 필요)

### Task

```text
[x] 주변 유닛 검색
[x] 충돌 예상 검사
[x] Push Vector 계산
[ ] EntityId 순서로 결정론적 처리 (현재 World 삽입 순서 fallback)
[x] 이동 중 부드러운 회피 적용
[x] 지나치게 밀리는 현상 제한
```

### 초기 구현 추천

```text
RVO보다 먼저:
- 가까운 유닛끼리 살짝 밀어내기
- 목적지에 가까우면 정지 우선
- 막혔으면 재경로 요청
```

---

## Epic 3.5 Formation

현재 상태: `[100%]` (기본 grid-slot 분산 이동 구현 완료)

### Task

```text
[x] 선택 유닛 정렬
[x] 목적지 기준 Grid Slot 생성
[x] 각 유닛에 슬롯 할당
[x] 슬롯 위치가 막혔으면 근처 위치 검색
[x] 유닛별 MoveCommand 발행
```

### 완료 기준

```text
- 10기 이상 이동해도 한 점에 뭉치지 않는다.
```

---

# 7. Phase 4. Combat

## 목표

```text
상성과 세밀한 마이크로 컨트롤이 가능한 전투 엔진을 구축한다.
```

---

## Epic 4.1 기본 전투

현재 상태: `[100%]`

### 추가 안정화 Task

```text
[x] 타겟 사망 시 재탐색
[x] 공격 불가 대상 필터링 (Unit::canAttackTarget 중앙화 — 죽은/아군/중립 자원 제외, attack/beginAttack/holdEngage·자동획득 공통 적용)
[x] 공중/지상 공격 가능 여부 추가 준비 (MovementDomain + attacksGround/attacksAir 데이터·JSON 배선, canAttackTarget에서 레이어 검사)
[x] 사거리 계산 최적화 (m_attackRangeSq 캐시; 모든 사거리 비교는 distanceSq vs rangeSq 제곱 비교)
```

---

## Epic 4.2 무기/장갑 상성

현재 상태: `[100% 완료]`

### Task

```text
[x] WeaponType enum 추가 (core/data/CombatTypes.hpp)
[x] ArmorType enum 추가
[x] 상성 테이블 추가 (damageMultiplier(WeaponType, ArmorType))
[x] UnitStaticData에 armorType 연결 (Unit::armorType() override)
[x] WeaponData에 weaponType 연결 (Unit m_weaponType, 건물은 BuildingStaticData.armorType=Fortified)
[x] 데미지 공식에 배율 적용 (Unit 공격 시 attackDamage * damageMultiplier(weapon, target->armorType()))
```

### 완료 기준

```text
- 관통 무기는 경장갑에 강하다.
- 공성 무기는 건물에 강하다.
```

---

## Epic 4.3 공격 FSM 분리

현재 상태: `[100% 완료]`

### Task

```text
[x] AttackState enum 추가 (Unit::AttackPhase)
[x] Ready 상태
[x] PreCast 상태 (선딜 = attackCooldown * 0.35)
[x] FirePoint 상태 (PreCast 종료 시 데미지 적용 — 순간 전환)
[x] Cooldown 상태 (후딜 = attackCooldown * 0.65)
[x] 공격 중 Move/Stop 입력 처리 (action 전환 시 updateAttack 미실행 → 스윙 취소)
[x] FirePoint 이전 취소 가능 (PreCast 중 이동/사거리 이탈 시 Ready로 리셋, 데미지 없음)
[x] FirePoint 이후 공격 발생 보장 (데미지가 FirePoint에서 즉시 확정 후 Cooldown)
```

### 완료 기준

```text
- 선딜/발사/후딜이 분리된다.
- 무빙샷 구현 기반이 생긴다.
```

---

## Epic 4.4 Projectile Manager

현재 상태: `[90% — homing 투사체 완비 / directional·전용 ProjectileData는 후속]`

### Task

```text
[x] Projectile Entity 타입 추가 (GameWorld가 m_projectiles로 보관·업데이트)
[~] ProjectileData 추가 (전용 struct 대신 발사 시 damage/weaponType/speed 전달)
[~] Instant/Homing/Directional 타입 구분 (Homing 구현; Instant=근접 즉시타; Directional 후속)
[x] 발사 위치 계산 (origin = 유닛 위치)
[x] 목표 위치/목표 Entity 저장 (m_target + lastKnownTargetPos, 타겟 사망 시 마지막 위치로)
[x] 매 Tick 위치 갱신 (GameWorld::updateProjectiles → Projectile::tick)
[x] 충돌 또는 도착 시 데미지 적용 (hit radius 도달 시 damageMultiplier 적용 takeDamage)
[x] 투사체 제거 (expired 제거)
```

### 완료 기준

```text
- 원거리 공격 시 투사체가 생성되고 목표에 도달하면 데미지가 들어간다.
```

---

## Epic 4.5 AoE / Splash

현재 상태: `[100%]`

### Task

```text
[x] SplashData 추가 (CombatTypes.hpp SplashRadii; UnitStaticData.splash, marine 기본값)
[x] 중심/중간/외곽 반경 추가 (inner/mid/outer = 100%/50%/25%, outer==0이면 단일타격)
[x] Spatial Query로 범위 내 대상 검색 (GameWorld::applySplashDamage가 m_elements 순회)
[x] 거리별 데미지 배율 적용 (splashFalloff × damageMultiplier 적용 takeDamage)
[x] 아군 피해 여부 정책 추가 (적 팀만 피격; 아군·중립 자원 제외)
```

### 완료 기준

```text
- splash가 설정된 원거리 공격이 명중하면 착탄 지점 주변의 적들이 거리별로 피해를 입는다.
- 아군과 중립 자원은 splash 피해를 받지 않는다.
```

---

# 8. Phase 5. Economy & Tech

## 목표

```text
자원을 모으고 병력을 양산하는 거시 경제 체계를 완성한다.
```

---

## Epic 5.1 자원 UI

현재 상태: `[100%]`

### 추가 Task

```text
[x] 자원 변경 이벤트 기반 UI 갱신 (recomputeSupply가 매 틱 스냅샷 비교 → 변경 시에만 처리)
[x] 자원 부족 시 빨간색 표시 (인구 ≥ 수용량이면 Food pill 값 빨강 kDanger)
[x] 인구수 표시 추가 (foodUsed = 생존 유닛 + 생산 큐 식량, army = 생존 전투 유닛 수를 실시간 집계)
[x] 자원 증가/감소 로그 디버그 출력 (logResourceChange가 팀별 gold/wood/food/army 델타 출력)
```

---

## Epic 5.2 자원 소비

현재 상태: `[100% — 버티컬 슬라이스 범위. 업그레이드 콘텐츠 자체가 없어 연결 대상 없음, 프레임워크는 준비됨]`

### Task

```text
[x] Cost 구조 추가 (Cost{gold,wood,food} — supply 대신 food)
[x] CanAfford 함수 추가 (PlayerResourceState::canAfford)
[x] PayCost 함수 추가 (pay)
[x] RefundCost 함수 추가 (refund)
[x] 생산/건설/업그레이드에 연결 (생산·건설 연결 완료. 업그레이드/연구 콘텐츠가 없어 비용 연결 대상이 없음 — TechTreeValidator.canResearch + Requirement.requiredUpgrades로 추후 추가 시 즉시 연결 가능)
[x] 자원 부족 시 명령 거부
```

### 예시 구조

```cpp
struct Cost
{
    int gold;
    int wood;
    int supply;
};
```

### 완료 기준

```text
- 자원이 부족하면 생산/건설 불가
- 성공 시 자원이 차감됨
```

---

## Epic 5.3 건물 Footprint

현재 상태: `[100% — footprint 점유를 캐시 그리드로 정밀화(구조물 추가/파괴 시 재구성), isCellOccupied O(1)]`

### Task

```text
[x] BuildingStaticData에 footprint 크기 추가
[x] 건물 생성 시 Grid 점유 처리 (isCellOccupied가 footprint 전체 셀 반영)
[x] 건물 파괴 시 Grid 점유 해제 (Dead 건물은 점유에서 제외 — 자동)
[x] Pathfinding walkability 갱신 (isBlockedDynamic→A*, addElement의 collisionVersion bump로 캐시 무효화)
[x] 건설 중인 건물도 막힘 처리 (완성 전에도 점유 — 통과 불가로 정책 결정)
```

### 완료 기준

```text
- 건물이 차지한 타일을 유닛이 통과하지 못한다.
```

---

## Epic 5.4 TechTreeValidator

현재 상태: `[100% — 검증기/게이트 완비. 업그레이드는 콘텐츠가 없어 canResearch는 구조만 준비(UnknownUpgrade 반환)]`

### Task

```text
[x] Requirement 구조 추가 (core/data/TechTree.hpp: Requirement{requiredBuildings, requiredUpgrades})
[x] requiredBuildings 추가 (Building/Unit static data + units.json/buildings.json requirements 파싱)
[x] requiredUpgrades 추가 (UpgradeType enum + Requirement.requiredUpgrades — 콘텐츠는 아직 없음)
[x] CanProduce 검사 (TechTreeValidator::canProduce → handleTrainCommand에서 게이트)
[x] CanBuild 검사 (TechTreeValidator::canBuild → handleBuildCommand, hasBuildingRequirements 위임)
[x] CanResearch 검사 (TechTreeValidator::canResearch — 업그레이드 정의 추가 전까지 UnknownUpgrade)
[x] UI에서 잠금 표시 (생산 가능 건물의 Train 버튼이 미완성/조건 미충족 시 회색 비활성)
```

### 완료 기준

```text
- 병영이 없으면 병사를 생산할 수 없다. ✅ (canProduce + 선택 건물 생산 목록)
- 선행 건물이 없으면 고급 건물을 지을 수 없다. ✅ (canBuild가 requirements 검사)
```

---

# 9. Phase 6. UI & Fog of War

## 목표

```text
상황 파악과 조작을 극대화하는 UI/UX를 완성한다.
```

---

## Epic 6.1 Wireframe

현재 상태: `[100%]`

### 추가 Task

```text
[x] 다중 선택 초상화 표시 (UpdateHudSelection.portraits → drawSelectionPortraits 그리드, 종류별 테두리+HP 틴트바)
[x] 선택 유닛 HP 바 표시 (단일 선택 시 초상화 아래 HP 바, 비율별 녹/황/적)
[x] 생산 중인 건물 진행도 표시 (trainProgress01 + 큐 개수 → drawLabeledProgress)
[x] 건설 중인 건물 진행도 표시 (buildProgress01 → drawLabeledProgress)
[x] 공격력/방어력 아이콘 표시 (Icon_04/05 아이콘 + 수치)
```

---

## Epic 6.2 Minimap

현재 상태: `[100% — 데이터 기반 미니맵 + 클릭 인터랙션]`

### Task

```text
[x] 맵 지형 축소 렌더링 (UpdateMinimap.fog 그리드를 셀 단위로 타일 색 렌더)
[x] 아군 유닛 점 표시 (dots team=0 파랑)
[x] 적군 유닛 점 표시 (dots team=1 빨강, FoW로 가려진 적은 제외)
[x] 자원 점 표시 (dots team=2 금색)
[x] 카메라 viewport 사각형 표시 (camU/V/W/H 정규화 사각형)
[x] 미니맵 좌클릭 시 카메라 이동 (MinimapCommand left → camera.setPosition)
[x] 미니맵 우클릭 시 선택 유닛 명령 전달 (MinimapCommand right → issueWorldOrderAtWorld)
[x] Fog of War 반영 (미니맵이 fog 상태로 미탐색/탐색/시야 틴트)
```

### 완료 기준

```text
- 미니맵으로 카메라 이동 가능 ✅
- 미니맵에서 우클릭 명령 가능 ✅
```

### 완료 기준

```text
- 미니맵으로 카메라 이동 가능
- 미니맵에서 우클릭 명령 가능
```

---

## Epic 6.3 Fog of War

현재 상태: `[100% 기능 — 완료 기준 충족. 마스크 캐싱/Dirty 추적은 후속 최적화]`

### Task

```text
[x] Player별 exploredTiles 추가 (FogOfWar State::Explored)
[x] Player별 visibleTiles 추가 (FogOfWar State::Visible)
[~] 시야 반경별 마스크 캐싱 (현재 매 틱 revealCircle 재계산 — 32x32 규모라 충분, 마스크 캐싱은 후속 최적화)
[~] 유닛 이동 시 시야 Dirty 처리 (매 틱 전체 재계산이라 Dirty 추적 불필요 — 대형 맵 시 후속)
[x] Dirty 유닛 기준 visibleTiles 갱신 (updateFog가 매 틱 resetVisible + 플레이어 유닛/건물 reveal)
[x] 렌더링 마스크 적용 (GameUIManager가 미탐색=불투명/탐색=반투명 shroud DrawRect를 World 레이어에 emit)
[x] 적 유닛 보임/숨김 처리 (시야 밖 Enemy 요소의 ViewModel·미니맵 점 제외)
[x] 건물 마지막 위치 잔상 처리 여부 결정 (잔상 미표시로 결정 — 시야 밖 적 건물은 숨김. 추후 explored 잔상은 옵션)
```

### 완료 기준

```text
- 유닛 주변만 현재 시야로 보인다. ✅
- 한 번 본 지역은 흐리게 유지된다. ✅ (Explored 반투명 shroud)
- 시야 밖 적 유닛은 보이지 않는다. ✅
```

### 완료 기준

```text
- 유닛 주변만 현재 시야로 보인다.
- 한 번 본 지역은 흐리게 유지된다.
- 시야 밖 적 유닛은 보이지 않는다.
```

---

## Epic 6.4 Command Card

현재 상태: `[100% — 선택 종류별 명령 버튼 동적 구성 + 조건/자원 미충족 시 회색 잠금]`

### Task

```text
[x] 선택 대상 기준 사용 가능한 명령 목록 생성 (UpdateHudSelection.kind/canProduce → SfmlHudOverlay)
[x] 3x3 버튼 UI 생성 (기존 그리드 재사용, 동적 개수)
[x] Move/Attack/Stop/Hold/Patrol 버튼 연결 (전투/워커 종류별)
[x] Worker 선택 시 Build 버튼 표시
[x] 생산 건물 선택 시 생산 버튼 표시 (Train → TrainUnit, canProduce일 때)
[x] 버튼 Hotkey 연결 (HUD 버튼·핫키 모두 GameplayInputCommand 경유로 동일 경로)
[x] 비활성 버튼 잠금 표시 (조건 미충족=TechTreeValidator, 자원 부족=trainAffordable → HudCommandButton.locked 회색·비클릭)
```

### 완료 기준

```text
- 선택한 대상에 따라 하단 명령 버튼이 바뀐다. ✅
```

---

# 10. Phase 7. Map / Save / Replay

## 목표

```text
맵 로딩, 저장/불러오기, 리플레이 기반을 구축한다.
```

---

## Epic 7.1 Tiled 맵 로딩

현재 상태: `[100% — JSON 네이티브 + Tiled .tmx(tmxlite) 임포트. loadMap이 확장자로 분기]`

### Task

```text
[x] .tmx 파싱 안정화 (vendored tmxlite로 tmx::Map::load, loadMap이 .tmx 확장자 감지 → loadTmxMap)
[x] TileLayer 로딩 (tmx::TileLayer::getTiles 순회)
[x] ObjectLayer 로딩 (tmx::ObjectGroup::getObjects, class=building/unit/resource)
[x] Collision Layer 로딩 (이름에 "collis" 포함된 타일 레이어의 비-0 gid → blockedTiles)
[x] Resource Spawn Point 로딩 (class=resource + kind=gold/wood)
[~] Player Start Point 로딩 (building/unit를 좌표에 직접 배치. 별도 "start" 마커는 파싱은 하되 현재 미사용)
[x] Neutral Object 로딩 (자원은 중립, team=neutral 객체 처리)
[x] MapData를 World로 변환 (기존 setupInitialWorld가 MapData→World 변환; .tmx가 MapData를 채움)
```

### 완료 기준

```text
- Tiled에서 만든 맵으로 실제 게임을 시작할 수 있다. ✅ (data/maps/tiled_skirmish.tmx 샘플; 시나리오 경로를 .tmx로 지정)
```

### 완료 기준

```text
- Tiled에서 만든 맵으로 실제 게임을 시작할 수 있다.
```

---

## Epic 7.2 Save / Load

현재 상태: `[0%]`

### Task

```text
[ ] SaveGameData 구조 정의
[ ] currentTick 저장
[ ] PlayerState 저장
[ ] Entity 목록 저장
[ ] 각 Entity 상태 저장
[ ] CommandQueue 저장
[ ] ProductionQueue 저장
[ ] ResourceNode 상태 저장
[ ] Fog 상태 저장
[ ] Load 시 World 복원
```

### 완료 기준

```text
- 저장 후 불러오면 같은 상태에서 이어서 플레이 가능하다.
```

---

## Epic 7.3 World Hash

현재 상태: `[100% 핵심 — 완료 기준 충족. 명령/생산 큐 포함은 후속]`

### Task

```text
[x] WorldHash 함수 추가 (GameWorld::worldHash, FNV-1a 64bit)
[x] EntityId 정렬 후 해시 (entityId().index 정렬 후 순회 — 반복 순서 무관)
[~] 위치/HP/상태/명령 큐 포함 (위치(정수화)/HP/액션/팀 포함. 명령 큐는 후속)
[x] PlayerResource 포함 (gold/wood/foodUsed/foodCapacity/army)
[~] ProductionQueue 포함 (현재 미포함 — 후속)
[x] RandomSeed 포함 (프로젝트에 RNG 없음 → 시드 불필요, 결정성 영향 없음)
[x] 렌더링/UI 상태 제외 (시뮬레이션 상태만 해시)
[x] DebugOverlay에 Hash 표시 (F3 토글, 좌상단 tick + hash 텍스트)
```

### 완료 기준

```text
- 같은 상태는 같은 Hash를 가진다. ✅ (정수화로 부동소수 잡음 제거)
```

### 완료 기준

```text
- 같은 상태는 같은 Hash를 가진다.
```

---

## Epic 7.4 Replay

현재 상태: `[0%]`

### Task

```text
[ ] PlayerCommand 로그 기록
[ ] Tick 번호와 함께 저장
[ ] 초기 맵/시드 저장
[ ] Replay 재생 모드 추가
[ ] 입력 대신 로그 명령 실행
[ ] 재생 속도 조절
[ ] WorldHash 비교
```

### 완료 기준

```text
- 플레이한 경기를 명령 로그만으로 다시 재생할 수 있다.
```

---

# 11. Phase 8. AI / Polish / Optimization

## 목표

```text
게임 완성도, 반응성, 성능, 연출 품질을 높인다.
```

---

## Epic 8.1 전술 AI

현재 상태: `[100%]`

### Task

```text
[x] AI 빌드 오더 상태 추가
[x] AI 자원 채집 명령
[x] AI 생산 명령
[x] AI 병력 집결 지점
[x] AI 공격 타이밍 판단
[x] AI 방어 판단
```

### 초기 AI 상태

```text
Opening
 ↓
Gather
 ↓
BuildBarracks
 ↓
ProduceArmy
 ↓
Attack
 ↓
Rebuild
```

---

## Epic 8.2 사운드

### Task

```text
[x] 선택 사운드
[x] 이동 명령 사운드
[x] 공격 사운드
[x] 피격 사운드
[x] 사망 사운드
[x] 생산 완료 사운드
[x] 건설 완료 사운드
[x] 자원 부족 사운드
```

---

## Epic 8.3 이펙트 / 데칼

### Task

```text
[x] 공격 이펙트
[x] 피격 이펙트
[x] 사망 이펙트
[x] 폭발 이펙트
[x] 건설 먼지 이펙트
[x] 자원 채집 이펙트
[x] 혈흔/그을림 데칼
```

---

## Epic 8.4 성능 최적화

### Task

```text
[x] Spatial Grid 도입
[x] 타겟 탐색 최적화
[x] Pathfinding 요청 제한
[x] Projectile Pool
[x] Effect Pool
[x] Render Batch
[x] UI 갱신 빈도 제한
[x] DebugOverlay 토글화
```

---

# 12. Sprint 계획

## Sprint 1. 자원 채집 완성

### 작업 순서

```text
[x] WorkerGatherState 추가
[x] GatherCommand 추가
[x] ResourceNode 예약 추가
[x] Worker Gather FSM 추가
[x] Drop-off 건물 탐색 추가
[x] PlayerResource 증가 연결
```

### 완료 결과

```text
Worker가 Gold/Wood를 실제로 채집하고 타운홀에 반납한다.
```

---

## Sprint 2. 생산 완성

### 작업 순서

```text
1. Cost 구조 추가
2. CanAfford / PayCost 추가
3. ProductionQueue 추가
4. 생산 진행도 Tick 처리
5. 유닛 스폰 위치 계산
6. RallyPoint 이동 연결
```

### 완료 결과

```text
건물에서 자원을 소비해서 병사를 생산한다.
```

---

## Sprint 3. 건설 완성

### 작업 순서

```text
1. BuildingStaticData 추가
2. Build Preview 추가
3. PlacementValidator 추가
4. ConstructionSite 추가
5. Worker Build FSM 추가
6. 완성 건물 전환
```

### 완료 결과

```text
Worker가 새 건물을 지을 수 있다.
```

---

## Sprint 4. 작은 게임 완성

### 작업 순서

```text
1. AI Wave Attack 추가
2. TownHall 승패 조건 추가
3. 결과 UI 추가
4. 테스트 맵 구성
5. 밸런스 1차 조정
```

### 완료 결과

```text
작은 RTS 한 판이 처음부터 끝까지 플레이된다.
```

---

# 13. GitHub Issue 구성 예시

```text
[Epic] Phase 0 - Vertical Slice 완성

[Feature] Worker 자원 채집 FSM 구현
[Task] ResourceNode 예약 시스템 추가
[Task] GatherCommand 추가
[Task] Worker CarryResource 상태 추가
[Task] Drop-off 건물 탐색 로직 추가
[Task] 자원 반납 시 PlayerResource 증가 처리

[Feature] Production Queue 구현
[Task] ProductionQueueComponent 추가
[Task] 생산 비용 차감 처리
[Task] 생산 진행도 Tick 업데이트
[Task] 유닛 스폰 위치 계산
[Task] RallyPoint 이동 명령 연결

[Feature] Construction Loop 구현
[Task] BuildPreview UI 추가
[Task] PlacementValidator 구현
[Task] ConstructionSite Entity 추가
[Task] Worker Build FSM 추가
[Task] 건물 완성 전환 처리

[Feature] Basic AI 구현
[Task] AI PlayerState 추가
[Task] Wave Attack 생성
[Task] Player TownHall 탐색
[Task] AttackMove 명령 발행

[Feature] Victory / Defeat 구현
[Task] TownHall 파괴 감지
[Task] 승리 조건 검사
[Task] 패배 조건 검사
[Task] Result UI 표시
```

---

# 14. 지금 가장 먼저 할 일

현재 개발 우선순위는 다음입니다.

```text
1. 자원 채집 안정화
   - MaxGatherers 초과 시 대기/다른 자원 탐색
   - Drop-off 파괴 시 재탐색
2. ProductionQueue 구현
3. 생산 비용 차감 처리
4. 생산 진행도 Tick 업데이트
5. 유닛 스폰 위치 계산
6. RallyPoint 이동 연결
```

자원 채집 기본 루프는 구현되었고, 안정화 항목을 보강하면 Sprint 1을 닫을 수 있습니다.

그 다음 순서는 다음과 같습니다.

```text
ProductionQueue
 ↓
BuildingConstruction
 ↓
AI
 ↓
VictoryCondition
```

---

# 15. 최종 목표

본 로드맵의 1차 완료 목표는 다음과 같습니다.

```text
일꾼이 자원을 채집하고,
자원으로 건물을 짓고,
건물에서 유닛을 생산하고,
생산된 유닛으로 적을 공격하고,
적 타운홀을 파괴하면 승리하는 작은 RTS를 완성한다.
```

이 목표가 달성되면 이후 다음 시스템을 안정적으로 확장할 수 있습니다.

```text
- Fog of War
- Minimap
- Command Card
- TechTree
- Upgrade
- Projectile
- Advanced AI
- Save / Load
- Replay
- Multiplayer Lockstep
```
