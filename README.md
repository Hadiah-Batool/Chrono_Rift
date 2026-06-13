<div align="center">

<img src="https://readme-typing-svg.demolab.com?font=BlockBlueprint&size=40&pause=1000&color=C084FC&center=true&vCenter=true&width=600&lines=Chrono+Rift" alt="Chrono Rift" />

**A multi-process, turn-based RPG combat system**

![C++](https://img.shields.io/badge/C%2B%2B-17-C084FC?style=flat-square&logo=c%2B%2B&logoColor=white)
![SFML](https://img.shields.io/badge/SFML-2.x-C084FC?style=flat-square&logo=sfml&logoColor=white)
![POSIX](https://img.shields.io/badge/POSIX-IPC-C084FC?style=flat-square&logo=linux&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-C084FC?style=flat-square&logo=linux&logoColor=white)
![Version](https://img.shields.io/badge/version-v0.1-C084FC?style=flat-square)

</div>

---

## Overview

Chrono Rift is an **OS semester project** built in C++ that blends turn-based RPG combat with low-level systems programming. The game runs as four separate POSIX processes communicating over shared memory — no sockets, no pipes, just raw `shm_open` + `mmap` + condition variables.

Choose your party, pick a level, and fight through increasingly difficult enemy encounters across three environments.

---

## Architecture

```
┌─────────────┐     shm_open/mmap      ┌──────────────────────┐
│   ARBITER   │◄──────────────────────►│  SharedMemoryBlock   │
│  (arbiter)  │                        │  - GameState         │
└──────┬──────┘                        │  - hip_mailbox       │
       │ fork/exec                     │  - turn_condition    │
       │                               │  - global_mutex      │
┌──────▼──────┐     shm_open/mmap      └──────────────────────┘
│     HIP     │◄──────────────────────►         ▲
│ (hip_main)  │                                 │
└──────┬──────┘                                 │
       │ fork/execl                             │
┌──────▼──────┐     shm_open/mmap              │
│   PLAYER    │◄───────────────────────────────┘
│  PROCESSES  │                                 │
└─────────────┘                                 │
┌─────────────┐     shm_open/mmap              │
│     ASP     │◄───────────────────────────────┘
│(enemy proc) │
└─────────────┘
```

| Process | Binary | Role |
|---|---|---|
| **Arbiter** | `arbiter.out` | Game master — owns game state, resolves actions, spawns HIP and ASP |
| **HIP** | `hip` | Human Interface Process — SFML window, menus, renderer; forks player processes |
| **ASP** | `asp` | Adversary Simulation Process — runs enemy AI threads |
| **Player Process** | `player_process` | One per human player — reads game state, writes actions to mailbox |

---

## Project Structure

```
Chrono_Rift/
├── arbiter/
│   ├── arbiter.out
│   ├── enemies_description/
│   │   ├── level_1_sublevel_1.txt
│   │   ├── level_2_sublevel_1.txt
│   │   └── level_3_sublevel_1.txt
│   └── player_description/
│       └── level_2_sublevel_1.txt
├── shared/
│   └── game_state.h          ← GameState, Action enums, ActionLog
├── resources/
│   └── shared_mem_abs.h      ← SharedMemoryBlock definition
├── DisplayRendering/
│   ├── render.h / render.cpp ← Renderer class
│   ├── Menu.h                ← GameMenu class
│   └── BlockBlueprint.ttf
├── Characters/
│   └── Player.h              ← PlayerType enum
├── Players/
│   └── [character sprites]
└── MapsNScreen/
    └── [level tile images]
```

---

## Build & Run

### Dependencies

- `libsfml-dev` — SFML 2.x
- `g++` — C++17 support
- POSIX environment — Linux or macOS only

### Build

```bash
cd arbiter && make
cd ../hip && make
```

### Run

```bash
cd arbiter
./arbiter.out
```

> Always launch via the arbiter. It creates shared memory and spawns HIP and ASP automatically. Do **not** launch `hip` or `player_process` directly.

---

## Startup Flow

```
1.  Arbiter creates + initialises shared memory (shm_open, ftruncate, mmap)
2.  Arbiter sets current_turn_owner_id = -2  ("waiting for setup")
3.  Arbiter forks → exec HIP, ASP

4.  HIP attaches to shm, opens SFML window
5.  HIP runs GameMenu:
      MAIN → LEVEL_SELECT → PARTY_SELECT → CONFIRM

6.  HIP fires setPartyReadyCallback:
      - Loads level tile images
      - Launches setup pthread → writes SETUP_GAME to hip_mailbox
      - Forks one player_process per selected player

7.  Arbiter wakes, reads hip_mailbox:
      - Loads enemy + player spawn data from description files
      - Spawns enemy AI threads (ASP)
      - Begins turn loop

8.  Renderer.run() enters game loop — reads GameState from shm each frame
```

---

## Playable Characters

| Character | Role | HP | DMG | DEF | Flavour |
|---|---|---|---|---|---|
| **CHRONO** | Warrior | 85 | 70 | 65 | Balanced fighter, strong strikes |
| **FROG** | Knight | 95 | 80 | 90 | High defence, heavy blows |
| **MARLE** | Healer | 70 | 55 | 60 | Support magic, keeps party alive |
| **MAGUS** | Mage | 60 | 95 | 40 | Devastating magic, low HP |

Supports **1–4 players** per session, each running as a separate OS process.

---

## Levels

| # | Name | Enemies | Difficulty |
|---|---|---|---|
| 1 | Guardia Forest | 4 | ⭐ |
| 2 | Manoria Cathedral | 4 | ⭐⭐ |
| 3 | Heckran Cave | 4 | ⭐⭐⭐ |

---

## Combat

### Controls

| Key | Action |
|---|---|
| `Space` | Strike — basic attack on selected enemy |
| `E` | Exhaust — depletes enemy stamina |
| `W` | Use Weapon — uses equipped inventory weapon |
| `H` | Heal — restores HP |
| `U` | Ultimate — requires Solar Core + Lunar Blade |
| `P` | Pickup — collect weapon dropped by defeated enemy |
| `Tab` | Swap — move backpack weapon into inventory |
| `Esc` | Skip — pass current turn |
| `← / →` | Cycle target enemy |
| `↑ / ↓` | Cycle inventory / backpack |
| `G + [0/1/2]` | Claim artifact |
| `M` | Return to main menu |

### Artifacts

Three artifacts enter play on a fixed schedule (turns 11, 13, 15):

| Artifact | Effect |
|---|---|
| **Solar Core** | Required for Ultimate |
| **Lunar Blade** | Required for Ultimate |
| **Eclipse Relic** | Special power relic |

States: `HIDDEN → AVAILABLE → HELD_BY_ME / HELD_BY_OTHER / WAITING`

### Weapons

Inventory holds **20 slots** (weapons occupy 2–6 slots each). A separate backpack (LTS) handles swapping.

`Solar Core` · `Lunar Blade` · `Iron Halberd` · `Venom Dagger` · `Thunderstaff` · `Obsidian Axe` · `Frostbow` · `Splinter Stick`

---

## UI Layout

```
┌────────────────────────────────┬──────────────────┐
│                                │   SIDEBAR 420px  │
│        MAP AREA 860px          │  ┌─────────────┐ │
│                                │  │ Active Plyr │ │
│  [Turn Banner]                 │  │ HP / Stamina│ │
│                                │  │ Party Mini  │ │
│  [Enemy sprites]               │  ├─────────────┤ │
│  [Player sprites]              │  │ ENEMIES tab │ │
│                                │  │ INVENTORY   │ │
│  [Dropped weapon banner]       │  │ BACKPACK    │ │
│  [Artifact banners]            │  ├─────────────┤ │
│                                │  │ Action Log  │ │
│  [HUD hints]                   │  │ (5 lines)   │ │
└────────────────────────────────┴──────────────────┘
```

---

## Concurrency Model

| Mechanism | Purpose |
|---|---|
| `shm_open` + `mmap` | Shared memory across all processes |
| `pthread_mutex_t global_mutex` | Protects all shared state reads/writes |
| `pthread_cond_t turn_condition` | Signals turn transitions between processes |
| `pthread_t` (per enemy) | Each enemy runs its own AI thread inside ASP |
| `fork` + `execl` | Spawns HIP, ASP, and each player process |
| `SIGTERM` | Clean shutdown signal from arbiter |
| `waitpid` | HIP and arbiter wait for child processes |

---

## Shutdown Sequence

```
1. Arbiter sends SIGTERM → HIP, ASP
2. HIP handler:
     - sets game_running = false
     - broadcasts turn_condition (wakes player processes)
     - waitpid() on all player processes → _exit(0)
3. ASP: enemy threads detect game_running = false → exit
4. Arbiter waitpid() on HIP → waitpid() on ASP
5. Arbiter destroys mutexes, unlinks shared memory
```

---

## Team

<table>
  <tr>
    <td align="center">
      <a href="https://github.com/Hadiah-Batool">
        <img src="https://github.com/Hadiah-Batool.png" width="80" style="border-radius:50%" /><br/>
        <sub><b>Hadiah Batool</b></sub>
      </a>
    </td>
    <td align="center">
      <a href="https://github.com/Hashimk101">
        <img src="https://github.com/Hashimk101.png" width="80" style="border-radius:50%" /><br/>
        <sub><b>Hashim Khushal Khan</b></sub>
      </a>
    </td>
  </tr>
</table>

---

<div align="center">
  <sub>Built as an Operating Systems semester project @ FAST NUCES</sub>
</div>
