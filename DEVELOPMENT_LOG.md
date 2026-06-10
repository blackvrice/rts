# Development Log

## 2026-06-10 - Epic 4.5 AoE / Splash (범위 피해)

### 변경 내용
- **CombatTypes.hpp**: `SplashRadii { inner, mid, outer }` 구조체 + `splashFalloff(distance, radii)`(inner 이내 100%, mid 이내 50%, outer 이내 25%, 그 밖 0%). `any()`는 outer>0일 때 splash 활성으로 판정.
- **UnitStaticData**: `SplashRadii splash` 필드 추가(기본 0=단일타격). marine에 `{24, 40, 56}` 기본값 부여(팩토리 기본값이라 units.json 미지정 시에도 유지). Unit이 applyStaticData에서 `m_splash` 저장.
- **발사 경로**: `ProjectileSpawner` 시그니처에 `SplashRadii` 추가(IGameElement alias 한 곳 수정으로 Unit 오버라이드 자동 반영). FirePoint에서 `m_splash` 전달.
- **Projectile**: `m_splash` + `SplashApplier`(범위 피해 콜백) 보유. 명중 시 splash가 활성이면 단일 타격 대신 `m_applySplash(착탄지점, ...)` 호출, 아니면 기존 단일 타격.
- **GameWorld**: `applySplashDamage(center, damage, weapon, ownerTeam, radii)` 추가 — m_elements를 순회하며 **적 팀만**(아군·중립 자원 제외) 착탄 반경 내 대상에 `damage * splashFalloff(dist) * damageMultiplier(weapon, armor)` 적용. addElement의 spawner 람다가 splash 활성 시 이 콜백을 Projectile에 주입.

### 검증
- 빌드 성공(21/21). 실행 정상, 크래시/경고 없음. (IDE clang 진단의 include 경로 오류는 거짓 양성 — g++ 빌드로 검증.)
- 완료 기준 충족: splash 설정 원거리 공격 명중 시 착탄 주변 적이 거리별 피해, 아군·중립 자원 무피해.
- 한계: 인게임 splash 범위/낙폭 체감은 수동 확인 필요. 적용 경로·정책은 검토 완료.

## 2026-06-09 - Epic 4.4 Projectile Manager (원거리 투사체)

### 변경 내용
- **Projectile**: texturePath 제거, `weaponType` 보유. 명중 시 `damage * damageMultiplier(weaponType, target->armorType())`로 데미지 적용(4.2 연계). homing(타겟 추적, 사망 시 마지막 위치로 비행 후 소멸).
- **GameWorld**: `m_projectiles` 보관 + `spawnProjectile`/`projectiles()`/`updateProjectiles(dt)`(매 틱 tick + expired 제거). resetForNewMatch에서 정리.
- **발사 주입**: IGameElement에 `setProjectileSpawner` 가상(no-op) 추가, GameWorld::addElement가 게임 요소에 주입(EntityId resolver와 동일 패턴). 콜백이 Projectile 생성 후 spawnProjectile.
- **Unit 원거리 발사**: `isRanged()`(attackRange ≥ 120) + FirePoint 분기 — 원거리면 투사체 발사(데미지는 도착 시), 근접이면 즉시 타격. archer(180)/marine(150) → 투사체, warrior(80)/worker(48) → 근접.
- **GameLogicManager::tick**: `m_world.updateProjectiles(dt)` 호출(movement 직후).
- **렌더**: ProjectileViewModel을 DrawCircle(작은 노란 볼트)로 변경(투사체 스프라이트 에셋 불필요) + IViewModel 누락 오버라이드(update/visible/setVisible/name) 보강. syncWithWorld가 world.projectiles()도 뷰모델화.

### 검증
- 빌드 성공(11/11). 실행 정상, 크래시/경고 없음. (디버깅: ProjectileViewModel이 인스턴스화되며 추상클래스 오류 → IViewModel 순수가상 4개 구현 추가로 해결.)
- 완료 기준 충족: 원거리 공격 시 투사체 생성·이동·도착 시 데미지.
- 한계: 인게임 투사체 비행/명중은 수동 확인 필요.

## 2026-06-09 - Epic 4.3 공격 FSM 분리 (선딜/발사/후딜)

### 변경 내용
- **Unit::AttackPhase** enum(Ready/PreCast/Cooldown) + `m_attackPhase`. updateAttack의 단순 쿨다운을 위상 FSM으로 교체:
  - Ready → 사거리 내 진입 시 PreCast(선딜, attackCooldown*0.35) 시작.
  - PreCast 종료 = **FirePoint**: 데미지 확정 적용 → Cooldown(후딜, attackCooldown*0.65).
  - Cooldown 종료 → Ready. 선딜+후딜 = attackCooldown이라 공격 속도(DPS) 불변.
- **취소/커밋**: PreCast 중 Move/Stop으로 m_action이 바뀌면 updateAttack 미실행 → 스윙 취소(데미지 없음). 사거리 이탈(추격) 시에도 Ready로 리셋. FirePoint 이후엔 데미지가 이미 적용돼 보장됨. → 무빙샷 구현 기반.
- beginAttack이 새 타겟마다 phase=Ready로 초기화.

### 검증
- 빌드 성공(8/8). 실행 정상, 크래시/경고 없음.
- 한계: 인게임 선딜/후딜 체감은 수동 확인 필요. 위상 전환·취소 경로는 검토 완료.

## 2026-06-09 - Epic 4.2 무기/장갑 상성 (데미지 배율)

### 변경 내용
- **CombatTypes.hpp**: WeaponType/ArmorType enum을 UnitStaticData.hpp에서 이리로 이동(순환 의존 회피) + `damageMultiplier(WeaponType, ArmorType)` 상성 테이블. Pierce>Light(1.5), Siege>Fortified(1.5, 건물), Magic>Heavy(1.5), Normal<Fortified(0.7) 등.
- **BuildingStaticData**: `armorType` 추가(기본 Fortified) → 건물은 공성에 약점.
- **IGameElement**: `virtual armorType()`(기본 Unarmored). Unit override→m_armorType, Building override→정적 데이터 armorType.
- **데미지 적용**: Unit 공격(updateAttack/updateHold)에서 `attackDamage * damageMultiplier(m_weaponType, target->armorType())`로 배율 적용. 기존 flat armor 경감과 함께 작동.

### 검증
- 빌드 성공(18/18). 실행 정상, 크래시/경고 없음.
- 완료 기준 충족: Pierce(archer/marine) → Light 1.5배, Siege → 건물(Fortified) 1.5배.
- 한계: 인게임 데미지 수치는 수동 확인 필요. 테이블·적용 경로는 검토 완료.

