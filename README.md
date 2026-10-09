<p align="center">
  <strong>Danger from the Deep</strong>
</p>
<p align="center">
  <em>Open Source WW2 German Submarine Simulation / U-Boot-Simulation</em>
</p>
<p align="center">
  <sub>v0.3.900 · GPL v2 · AmigaOS 4.1 & Linux</sub>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-AmigaOS%204.1-red?logo=amiga" alt="AmigaOS 4.1" />
  <img src="https://img.shields.io/badge/CPU-PowerPC%20%2B%20AltiVec-orange" alt="PowerPC" />
  <img src="https://img.shields.io/badge/3D-Mesa%203D%20%2F%20Warp3D%20Nova-green" alt="Mesa 3D" />
  <img src="https://img.shields.io/badge/Platform-Linux-FCC624?logo=linux&logoColor=black" alt="Linux" />
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20" />
  <img src="https://img.shields.io/badge/CMake-3.20+-064F8C?logo=cmake&logoColor=white" alt="CMake" />
  <img src="https://img.shields.io/badge/SDL2-2.0-8B0000?logo=sdl" alt="SDL2" />
  <img src="https://img.shields.io/badge/License-GPL%20v2-blue" alt="GPL v2" />
</p>

---

## Language / Sprache
- [English Documentation](#english)
- [Deutsche Dokumentation](#deutsch)

---

<a name="english"></a>
# English

## About the Game
**Danger from the Deep** (dftd) is an advanced open-source World War II German submarine simulation. It offers realistic physics, authentic celestial mechanics, acoustic sensors (sonar / hydrophones), torpedo ballistics, damage control, convoy tactics, and immersive 3D naval combat.

### AmigaOS 4.1 Native Port Highlights
Ported and optimized for **AmigaOS 4.1** (PowerPC) by **TearsOfMe**:
- **Hardware 3D Acceleration**: Powered by **Mesa 3D** and **Warp3D Nova** on modern Radeon graphics cards (RadeonHD and RadeonRX).
- **AltiVec / VMX SIMD Acceleration**: Hand-tuned vector math routines for water wave synthesis, terrain processing, and 3D vector transformations on capable PowerPC CPUs (e.g. PA6T on AmigaOne X1000).
- **Optimized Navigation Map**: Drastically improved world and navigation map rendering performance with persistent OpenGL vertex/chunk caching.
- **Robustness & Stability**: Fixed audio thread shutdown race conditions, protected against missing historical ship models with fallback aliases, and safeguarded mission/savegame loading.
- **Embedded Stack Cookie**: Hardcoded 4 MB stack (`$STACK: 4194304`) to prevent stack overflow without requiring manual Shell configuration.

---

## Included Executables

| Executable | Description | Recommended For |
| :--- | :--- | :--- |
| `dangerdeep_mesa_fast` | Highest performance release build (-O3, AltiVec enabled, file logging overhead disabled) | Daily gameplay |
| `dangerdeep_mesa` | Diagnostic build with verbose startup and in-game logging to `PROGDIR:dftd_startup.log` | Troubleshooting & debugging |

---

## System Requirements

### AmigaOS 4.1:
- **OS**: AmigaOS 4.1 Final Edition (Update 1 or Update 2 recommended).
- **Hardware**: AmigaOne X1000, X5000, Sam460cr/ex, A1222+, or compatible PowerPC machine.
- **Graphics**: RadeonHD or RadeonRX with Warp3D Nova and Mesa 3D (`mesa.library`).
- **Memory**: 1 GB RAM recommended.
- **Audio**: AHI-compatible sound card.

### Linux / PC:
- **OS**: Modern Linux distribution (Ubuntu, Debian, Fedora, Arch).
- **Compiler**: GCC 11+ or Clang with C++20 support.
- **Dependencies**: SDL2, SDL2_image, SDL2_mixer, FFTW3, TinyXML, bzip2, OpenGL.

---

## Keyboard Controls & Stations

### Stations & Views (F1 – F10):
- `[F1]` Central Helm & Gauges (compass, rudder, depth gauges)
- `[F2]` Attack Periscope (observation & optical targeting)
- `[F3]` UZO (Target optics on bridge for surface night attacks)
- `[F4]` Bridge Watch (open surface view)
- `[F5]` Navigation & Situation Map (world map, convoy routes, sensor contacts)
- `[F6]` Torpedo Room (tubes, reloading, preheating, magnetic/impact pistols)
- `[F7]` Damage Control (compartment integrity, bilge pumps)
- `[F8]` War Diary / Logbook (KTB)
- `[F9]` Sunk Tonnage Record
- `[F10]` Free External Camera

### Submarine Helm & Engine:
- `[Cursor Left / Right]` Rudder port / starboard
- `[Cursor Up / Down]` Dive planes (down / up)
- `[Shift + Cursors]` Faster rudder movement
- `[Enter / Return]` Rudder amidships (center)
- `[1]` Ahead Slow · `[2]` Ahead Half · `[3]` Ahead Flank · `[4]` Stop Engines
- `[5]` Astern Slow · `[6]` Astern Full

### Diving & Weapons:
- `[s]` Surface (blow ballast tanks)
- `[p]` Periscope depth (12–14 meters)
- `[d]` Snorkel depth
- `[c]` Alarm / Crash dive
- `[0]` Raise / lower periscope
- `[Space]` Lock / track target ship
- `[i]` Identify target ship (Recognition Manual)
- `[t]` Fire torpedo
- `[Shift + 1..6]` Fire specific torpedo tube 1 to 6
- `[g]` Man deck gun
- `[+] / [-]` Zoom in / out on navigation map
- `[F11] / [F12]` Time compression faster / slower
- `[Pause]` Pause game · `[Esc]` Menu / Quit

---

## Game Data & Git LFS
Game assets (textures, 3D meshes, sound files, missions) are tracked using **Git LFS**. When cloning the repository on a PC:
```bash
git lfs install
git lfs pull
```

---

## Building from Source

### AmigaOS 4.1 (Cross-compilation via Docker):
A ready-to-use build script is provided in `amiga/build_mesa.sh`:
```bash
./amiga/build_mesa.sh
```
This script compiles the project using the official Docker cross-compilation environment (`walkero/amigagccondocker:os4-gcc11-3.3.0`) and outputs stripped Mesa binaries.

### Linux Native:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Documentation Files
- Detailed AmigaOS manual (English): [`amiga/MANUAL.txt`](amiga/MANUAL.txt)
- Ausführliche AmigaOS-Anleitung (Deutsch): [`amiga/ANLEITUNG.txt`](amiga/ANLEITUNG.txt)
- AmigaOS distribution readme: [`amiga/DangerFromTheDeep.readme`](amiga/DangerFromTheDeep.readme)
- Licensing breakdown: [`amiga/LIZENZEN.txt`](amiga/LIZENZEN.txt) / [`amiga/LEGAL_NOTICES.txt`](amiga/LEGAL_NOTICES.txt)
- Credits & Attribution: [`amiga/CREDITS.txt`](amiga/CREDITS.txt)

---

<a name="deutsch"></a>
# Deutsch

## Über das Spiel
**Danger from the Deep** (dftd) ist eine anspruchsvolle Open-Source-U-Boot-Simulation aus dem Zweiten Weltkrieg. Das Spiel bietet physikalisch akkurate U-Boot-Führung, Himmelsmechanik, Akustiksensoren (aktives Sonar und Horchgeräte), Torpedoballistik, Schadenskontrolle, Geleitzugtaktik und detaillierte 3D-Seeschlachten.

### Highlights der nativen AmigaOS 4.1-Portierung
Portiert und optimiert für **AmigaOS 4.1** (PowerPC) von **TearsOfMe**:
- **Hardware-3D-Beschleunigung**: Basiert auf **Mesa 3D** und **Warp3D Nova** für RadeonHD- und RadeonRX-Grafikkarten.
- **AltiVec / VMX SIMD-Optimierung**: Vektorbeschleunigung für Wellenberechnung, Geländedarstellung und Matrixoperationen auf PowerPC-Prozessoren (z. B. PA6T auf dem AmigaOne X1000).
- **Extrem beschleunigte Navigationskarte**: Deutliche Leistungssteigerung auf der Welt- und Navigationskarte (`[F5]`) durch OpenGL-Chunk- und Vertex-Caching.
- **Stabilität & Fehlerbehebung**: Behebt Abstürze beim Beenden des SDL-Audio-Threads, fängt fehlende Schiffsmodelle mit Alias-Namen ab und sichert das Laden von Missionen und Spielständen ab.
- **Integrierter Stack-Cookie**: 4 MB Stack sind fest einkompiliert (`$STACK: 4194304`), sodass kein manuelles Einstellen in der Shell oder den Icon-Tooltypes erforderlich ist.

---

## Enthaltene Programmversionen

| Programm | Beschreibung | Empfohlen für |
| :--- | :--- | :--- |
| `dangerdeep_mesa_fast` | Maximale Performance (-O3, AltiVec aktiv, Logging-Overhead deaktiviert) | Normales Spielen |
| `dangerdeep_mesa` | Diagnose-Version mit ausführlichem Logging in `PROGDIR:dftd_startup.log` | Fehlersuche & Treiberprüfung |

---

## Systemvoraussetzungen

### AmigaOS 4.1:
- **Betriebssystem**: AmigaOS 4.1 Final Edition (Update 1 oder Update 2 empfohlen).
- **Hardware**: AmigaOne X1000, X5000, Sam460cr/ex, A1222+ oder kompatibles PowerPC-System.
- **Grafikkarte**: RadeonHD oder RadeonRX mit Warp3D Nova und Mesa 3D (`mesa.library`).
- **Arbeitsspeicher**: Mindestens 1 GB RAM empfohlen.
- **Audio**: AHI-kompatibles Soundsystem.

### Linux / PC:
- **Betriebssystem**: Aktuelles Linux (Ubuntu, Debian, Fedora, Arch).
- **Compiler**: GCC 11+ oder Clang mit C++20-Unterstützung.
- **Bibliotheken**: SDL2, SDL2_image, SDL2_mixer, FFTW3, TinyXML, bzip2, OpenGL.

---

## Tastatursteuerung & Stationen

### Stationen & Kameras (F1 bis F10):
- `[F1]` Zentraler Steuerstand & Instrumente (Ruderkontrolle, Tiefenmesser, Kompass)
- `[F2]` Angriffssehrohr / Periskop
- `[F3]` UZO (U-Boot-Zieloptik auf der Brücke für Nachtangriffe)
- `[F4]` Freie Brückensicht (Ausguck über Wasser)
- `[F5]` Navigations- & Lagekarte (Weltkarte, Geleitzugrouten, Kontakte)
- `[F6]` Torpedoraum (Rohre, Nachladen, Vorwärmung, Magnet-/Aufschlagzündung)
- `[F7]` Schadenskontrollraum (Zustand der Abteilungen, Lenzpumpen)
- `[F8]` Kriegstagebuch / Logbuch (KTB)
- `[F9]` Versenkungstabelle & Tonnage
- `[F10]` Freie Außenkamera

### Steuerung & Fahrt:
- `[Pfeil links / rechts]` Seitenruder Backbord / Steuerbord
- `[Pfeil hoch / runter]` Tiefenruder (Sinken / Steigen)
- `[Shift + Pfeiltasten]` Schnellere Ruderbewegung
- `[Enter / Return]` Ruder mittschiffs
- `[1]` Kleine Fahrt voraus · `[2]` Halbe Fahrt voraus · `[3]` AK voraus · `[4]` Stopp
- `[5]` Kleine Fahrt zurück · `[6]` AK zurück

### Tauchbefehle & Gefecht:
- `[s]` Auftauchen (Tauchzellen anblasen)
- `[p]` Seerohrtiefe (ca. 12–14 m)
- `[d]` Schnorcheltiefe
- `[c]` Alarm! / Alarmtauchen
- `[0]` Seerohr ein-/ausfahren
- `[Leertaste]` Zielschiff anvisieren / markieren
- `[i]` Ziel identifizieren (Erkennungsbuch)
- `[t]` Torpedo abfeuern
- `[Shift + 1..6]` Torpedorohr 1 bis 6 gezielt abfeuern
- `[g]` Deckgeschütz bemannen / abfeuern
- `[+] / [-]` Zoom der Navigationskarte
- `[F11] / [F12]` Zeitraffer schneller / langsamer
- `[Pause]` Pause · `[Esc]` Menü / Beenden

---

## Spieldaten & Git LFS
Grafiken, 3D-Modelle, Sounds und Missionen werden mit **Git LFS** verwaltet. Nach dem Klonen auf einem PC:
```bash
git lfs install
git lfs pull
```

---

## Kompilieren aus dem Quelltext

### AmigaOS 4.1 (Cross-Build via Docker):
Das Skript `amiga/build_mesa.sh` baut die Mesa-Versionen automatisch:
```bash
./amiga/build_mesa.sh
```

### Linux nativ:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Urheberrecht & Lizenzen
- **Programmcode**: GNU General Public License, Version 2 (GPLv2).
- **Grafiken & 3D-Modelle**: Creative Commons CC-BY-NC-ND 2.5.
- **Musik & Soundtrack**: CC-BY-NC-ND 2.0 / Public Domain.
- **AmigaOS 4.1 Portierung**: TearsOfMe (2026).
- Vollständige Lizenztexte: [`amiga/LIZENZEN.txt`](amiga/LIZENZEN.txt) und [`amiga/LEGAL_NOTICES.txt`](amiga/LEGAL_NOTICES.txt).
