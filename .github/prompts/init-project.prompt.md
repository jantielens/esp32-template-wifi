---
mode: agent
description: Initialize a new project from the ESP32 template repository
---

# Initialize Project from ESP32 Template

You are helping a developer initialize a new project that was cloned from the `esp32-template-wifi` template repository.

## Step 1: Gather Project Information

Use the `vscode_askQuestions` tool to interview the developer. The interview happens in **three rounds** so that earlier answers inform smart defaults for later questions.

### Round 1 — Project description

Ask a single free-text question:

1. **Describe your project** — What does this project do? (1–2 sentences, e.g. "Monitors home solar panel production and grid consumption using CT clamp sensors"). This description is used in the README and copilot instructions.

### Round 2 — Project name and slug (offer options derived from description)

Based on the description, generate options for both questions. Ask them together in a single `vscode_askQuestions` call:

2. **Project display name** — Propose 2–3 human-readable project names derived from the description (used in web portal UI, device name, README title, and logs). Use the `options` array with `allowFreeformInput: true` so the developer can pick one or type their own. Example options for a solar monitoring description:
   - `Solar Energy Monitor`
   - `Home Energy Tracker`
   - `ESP32 Energy Monitor`

3. **Project slug** — Propose 2–3 slug-format names derived from the project name options above (lowercase, hyphens, no spaces). Used for build artifacts (`{slug}-v0.1.0.bin`), WiFi AP SSID, and GitHub releases. Use `options` with `allowFreeformInput: true`. Example options:
   - `solar-energy-monitor`
   - `energy-monitor`
   - `esp32-energy-monitor`

### Round 3 — Boards and copyright

Based on the description, pre-select recommended boards. Ask both questions in a single `vscode_askQuestions` call:

4. **Which boards should this project support?** First, read `config.sh` and parse the `FQBN_TARGETS` associative array to get the current list of available boards. Each entry has the format `["board-name"]="FQBN" # description`. Use the board names as option labels and the inline `#` comments as descriptions.

   Present the boards using `options` with `multiSelect: true` and `allowFreeformInput: true`. Mark boards as `recommended: true` based on the project description (display/UI mentioned → recommend display boards; sensors only → recommend no-display boards; unclear → recommend `esp32-nodisplay`).

   If the developer types a board not in this list via freeform input, ask a follow-up for its FQBN string.

5. **Copyright holder** for the LICENSE file (e.g. `Acme Corp` or `Jane Doe`). Free-text question.

## Step 2: Apply Changes

After gathering all inputs, apply the following changes **without asking for further confirmation**:

### 2.1 Set version to 0.1.0

Edit `src/version.h` — change the version defines:
```
#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0
```

### 2.2 Clean the changelog

Replace the entire contents of `CHANGELOG.md` with a fresh changelog. Use the project display name in context. Keep the Keep-a-Changelog format:

```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- Initial project setup from [esp32-template-wifi](https://github.com/jantielens/esp32-template-wifi) template

## [0.1.0] - YYYY-MM-DD

_Initial release._
```

Replace `YYYY-MM-DD` with today's date.

### 2.3 Update config.sh

Edit `config.sh`:
- Set `PROJECT_NAME="<project-name>"` (the slug the developer provided)
- Set `PROJECT_DISPLAY_NAME="<Project Display Name>"` (the display name)
- Replace the `FQBN_TARGETS` associative array to only include the boards the developer selected. Preserve the FQBN strings and inline comments for each kept board. Keep the `DEFAULT_BOARD=""` line (the build system handles single-board auto-detection).

### 2.4 Replace README.md

Replace the entire `README.md` with a minimal project README:

```markdown
# <Project Display Name>

<Project description provided by the developer.>

Built on the [esp32-template-wifi](https://github.com/jantielens/esp32-template-wifi) template.

## Quick Start

```bash
# First-time setup (downloads arduino-cli, ESP32 core, libraries)
./setup.sh

# Build firmware
./build.sh

# Upload to device
./upload.sh