## 2026-06-09 - Epic 2.1 추가 Task: 선택 우선순위/Shift/Ctrl/더블클릭/최대치

### 변경 내용
- **SelectCommand**: `additive`(shift), `sameType`(ctrl/더블클릭) 플래그 추가.
- **SelectBox**: UI 매니저의 modifier 플래그(`const bool&` additive/sameType)를 받아 릴리스 시 SelectCommand에 실어 보냄.
- **GameUIManager**: MouseLeftPressed에서 더블클릭 감지(steady_clock 350ms + 16px) → `m_selectSameType = m_ctrl || 더블클릭`. SelectBox에 m_shift·m_selectSameType 전달.
- **SelectionSystem 재작업**:
  - `selectInArea(area, additive)`: 영역이 작으면(클릭) **픽박스(30px)** 로 커서 아래 최근접 요소 선택, 아니면 드래그 박스. **유닛 우선**(플레이어 유닛 > 임의 유닛 > 건물/자원), **최대 24개** cap.
  - additive: 드래그=추가(중복 스킵), 클릭=토글(이미 선택 시 해제).
  - `selectSameType(point, additive)`: 커서 아래 유닛의 타입+팀과 같은 모든 생존 유닛 선택(없으면 클릭 폴백).
- **GameLogicManager**: SelectCommand 핸들러가 sameType이면 selectSameType, 아니면 selectInArea(additive)로 분기.
- **보너스 수정**: 클릭(start==end)이 degenerate Rect라 `contains`가 거의 매칭 안 되던 문제 → 픽박스 확장으로 단일 클릭 선택이 안정적으로 동작.

### 검증
- 빌드 성공(16/16). 실행 — DataRegistry/MapLoader 로드 정상, 크래시/경고 없음.
- 한계: 실제 마우스 선택(드래그/Shift/Ctrl/더블클릭) 동작은 자동화 환경상 수동 검증 필요. 로직 경로·우선순위·cap·토글은 검토 완료.
- 참고: IDE(clang) 분석기가 include 경로 미설정으로 다수 오탐을 냈으나 g++ 빌드는 클린.

## 2026-06-09 - JSON 맵 로딩 (시작 레이아웃 데이터화)

### 변경 내용
- **MapData/MapLoader** (`include/core/map/MapData.hpp`, `src/core/map/MapLoader.cpp`): 맵 시나리오 구조체 + JSON 로더. `width/height/tileSize`, 팀별 시작 자원, 건물/유닛/자원 배치, `blockedTiles`(비통행 타일)를 정의. 타입 문자열(`town_hall`/`worker`/`gold`…)은 DataRegistry의 `buildingById/unitById/resourceById`로 enum 해석. 파일 누락/파싱 오류 시 `defaultMapData()`(기존 하드코딩 레이아웃) 폴백.
- **data/maps/skirmish.json**: 기본 시나리오(32x32, 양 팀 TownHall+Barracks, 유닛 행, Gold/Wood). 편집만으로 레이아웃 변경(재컴파일 불필요).
- **GameWorld**: `initTileMap(w,h,tileSize)`(타일맵 재초기화 + gridTransform tileSize 설정), `setTileBlocked(x,y)`(moveCost 0) 추가.
- **GameLogicManager::setupInitialWorld**: 하드코딩 스폰 제거 → `loadMap(DataRoot + "/maps/skirmish.json")` 결과로 타일맵 초기화·블록타일·시작자원·건물·유닛·자원 스폰. 재시작(restartMatch)도 동일 경로라 맵 재적용.

### 검증
- 빌드 성공(11/11). 실행 — `[MapLoader] loaded map ... (32x32, 4 buildings, 8 units, 2 resources)` 출력, 경고 없음, 크래시 없음.
- 한계: 인게임 배치 확인은 수동 검증 필요. 로드·스폰 로직 경로는 확인.

### 비고
- 프로젝트 JSON 데이터 주도 패턴(nlohmann/DataPaths)과 일관. Tiled `.tmx`(XML) 임포트(Epic 7.1)는 별개 경로로 남김 — 본 작업은 JSON 맵.

## 2026-06-09 - A 키를 명시적 Attack-Move로

- A 핫키와 HUD 버튼을 `GameplayInputAction::Attack` → `AttackMove`로 변경하고 버튼 라벨을 "A-Move"로. (기존에도 handleGameplayInput이 Attack/AttackMove를 모두 AttackMove 모드로 보내 동작은 같았으나, 라벨·시맨틱이 모호했음.) 특정 타겟 공격은 기본 우클릭 스마트 명령이 담당.
- 빌드 성공(3/3), 실행 정상.

## 2026-06-09 - 유닛/건물 명령 단축키 정리 + HUD 단축키 표시

### 변경 내용
- **Escape 충돌 해결**: `starCraftHotkeyAction`이 Escape를 CancelProduction으로 먼저 소비해, Epic 0.5에서 넣은 "Escape로 빌드 모드 취소"가 죽어 있었음. CancelProduction을 **C**로 옮기고 Escape를 핫키 맵에서 제거 → KeyPressed `switch`의 `case Escape`가 살아나 무장된 명령 모드를 기본으로 되돌림(빌드/이동/채집 모드 취소).
- **전체 핫키 세트**: M=Move, A=Attack, S=Stop, H=Hold, P=Patrol, G=Gather, B=Build, T=Train, C=Cancel, R=Repair(로직 미구현), Escape=모드 취소. HUD 명령 카드의 모든 버튼이 대응 핫키를 가짐.
- **HUD 단축키 표시**: `HudCommandButton`에 `hotkey` 추가, 버튼 좌상단에 키 글자(노란색)를 표기해 발견성 확보. HUD 버튼과 핫키 모두 `GameplayInputCommand` 동일 경로.

### 검증
- 빌드 성공(3/3). 실행 6초 — 크래시/경고 없음.
- 한계: 실제 키 입력/버튼 표시는 자동화 환경상 수동 검증 필요. 매핑·디스패치 경로는 확인.

### 참고
- Repair(R)·ReturnResource·UseAbility는 로직이 아직 no-op이라 동작 명령에만 핫키를 부여(ReturnResource/UseAbility는 키 미할당). Epic 6.4 "버튼 Hotkey 연결" 완료.

## 2026-06-09 - 문맥 의존 HUD 명령 카드 (Epic 6.4) — 생산/건설 버튼 노출

### 문제
- HUD 하단 명령 카드가 선택과 무관하게 고정 9버튼(Move/Stop/Hold/Attack/Patrol/Gather/Build/Repair/Cancel)이라, **Train(생산) 버튼이 아예 없어** 건물 선택 시 HUD로 생산 불가. 선택 종류에 따라 바뀌지도 않음.

