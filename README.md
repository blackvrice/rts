# RTS Engine & Game (C++ / SFML / ECS)

## Overview
C++17 기반으로 구현한 RTS 게임 엔진 및 데모 게임입니다.
ECS 아키텍처, Command 패턴, Scene 관리, Camera, Fog of War,
Pathfinding 구조, HUD/UI 시스템을 포함합니다.

## Key Features
- Custom ECS (cache-friendly, data-oriented)
- Scene / GameScene 분리 구조
- CommandBus 기반 입력/행동 처리
- Camera + Z-sort Renderer
- Animation (Sprite Sheet)
- Combat System (Range, Cooldown, FSM)
- Fog of War (Vision-based)
- MiniMap
- HUD (Resource / Selection / Build)
- Data-driven (JSON)

## Tech Stack
- C++17
- SFML 3.x
- CMake
- Custom ECS

## Architecture
- Scene → GameManager → ECS Systems
- UI (Screen Space) / World (Camera Space) 분리
- Command → System 실행

## Build
```bash
cmake -S . -B build
cmake --build build