# Serial monitor
./monitor.sh
```

## Supported Boards

<List the boards the developer selected, one bullet per board with a short description.>

## Development

See the [template documentation](https://github.com/jantielens/esp32-template-wifi) for full details on:
- [Build & Release Process](docs/build-and-release-process.md)
- [Web Portal](docs/web-portal.md)
- [Display/Touch Architecture](docs/display-touch-architecture.md)
- [Script Reference](docs/scripts.md)
```

### 2.5 Update LICENSE

Edit the `LICENSE` file — replace the copyright line:
- From: `Copyright (c) 2025 Jan Tielens`
- To: `Copyright (c) <current-year> <copyright-holder>`

Use the current year and the copyright holder name the developer provided.

### 2.6 Clean up src/boards/

Remove board override directories under `src/boards/` that are **not** in the developer's selected board list. Use `rm -rf` for each directory to remove.

**Important**: Only remove directories whose name does NOT match any selected board. The remaining directories contain `board_overrides.h` files needed for compilation.

After removing directories, regenerate the board→driver table:
```bash
python3 tools/generate-board-driver-table.py --update-drivers-readme
```

### 2.7 Remove config.project.sh.example

Delete `config.project.sh.example` — it's a template convenience file not needed in a project repo. The project's config is now directly in `config.sh`.

### 2.8 Update copilot-instructions.md

Edit `.github/copilot-instructions.md`:

1. **Change the title** from `# Copilot Instructions for ESP32 Template Project` to `# Copilot Instructions for <Project Display Name>`

2. **Replace the Project Overview section** to make it clear this is a project repo, not a template:
   ```
   ## Project Overview

   <Project Display Name> — <project description>. ESP32 Arduino project using `arduino-cli` for headless builds. Built on the [esp32-template-wifi](https://github.com/jantielens/esp32-template-wifi) template. Designed for WSL2/Linux environments with local toolchain installation (no system dependencies).
   ```

3. **Update the Board Targets section** to only list the boards the developer selected (remove examples for boards not in this project).

4. **Remove the `config.project.sh.example` reference** from the Key Files section if present, since that file was deleted.

5. Do NOT rewrite the entire file — only update the sections above. The architecture docs, conventions, workflows, and guidelines all still apply.

## Step 3: Transition to Implementation

Print a brief summary of what was done:
- Project name, slug, and description configured
- Version set to 0.1.0
- Boards configured and unused board directories cleaned up
- README, LICENSE, CHANGELOG, config.sh, and copilot-instructions.md updated

Then **transition into implementation planning**:

1. **Analyze the project description** — Based on what the developer described, reason about what the firmware likely needs to do. Consider:
   - What sensors or hardware peripherals are implied?
   - Does it need MQTT, BLE, or other transport?
   - Does it need custom web portal pages or settings?
   - Does it need display UI screens?
   - What libraries might be needed (or removed from `arduino-libraries.txt`)?

2. **Scan the codebase** — Read `src/app/app.ino`, `arduino-libraries.txt`, and the selected board override files to understand the current starting point and what's already available.

3. **Ask clarifying questions** — Use `vscode_askQuestions` to ask targeted follow-up questions based on gaps in the project description. For example:
   - Which specific sensors/hardware will be connected? (model numbers, communication protocol)
   - What data should be published, and where? (MQTT topics, BLE telemetry, web dashboard)
   - Are there custom web portal settings needed? (configuration fields, UI pages)
   - Should the device operate in always-on or duty-cycle (sleep) mode?
   - Any Home Assistant integration requirements?
   - Anything else specific to the project?

   Tailor these questions to what's actually unclear — skip questions the description already answers.

4. **Propose an implementation plan** — Based on all gathered information, present a phased implementation plan with concrete steps. For example:
   - Phase 1: Core sensor reading and data model
   - Phase 2: Transport (MQTT/BLE publishing)
   - Phase 3: Web portal customization
   - Phase 4: Display UI (if applicable)

   Ask the developer to confirm or adjust the plan before starting work.