### 변경 내용
- **UpdateHudSelection 확장**: `HudSelectionKind`(None/Worker/CombatUnit/Building/Resource) + `canProduce`(produces 비어있지 않은 완성 건물) 추가.
- **GameUIManager::render**: 주 선택 요소를 판별해 kind/canProduce 설정(Unit→isWorker로 Worker/CombatUnit, Building→canProduce 계산, ResourceNode→Resource).
- **SfmlHudOverlay::drawHud**: 고정 배열 대신 kind별 버튼 목록 동적 구성 —
  - Building(canProduce): **Train** + Cancel
  - Worker: Move/Stop/Hold/Gather/Build/Attack
  - CombatUnit: Move/Stop/Hold/Attack/Patrol
  - Resource/None: 없음
  - 버튼 클릭은 기존대로 `GameplayInputCommand`로 발행(핫키와 동일 경로) → Train은 TrainUnit, Build는 build 모드 무장(프리뷰).

### 검증
- 빌드 성공(15/15). 실행 6초 — 크래시/경고 없음.
- 한계: 실제 HUD 버튼 표시/클릭은 자동화 환경상 수동 검증 필요. 선택 판별·버튼 구성·명령 발행 경로는 확인.

### 결과
- 건물 선택 → Train으로 생산, 워커 선택 → Build로 건설이 HUD에서 가능. Epic 6.4 0% → 80%(비활성 잠금 표시만 후속). 0.4 "HUD 생산 버튼" 후속 해소.

## 2026-06-09 - 자원 노드 footprint 데이터화 (길찾기 우회)

### 변경 내용
- **ResourceStaticData**에 `footprintWidth`/`footprintHeight` 추가(기본 1, Gold/Wood 프리셋·`data/resources.json`은 2×2). DataRegistry가 파싱(양수 검증 포함).
- **isCellOccupied 통합**: 건물·자원을 동일한 footprint 경로로 처리(요소별 `footprintWidth/Height` 조회 → 중심에서 origin 계산 → 영역 점유). 유닛은 1×1. 자원도 이제 footprint 전체가 pathfinding에서 막혀 유닛이 우회.
- 자원 충돌 반경(44px≈지름 88px)이 1타일보다 커서 2×2가 시각적 솔리드감과 맞음. `data/resources.json`에서 자유 조정 가능.

### 검증
- 빌드 성공(10/10). 실행 6초 — `loaded ... 2 resources`, 경고 없음, 크래시 없음.
- 한계: 실제 우회는 자동화 환경상 수동 확인 필요. 점유/파싱 로직 경로는 검토 완료.

## 2026-06-09 - 버그 수정: 유닛이 건물 footprint를 통과하던 길찾기 (Epic 5.3 진전)

### 문제
- 유닛 이동이 건물/자원 같은 고정 장애물을 우회하지 않고 통과. 특히 4×4 TownHall을 그냥 지나감.

### 원인
- `GameWorld::isCellOccupied`가 각 요소의 **중심 셀 1칸**만 점유로 표시 → A*(`isBlockedDynamic` 경유)는 건물의 footprint 중 중심 1칸만 막힌 것으로 보고 나머지 타일을 통과.

### 수정
- `isCellOccupied`가 **Building이면 footprint 전체 타일**(`buildingStaticDataFor(type).footprintWidth/Height`, 중심에서 origin 계산)을 점유로 반환하도록 변경. 그 외 요소(유닛/자원)는 기존대로 단일 셀.
- 효과: A*가 건물 전체를 우회. 목표가 건물 위면 기존 `findApproachPath` 폴백이 근처 빈 셀로 접근(회귀 없음). Dead 건물은 점유에서 제외되어 파괴 시 자동 통행 가능. 건설 중 건물도 점유(통과 불가).
- `DataRegistry::global().building()` const& 접근자로 footprint 조회(벡터 복사 없이 noexcept 유지).

### 검증
- 빌드 성공(2/2). 실행 8초 — 크래시/경고 없음.
- 한계: 실제 인게임 우회 경로는 자동화 환경상 수동 확인 필요. 점유/길찾기 로직 경로는 검토 완료.

### 관련 Epic
- Epic 5.3 건물 Footprint: 20% → 85%(footprint pathfinding 반영). 멀티타일 walkability 정밀화만 후속.

## 2026-06-09 - Epic 0.3/0.4 엣지케이스: 채집 안정화 확인 + 생산 스폰/RallyPoint 로직

### 변경 내용 (0.4 생산 스폰/RallyPoint)
- **RallyPoint 방향 우선 배치**: `findFreeSpawnPosition`이 `std::optional<Vector2D>` 반환 + 선호 지점(rally) 인자. 각 링에서 rally에 최근접한 빈 타일을 선택해 유닛이 rally 방향으로 퍼지도록 함.
- **스폰 공간 없으면 대기**: 반경 내 모든 타일이 막히면 `nullopt` 반환 → `flushPendingSpawns`가 해당 spawn을 `m_pendingSpawns`에 재큐(다음 틱 재시도). 이전엔 앵커로 폴백해 겹쳤음.
- **RallyPoint가 적이면 AttackMove**: `isEnemyNear(point, team)`(pick 반경 내 적대 생존 요소 검사) → rally가 적 근처면 `issueAttackMove`, 아니면 `moveTo`. 집결지에 적이 있으면 도착 시 교전.

### 0.3 채집 엣지케이스 (검증 — 기구현 확인)
- MaxGatherers 초과/예약 실패 → `handleGatherRedirects`의 NeedNewResource 분기가 동일 타입 다른 자원 재탐색(없으면 stop). Drop-off 파괴 → NeedNewDropOff → `findClosestDropOffFor` 재탐색. gather 대상은 EntityId 핸들(Epic 1.3). 모두 이미 동작 → 코드 변경 없이 상태/문서만 갱신.

### 검증
- 빌드 성공(4/4 재컴파일·링크). 실행 8초 — AI 생산으로 스폰 경로 구동, 크래시/경고 없음.
- 한계: rally 방향 배치/대기/적-rally AttackMove의 실제 인게임 동작은 수동 검증 필요. 스폰 경로·로직은 확인.

### 결과
- Epic 0.3 95%(전용 디버그 로그만 후속), 0.4 95%(HUD 명령카드·사운드·RallyPoint UI만 후속). Vertical Slice 핵심 루프의 로직 엣지케이스 정리 완료.

## 2026-06-09 - Epic 0.7: 결과 화면 재시작 (한 판 루프 완성)

### 변경 내용
- **RestartCommand** (`LogicCommand.hpp`): 무인자 명령. 게임 종료 상태에서만 처리.
- **GameWorld::resetForNewMatch**: 동적 상태 초기화 — `m_elements`/`m_entityByIndex` clear, `EntityManager` 재생성, `currentTick=0`, `gameResult=InProgress`, 충돌 버전 bump. 타일맵은 유지.
- **MovementSystem::reset**: 큐된 경로 요청/유닛별 최신 요청 맵 clear(재시작 시 모든 유닛 소멸 대비).
- **setupInitialWorld() 추출**: 생성자의 초기 스폰(자원 풀·유닛 행·양 팀 TownHall/Barracks·자원 노드)을 메서드로 분리. 생성자와 재시작이 동일 시작 위치를 공유.
- **restartMatch()**: write lock → `resetForNewMatch` → selection/movement/pendingSpawns/AI 타이머 리셋 → `setupInitialWorld()`. RestartCommand 핸들러가 게임 종료 시에만 호출.
- **GameUIManager**: 게임 종료 시 KeyPressed에서 Enter → RestartCommand push(그 외 입력 무시). VICTORY/DEFEAT 배너 아래 "Press Enter to restart" 힌트 추가.

### 검증
- 빌드 성공(19/19). 실행 6초 — 크래시/경고 없음, `loaded ... 30 sprites`.
- 한계: 인게임에서 실제 승패 발생 후 Enter 재시작 흐름은 자동화 환경상 수동 검증 필요. 리셋/재스폰 로직 경로·구동 안정성은 확인.

### 결과
- Epic 0.7 95%. "시작 → 플레이 → 승/패 → Enter 재시작" 한 판 루프가 코드 레벨에서 닫힘. "모든 건물 파괴" 대체 패배조건만 후속.

## 2026-06-09 - Epic 0.5: Build Preview + 건설 진행도 UI

### 변경 내용
- **Build Preview** (`GameUIManager::render`, Feature 0.5.2): Build 모드(B 핫키로 무장)에서 매 프레임 커서 그리드 셀에 건물 footprint 고스트를 emit. 배치 가능(초록 0xFF44EE44/투명채움)·불가(빨강 0xFFEE2222) 색으로 표시. 가능 여부는 `GameLogicManager::canPlaceBuilding`을 미러(footprint 타일별 `isTileBlocked`/`isCellOccupied`)해 로직 판정과 색이 일치. World 레이어 zOrder 50으로 스프라이트 위에 렌더.
- **ESC 취소**: KeyPressed 핸들러에 `Key::Escape` 케이스 추가 → Build 모드 해제(기본 모드로). 우클릭은 기존대로 배치.
- **건설 진행도 UI** (`BuildingViewModel`, Feature 0.5.4): 미완성 건물(`!isComplete()`)에 HP 바 아래 청록(0xFF33CCFF) 진행도 바(`buildProgress01()`)를 렌더. 완성 건물의 train 진행도 바와 위치 공유(상호 배타).

### 검증
- 빌드 성공(3/3 재컴파일·링크). 실행 6초 — 크래시/경고 없음, `loaded ... 30 sprites`.
- 한계: 프리뷰 고스트/진행도 바의 실제 시각 표시는 자동화 환경상 수동 확인 필요. 배치 판정 로직(canPlaceBuilding 미러)·렌더 커맨드 경로·구동 안정성은 확인.

### 결과
- Epic 0.5 90%. 건설 전 배치 위치를 초록/빨강으로 미리 확인하고 ESC로 취소 가능, 건설 중 진행도가 바로 표시됨. footprint walkability(Epic 5.3)와 건물 타입 선택 UI만 후속.

### Follow-up
- 건물 타입 선택 UI(현재 B=Barracks 고정), footprint를 그리드 walkability에 반영(Epic 5.3).

## 2026-06-09 - Epic 0.6: 적 AI 경제 루프 (유료 생산·채집·병력 기반 웨이브)

### 변경 내용
- **updateAI 분리**: `updateAiProduction`/`updateAiWorkers`/`updateAiWaves` 헬퍼로 분리.
- **유료 생산** (`updateAiProduction`, 5s 주기): enemy 자원 풀에서 비용을 차감하며 생산 — 더 이상 무료 아님. TownHall은 워커를 cap(6)까지, Barracks는 `defaultUnitFor`(Warrior)를 큐가 비었을 때 train. `canAfford` 통과 시에만 `trainUnit`+`pay`. 식량 용량(providesSupply 기반)에도 묶임.
- **AI 채집** (`updateAiWorkers`, 3s 주기): 유휴 enemy 워커를 `findClosestAvailableResource`(Gold→Wood) + `findClosestDropOffFor`로 gather 배정 → 적 경제가 자생. 채집 중 워커는 idle이 아니라 재배정 안 됨.
- **병력 기반 웨이브** (`updateAiWaves`): 유휴 enemy 병사 수가 `kAiWaveArmySize(6)` 이상이면 즉시, 아니면 `kAiWaveInterval(45s)` 타임아웃 시 발진. 플레이어 TownHall로 AttackMove. 시간-only였던 기존 방식을 "모이면 공격 + 타임아웃 안전망"으로 개선.
- 헤더: AI 헬퍼 3종 선언 + `m_aiGatherTimer` 추가.

### 검증
- 빌드 성공. (참고: GameApp.cpp에서 GCC 14.2.0이 system header `version.h`의 `202002L`를 "invalid digit"로 읽는 비결정적 글리치 1회 → 재빌드로 통과. 소스 무관, 프로젝트 기존 기록과 동일.)
- 실행 8초 — 생산/채집 틱 다회 구동, 크래시/경고 없음.
- 한계: 인게임에서 적 경제 성장·웨이브 실제 동작은 자동화 환경상 수동 검증 필요. 로직 경로·자원 차감·구동 안정성은 확인.

### 결과
- Epic 0.6 90%. 적 AI가 자원을 채집·소비하며 워커/병사를 생산하고, 병력이 모이면 플레이어 기지로 공격. 신규 병영 건설(워커 배치)만 후속.

### Follow-up
- AI 병영 건설(배치 검증 + 워커 buildAt) — 경제가 커지면 2번째 병영. Epic 5.3 footprint와 연계.

## 2026-06-09 - Epic 1.4.3 시작: 이동 계산 Fixed 전환 (무버 커널)

### 변경 내용
- **Fixed 커널 확장** (`include/core/sim/Fixed.hpp`): `Fixed::abs()`, 결정론적 64비트 정수 `isqrt64()`, `FixedVec2::length()`(거리² 오버플로 회피 위해 raw를 int64로 제곱 후 isqrt), 그리고 단일 이동 적분기 `stepToward(from, to, maxStep)`(도달 시 to로 스냅, 아니면 비례 전진). 오버플로 위험한 `lengthSq()`/`dot()`는 제거. `static_assert`로 length(3,4)=5·stepToward 부분전진/스냅/대각·isqrt 검증.
- **라이브 무버 전환**: `Unit::updateMove(dt, GridTransform)`(경로추종 — Move/Patrol의 실제 무버)의 도달 판정·전진을 Fixed `stepToward`로 수행. position은 아직 float 저장이라 진입 시 `fromFloat`, 종료 시 `toFloat`로 브리지.
- 죽은 코드였던 평면 `updateMove(dt)`는 손대지 않음(MovementSystem은 경로추종 변종만 호출).

### 설계 메모 / 결정론 현황
- 핵심 발견: 이 월드의 거리²(최대 ~4e6)는 16.16 Fixed 범위(±32768)를 초과 → 길이는 반드시 int64 중간연산으로. `stepToward`/`length`가 이를 처리.
- 현재는 **이동 적분 계산이 Fixed 경유**하지만 position 저장이 float이고 충돌 push가 float이므로 **완전 비트-결정론은 아직 미달**. 단계적 계획(plan 1.4.3 지침)대로 다음 증분: ① Unit position을 FixedVec2 권위 저장(getPosition/setPosition은 float 투영), ② 충돌 push Fixed화, ③ attack chase·moveToward 전환, ④ 사거리, ⑤ 투사체. 각 단계 후 동일 입력 재현 테스트.
- 참고: 동일 빌드·동일 실행 2회의 결정론은 float로도 성립(연산/순서 동일). Fixed의 목표는 크로스 플랫폼/락스텝 결정론.

### 검증
- 빌드 성공(Fixed.cpp static_assert 전부 통과 — 커널 수학 검증). 실행 6초 — 크래시/경고 없음, `loaded ... 30 sprites`.
- 한계: 인게임 이동의 시각적 동작은 자동화 환경상 수동 확인 필요. 커널은 컴파일타임 검증됨, 무버 전환은 동일 계산의 드롭인이라 동작 보존(서브픽셀 일치).

## 2026-06-09 - Epic 2.4: Command Queue 완성 (Move/Attack/Gather 혼합 체인)

### 변경 내용
- **UnitOrder.targetEntityId**: `int` → `ecs::EntityId`. 큐에 든 Attack/Gather 대상을 generation 핸들로 보관(Epic 1.3 연계) → 실행 시점에 `GameWorld::resolve`로 해석, 죽거나 재사용된 대상은 자동 무효.
- **issueNextQueuedOrder 전 타입 디스패치**: 기존 Move만 처리하던 것을 Move/AttackMove/Patrol/Attack/Gather로 확장. Attack은 `resolve(targetEntityId)`→`unit.attack()`, Gather는 자원 해석 + `findClosestDropOffFor`→`unit.gather()`. 대상이 사라졌으면 그 명령을 건너뛰고 다음 명령을 pop(큐 정지 방지).
- **우클릭 스마트 명령 큐잉**: `handleAttackCommand`의 shift(append) 분기를 대상 종류별로 분기 — 자원→Gather 주문(워커만), 적대→Attack 주문, 아군/빈 땅→Move 주문. 비-shift는 기존처럼 즉시 실행(큐 클리어).
- **enqueueOrderForSelected 헬퍼**: 선택 유닛 각각에 주문 1개 append, idle이면 즉시 첫 주문 시작. workersOnly로 Gather는 워커에만.
- **큐 우선순위**: `handleAttackRetargets`가 큐가 있는 유닛은 자동 retarget 대신 retarget 요청을 해제하고 다음 큐 명령으로 진행하도록 함(직접 공격 대상 사망 시 체인 계속).

### 검증
- 빌드 성공(8/8 재컴파일·링크). 실행 6초 — 크래시/경고 없음, `loaded ... 30 sprites`.
- 한계: 인게임에서 shift-체인(이동→공격→채집) 실제 동작은 자동화 환경상 수동 검증 필요. 디스패치/큐 진행/우선순위 로직 경로·빌드·구동은 확인. 동시 tick 순서(retarget→queue) 검토 완료.

### 결과
- Epic 2.4 Command Queue 100%. Shift 우클릭으로 이동 지점뿐 아니라 공격/채집을 섞어 순차 예약 가능. Attack/Gather 대상은 EntityId 핸들로 해석.

### Follow-up
- 명시 모드(AttackMove/Patrol/Gather 핫키) 명령에 append 인자 추가 시 해당 주문도 큐잉 가능(현재 디스패치는 이미 지원, 명령 wire만 미지원). Build는 큐 비대상.

## 2026-06-09 - Epic 1.4: 고정 틱 정리 + Float 감사 + Fixed 토대

### 변경 내용
- **SimClock** (`include/core/sim/SimClock.hpp`, 1.4.1): 시뮬레이션 고정 틱의 단일 출처. `kLogicTickHz=30`, `kFixedDeltaSeconds=1/30`, `kLogicTickInterval=33ms`, `TickCount` 타입. `LogicThread::run()`이 로컬 상수(33ms·1/30) 대신 이 상수를 사용(동작 동일).
- **currentTick** (1.4.1): `GameWorld`에 `currentTick()`/`advanceTick()` + `m_currentTick`. `GameLogicManager::tick()`가 매 틱 시작에 `advanceTick()` 호출 → 결정적 스케줄링/리플레이·이벤트 틱 스탬프용 단조 증가 인덱스.
- **Render/Logic Delta 분리 명시** (1.4.1): Logic은 SimClock 고정 dt, Render는 GameApp 루프(가변)로 이미 분리되어 있음을 문서화.
- **Float 사용 감사** (1.4.2): 시뮬레이션 경로에 wall-clock/RNG **없음** 확인(LogicThread의 steady_clock은 페이서, SfmlRenderManager/GameApp의 시간·animationSeconds는 렌더 전용). Sim float = Vector2D 위치/속도·moveSpeed·attackRange·쿨다운/빌드/훈련 타이머·std::sqrt. 결정론의 남은 격차는 float 수학뿐.
- **Fixed 토대** (`include/core/sim/Fixed.hpp`, 1.4.3 준비): 16.16 고정소수 `Fixed`(+,-,*,/,<=>; int64 중간연산), `FixedVec2`(+,-,scale,dot,lengthSq), `worldToGrid`/`gridToWorldCenter` 좌표 변환. 헤더 온리이며 `src/core/sim/Fixed.cpp`가 컴파일타임 `static_assert`(곱/나눗셈/그리드 변환 정확성)를 평가. **아직 시뮬레이션에 미연결** — 이동/사거리/투사체는 계획대로 단계적 후속 전환.

### 검증
- 빌드 성공(13/13, Fixed.cpp의 static_assert 전부 통과). 실행 6초 — 크래시/경고 없음, `loaded ... 30 sprites`.
- 한계: 결정론 완료 기준(같은 입력 2회 동일 결과)은 float 잔존으로 미충족. 틱/입력/시뮬레이션 구동 경로는 결정적임을 확인.

### Follow-up (Epic 1.4 잔여 = 1.4.3 마이그레이션)
- MovementSystem/Unit 이동 계산을 FixedVec2로 점진 전환 → 그 다음 사거리, 투사체. 각 단계 후 동일 입력 재현 테스트로 결정론 검증.

## 2026-06-08 - Epic 1.3 마무리: gather/build/dropOff·명령 대상 EntityId화 (100%)

### 변경 내용
- **gather/build/dropOff 핸들화**: `WorkerGatherState`의 `ResourceNode* targetResource`/`Building* targetDropOff` → `EntityId targetResourceId`/`targetDropOffId`, `Building* m_buildTarget` → `EntityId m_buildTargetId`. Unit에 `resolveEntity()`/`resolveResource()`/`resolveBuilding()`(resolver + dynamic_cast) 추가. 채집·건설 FSM의 모든 대상 접근을 매 사용 시 해석으로 교체.
  - 회귀 방지: `ResourceNode::getAction()`이 고갈 시 Dead → prune이 자원 id를 파괴하므로, `updateGather`의 "자원 null이면 클리어" 가드를 **자원이 필요한 단계(MoveToResource/Gathering)에서만** 적용하도록 재구성. 운반 중(MoveToDropOff/DropResource) 자원이 사라져도 적재물 정상 배달.
- **명령 대상 EntityId화**: `AttackCommand`에 `targetEntityId` 추가. GameUIManager가 클릭 시 커서 아래 최근접 요소의 EntityId를 실어 보냄(read lock). `handleAttackCommand`는 그 id가 **살아있는 자원/적대 유닛이고 클릭 근방(pick 반경 내)** 일 때만 우선 사용하고, 아니면 기존 선택-인지 위치 기반 해석으로 폴백 → UX 불변.

### 검증
- 빌드 성공, 실행 6초 — 크래시/경고 없음, `loaded ... 30 sprites`.
- 한계: 인게임 채집/건설/전투의 실제 동작은 수동 검증 필요(자동화 환경). 로직 경로·빌드/구동 안정성·회귀 가드는 검토 완료.

### 결과
- Epic 1.3 EntityId 시스템 100% 완료. 모든 런타임 대상 참조(attack/gather/build/dropOff)가 generation 기반 EntityId 핸들이고, 죽거나 재사용된 대상은 nullptr로 해석. 명령(AttackCommand)도 대상 EntityId를 전달.

## 2026-06-08 - Epic 1.3: EntityId 시스템 (generation 기반 핸들 + IsAlive)

### 변경 내용
- **EntityId** (`include/core/ecs/EntityId.hpp`, 1.3.1): `{uint32 index, uint32 generation}` 핸들. `InvalidEntityId` 센티넬({0xFFFFFFFF,0}), `operator==`(default), `std::hash<EntityId>` 특수화, `isValid()`.
- **EntityManager** (`include/core/ecs/EntityManager.hpp`, 1.3.2): `create()`/`destroy()`/`isAlive()`/`generation()`/`aliveCount()`. free list로 슬롯 재사용, destroy 시 해당 슬롯 generation 증가 → 재사용 전 만들어진 핸들은 isAlive=false. 헤더 온리.
- **IGameElement 연결** (1.3.3): `entityId()`/`setEntityId()` + `m_entityId` 멤버, 그리고 월드가 주입하는 `virtual setEntityResolver(std::function<IGameElement*(EntityId)>)`(기본 no-op).
- **GameWorld**: `EntityManager` + `index→weak_ptr<IGameElement>` 레지스트리 소유. `addElement`가 게임 요소에 EntityId 부여·resolver 주입(생성 위치와 무관하게 전부 커버). `isAlive(EntityId)`(generation 일치 && 살아있음 && Dead 아님), `resolve(EntityId)→shared_ptr`, `pruneDeadEntities()`(Dead 요소의 id 파괴 → 슬롯 회수·generation bump). lock은 호출자 보유 전제(getElements와 동일).
- **Unit 타겟 핸들화**: `IGameElement* m_attackTarget` → `ecs::EntityId m_attackTargetId` + `m_resolveEntity`. `attackTarget()`가 매 사용 시 resolver로 살아있는 포인터 해석(죽었으면 nullptr). 모든 대입/해제(~20곳)를 핸들 기반으로 교체. 전투 내부 로직·Dead 가드는 그대로 유지.
- **GameLogicManager::tick**: 매 틱 `m_world.pruneDeadEntities()` 호출 → 이번 틱에 죽은 대상의 핸들이 즉시 무효화.

### 설계 메모
- 요소는 m_elements에서 erase되지 않으므로(원시 포인터 dangling은 기존에도 없었음) 본 작업의 실익은 **generation 기반 안전 핸들 + IsAlive 검증**과 슬롯 재사용 시 stale 핸들 무효화. 죽은 대상→자동 retarget 동작은 기존(Dead 체크)과 동일하게 보존.

### 검증
- 빌드 성공(19/19). 실행 6초 — 매 틱 prune·resolver 주입 정상, 크래시/경고 없음, `loaded ... 30 sprites`.
- 한계: 자동화 환경에서 35초 AI 웨이브 전투를 관찰할 수 없어, EntityId 기반 타겟팅의 인게임 전투 동작은 수동 검증 필요. 빌드/구동 안정성·로직 경로는 확인. EntityManager 로직(create/destroy/isAlive/재사용)은 단순·검토 완료.

### Follow-up (Epic 1.3 잔여)
- gather/build/dropOff 대상 포인터도 EntityId 핸들로 전환(현재 attack target만 전환).
- 명령(wire) 대상 EntityId화 — 현재 명령은 클릭 위치 기반 해석. 클릭 시 대상 EntityId를 실어 보내는 방식으로 확장 가능.

## 2026-06-08 - 스프라이트/애니메이션 데이터 주도화 (data/animations.json)

### 변경 내용
- **목표**: 소스 수정·재컴파일 없이 PNG + JSON 편집만으로 스프라이트/애니메이션을 추가·삭제·변경.
- **SpriteClip** (`include/core/data/SpriteData.hpp`): 텍스처 경로(에셋 루트 상대)·frameCount·fps·source 사각형·displayW/H·anchorX/Y·trim 을 담는 디자인 타임 클립 구조체.
- **DrawSprite 확장** (`RenderCommand.hpp`): `std::string texturePath` 필드 추가. 비어있지 않으면 렌더러가 경로로 로드, 비어있으면 기존 정수 textureId 폴백(커서/투사체 호환).
- **DataRegistry 확장**: `data/animations.json` 로드. `sprites`(키→SpriteClip)와 `unitSpriteSets`(유닛 id→스프라이트셋) 파싱. 키 규칙: `unit.<set>.<team>.<action>`, `building.<type>.<team>`, `resource.gold.<1-6>`, `resource.wood`. 접근자 `sprite(key)`/`unitSpriteSet(id)` 추가. 빌트인 시드(`seedSprites`)로 JSON 부재 시에도 렌더 가능, **파일이 읽히면 시드를 비우고 JSON을 권위 소스로 사용**(삭제 반영). 미지/텍스처 누락 경고, 로드 요약에 sprite 수 추가.
- **뷰모델 데이터화**: UnitViewModel/BuildingViewModel/ResourceNodeViewModel이 하드코딩 클립·텍스처 id 대신 레지스트리에서 키로 클립을 조회해 DrawSprite를 채움(앵커/크기/소스/프레임/fps/trim 모두 데이터). 유닛 액션→키(idle/move/attack/hold), 팀→blue/red, 골드는 잔량 기반 stage(1~6) 키.
- **렌더러 경로 로더**: `tinySwordsTextureByPath(relativePath)` 추가(경로 키 캐시, 미스도 캐시). `spriteTextureFor(DrawSprite)`가 texturePath 우선·textureId 폴백. DrawSprite 그리기와 HUD 선택 미리보기 양쪽에 적용. 레거시 id→경로 switch는 커서/폴백용으로 유지.
- **데이터 파일**: `data/animations.json` 신설(유닛 14·건물 4·자원 7 = 25 클립).

### 검증
- 빌드 성공(58/58). `RTS.exe` 실행 — `[DataRegistry] loaded 4 units, 2 buildings, 2 resources, 25 sprites` 출력, stderr 경고 없음, 크래시 없음.
- 디버깅: `.items()`를 임시 json에 직접 호출해 dangling 참조로 `type_error(302)` 크래시 → 객체를 명명 변수에 바인딩 후 순회로 수정.
- 한계: 자동화 환경에서 픽셀 단위 시각 확인 불가 → 실제 스프라이트 렌더는 수동 확인 필요. 로드·구동 안정성·로직 경로는 확인.

### 사용법
- 비주얼 변경: `data/animations.json`에서 해당 키의 `texture`/`frameCount`/`fps`/`anchorX,Y`/`displayW,H` 수정 후 재실행.
- 추가: PNG를 에셋 루트에 두고 새 키 추가(예: 새 팀/액션). 삭제: 키 제거(시드 폴백은 파일이 읽히면 무시됨).
- 한계: 새 유닛/건물 enum 자체 추가는 여전히 코드 필요(비주얼만 데이터 주도). weaponType 등과 동일하게 enum은 코드 개념.

### 추가 (같은 날): 커서·월드 타일셋도 데이터 주도화
- `UpdateHudCursor`를 정수 id에서 의미 키(`std::string cursorKey`: default/drag/move/attack)로 변경. GameUIManager가 상태에 따라 키를 발행하고, 렌더러는 `cursor.<key>` 클립을 레지스트리에서 해석해 경로로 로드.
- 월드 타일셋 경로를 `world.tileset` 클립에서 읽도록 변경(부재 시 기존 상수 폴백). 타일 격자 레이아웃/타일 크기는 코드 유지.
- `data/animations.json`에 `cursor.default/attack/drag/move`(4)·`world.tileset`(1) 추가 → 총 30 클립.
- 검증: 빌드 성공, 실행 시 `loaded ... 30 sprites`, 경고/크래시 없음. 이제 모든 렌더 텍스처(유닛·건물·자원·커서·타일셋)가 JSON 경유.

### 추가 (같은 날): 레거시 정수 textureId 파이프라인 완전 제거
- `DrawSprite`에서 `int textureId` 필드 삭제 → 모든 스프라이트가 `texturePath`만 사용. 뷰모델 3종의 `.textureId = 0` 제거.
- `Projectile`의 `int textureId`를 `std::string texturePath`로 교체(생성자/접근자/멤버), `ProjectileViewModel`이 경로 사용. (참고: 현재 투사체를 생성하는 시스템은 아직 없음 — 향후 전투용.)
- 렌더러에서 `tinySwordsSpriteTexture()` id→경로 switch 함수와 관련 정수 id/경로 상수 전부 삭제. 텍스처 로더는 경로 기반 `tinySwordsTextureByPath()` 하나로 단일화. `spriteTextureFor()`는 texturePath 비면 nullptr.
- `kWorldTileSize`·`kWorldTilesetPath`(폴백)만 유지.
- 검증: 빌드 성공(16/16), 실행 시 `loaded ... 30 sprites`, 경고/크래시 없음.

### Follow-up
- 투사체 스폰 시스템 도입 시 사용할 투사체 스프라이트 키를 animations.json에 추가.
- 방향별(8-dir) 시트, 액션별 anchor 차이 등 확장 시 키/스키마 보강.

## 2026-06-08 - Epic 1.2 마무리: UnitStaticData / BuildingStaticData 필드 확장 (1.2.1 / 1.2.2)

### 변경 내용
- **UnitStaticData (1.2.1)**: `sightRange`/`collisionRadius`/`buildTimeSeconds`/`weaponType`/`armorType` 추가. `WeaponType{Normal,Pierce,Siege,Magic}`·`ArmorType{Unarmored,Light,Heavy,Fortified}` enum 정의. warrior/archer/worker/marine 프리셋과 `data/units.json`에 값 채움.
- **Unit 소비**: `m_sightRange`/`m_collisionRadius`/`m_buildTimeSeconds`/`m_weaponType`/`m_armorType` 멤버 + getter 추가, `applyStaticData()`에서 반영.
- **CollisionSystem**: `collisionRadiusFor()`가 유닛별 `getCollisionRadius()`를 사용(이전 타입 고정 상수). 이동 유닛 반경·폴백은 기존 상수 유지.
- **훈련 시간 데이터화**: `Building::currentTrainTime()`가 큐 선두 유닛의 `buildTimeSeconds`를 사용하도록 변경(`tick()` 임계값·`trainProgress()` 분모). 기본값이 기존 동작(Worker 12s / 전투유닛 8s)과 일치.
- **BuildingStaticData (1.2.2)**: `produces`(유닛 목록)·`providesSupply`·`isDropOff`·`requirements`(선행 건물 목록) 추가. TownHall(produces=[worker], supply=20, dropOff=true)·Barracks(produces=[warrior,archer,marine], requires=[town_hall]) 프리셋과 `data/buildings.json` 반영.
- **produces 소비**: `GameLogicManager::defaultUnitFor()`가 하드코딩 switch 대신 `produces.front()` 사용.
- **isDropOff 소비**: `Building::isDropOff()`가 `m_completed && DataRegistry::building(type).isDropOff` 참조(이전 buildingType 하드코딩).
- **providesSupply 소비**: `GameLogicManager::recomputeSupply()`를 매 틱 호출 — 팀별 완성 건물의 providesSupply 합으로 `foodCapacity` 산정(건설/파괴 자동 반영).
- **requirements 소비**: `handleBuildCommand()`가 `hasBuildingRequirements()`로 선행 건물(완성·동일 팀) 보유 여부를 검사해 미충족 시 건설 거부.
- **DataRegistry 파서 확장**: 유닛 새 스칼라 필드 + weaponType/armorType 문자열 매핑, 건물 produces/requirements 배열(문자열 id→enum, 미지 id 경고 후 스킵)·providesSupply·isDropOff 파싱. collisionRadius 양수 검증 추가.

### 검증
- `cmake.exe --build cmake-build-debug --target RTS` 빌드 성공(11/11 재컴파일·링크).
- `RTS.exe` 실행 — `[DataRegistry] loaded 4 units, 2 buildings, 2 resources` 출력, stderr 경고 없음(새 배열/enum 필드 정상 파싱). 정상 구동.
- 한계: 자동화 환경에서 인게임 건설/생산/공급 흐름은 수동 검증 필요. 로직 경로·빌드/구동 안정성 확인.

### 결과
- Epic 1.2 데이터 주도 설계 확장 100% 완료(1.2.1~1.2.4). 유닛/건물/자원 스탯·생산·공급·선행조건이 `data/*.json` 편집만으로 변경 가능.

### Follow-up
- weaponType×armorType 데미지 배수 테이블(전투 시스템 에픽)과 sightRange 기반 FogOfWar 갱신 파이프라인 연결.
- 이동 유닛 자신의 collisionRadius 반영(현재 `movingUnitRadius()`는 대표 상수 28 사용).

## 2026-06-08 - Epic 1.2: DataRegistry + JSON 외부 데이터 로딩 (1.2.3 / 1.2.4)

### 변경 내용
- **JSON 라이브러리**: nlohmann/json 3.12.0 단일 헤더를 `external/json/nlohmann/json.hpp`로 벤더링. `CMakeLists.txt`의 `RTS` include 경로에 `external/json` 추가.
- **데이터 경로 주입**: `include/core/data/DataPaths.hpp.in` 추가 → `configure_file`로 `RTS_DATA_ROOT`(= `${CMAKE_SOURCE_DIR}/data`)를 절대경로 상수 `data::DataRoot`로 생성. 실행 디렉터리와 무관하게 JSON을 찾음.
- **ResourceStaticData** (`include/core/data/ResourceStaticData.hpp`, Feature 1.2.3): `resourceType`/`displayName`/`initialAmount`/`gatherAmountPerTrip`/`gatherDurationSeconds`/`maxGatherers`. Gold(5000)·Wood(2000) 프리셋. 타입 id는 기존 `ResourceNode::ResourceType` 재사용.
- **DataRegistry** (`include/core/data/DataRegistry.hpp`, `src/core/data/DataRegistry.cpp`, Feature 1.2.4):
  - 프로세스 전역 싱글턴 `DataRegistry::global()` — 생성 시 빌트인 기본값으로 시드.
  - `loadFromDirectory(dir)`로 `units.json`/`buildings.json`/`resources.json` 로드. 각 엔트리는 시드 기본값 위에 머지되어 부분 정의도 유효.
  - 문자열 ID → 내부 enum id 변환: `unitById`/`buildingById`/`resourceById`.
  - 로드 실패 처리: 파일 누락·파싱 오류 시 해당 파일 건너뛰고 빌트인 기본값 유지(stderr 경고).
  - 데이터 검증: 미지 ID·비양수 필드(maxHp/footprint/initialAmount) 경고 후 엔트리 스킵, 마지막에 로드 요약 1줄 출력.
- **lookup 함수 레지스트리 위임**: `UnitStaticData.hpp`/`BuildingStaticData.hpp`/`ResourceStaticData.hpp`의 기존 인라인 switch를 `default*StaticDataFor()`(빌트인 시드)로 분리하고, `unitStaticDataFor`/`buildingStaticDataFor`/`resourceStaticDataFor`는 `DataRegistry.cpp`에서 레지스트리를 조회하도록 비인라인 정의로 전환. 모든 기존 호출부는 변경 없이 JSON 값을 사용.
- **시작 시 로드**: `GameApp` 생성자 최상단에서 씬/유닛/건물 생성 전에 `DataRegistry::global().loadFromDirectory(data::DataRoot)` 호출.
- **자원 노드 데이터 주도화**: `GameLogicManager`의 골드/우드 노드 스폰을 `resourceStaticDataFor()` 값(initialAmount·gatherAmount·gatherDuration·maxGatherers)으로 생성하도록 변경.
- **데이터 파일**: `data/units.json`(4종)·`data/buildings.json`(2종)·`data/resources.json`(2종) 추가.

### 검증
- `cmake.exe --build cmake-build-debug --target RTS` 빌드 성공(58/58, 링크 완료).
- `RTS.exe` 실행 — stdout에 `[DataRegistry] loaded 4 units, 2 buildings, 2 resources from D:/Game/RTS/data` 출력, stderr 경고 없음. 정상 구동 확인.
- 완료 기준 충족: data/*.json 편집 → 재실행만으로 유닛/건물/자원 스탯 반영(코드 수정 불필요).

### Follow-up
- Feature 1.2.1 잔여 필드(sightRange·collisionRadius·buildTime·weaponType·armorType)와 1.2.2 잔여 필드(produces·providesSupply·isDropOff·requirements)를 struct·JSON·소비 코드에 확장.
- `Unit()` 기본 생성자는 여전히 `warriorUnitStaticData()` 직접 사용 — 필요 시 레지스트리 경유로 통일.

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
