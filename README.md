# Bleedout Execution (SKSE Plugin)

**Bleedout Execution** is an SKSE plugin for Skyrim Special Edition / Anniversary Edition that allows players to perform killmoves (executions) on enemies that are in a **Bleedout** or **Getting Up** state.

By default, the Skyrim engine handles bleedout NPCs very poorly when it comes to killmoves: any hit to a bleedout NPC triggers an instant ragdoll death, breaking the paired killmove animation synchronization between the player and the NPC. This plugin bypasses the hardcoded engine limits using specialized hooks and task queues to seamlessly lock both actors into a killmove animation without instant deaths or desync issues.

## 🌟 Features

* **Bleedout & GetUp Execution:** Seamlessly triggers killmoves on enemies crawling on the ground (Bleedout) or currently trying to stand back up (GetUp).
* **Pre-Hit & Post-Hit Triggers:** Utilizes both `AttackActionHook` (triggers *before* the weapon swings to prevent physics collision issues) and `ProcessHitHook` to guarantee stable execution.
* **Deferred Task Queuing:** Heavily inspired by *Valhalla Combat*, the plugin offloads the actual `_playPairedIdle` execution to the `SKSE::GetTaskInterface()` game thread queue. This completely solves the infamous engine bug where NPCs die instantly before the animation finishes.
* **Configurable:** JSON-based configuration allows you to tweak execution probabilities, skip essential/protected NPCs, and toggle debug logging.

## 📦 Requirements

To use this mod, you need:
* Skyrim Special Edition or Anniversary Edition
* [SKSE64](https://skse.silverlock.org/)
* [Kaputt - The Ultimate Killmove Manager](https://www.nexusmods.com/skyrimspecialedition/mods/67012) (Required for animation configuration and tags. Only the .esp plugin and SKSE\plugins\Kaputt\anims\vanilla.json are needed)

To build the project from source, you need:
* C++23 Compiler (MSVC)
* [xmake](https://xmake.io/) (Build system)
* [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)

## ⚙️ Configuration (`BleedoutExecution.json`)

The plugin generates a configuration file at `Data\SKSE\Plugins\BleedoutExecution.json`.

```json
{
    "enabled": true,
    "enable_bleedout_execution": true,
    "enable_getup_execution": true,
    "execution_probability": 100.0,
    "essential_protection": true,
    "protected_protection": true,
    "height_diff_range": [-35.0, 35.0],
    "last_hostile_check": true,
    "last_hostile_range": 1024.0,
    "banned_tags": ["adv", "sneak"],
    "skipped_race": [
        "FrostbiteSpiderRaceGiant",
        "SprigganMatronRace"
        // ... (other skipped races)
    ],
    "enable_debug_log": false
}
```

**Key Settings:**
* `enable_debug_log`: Set to `true` to print verbose animation filtering and trigger logs to `Documents\My Games\Skyrim Special Edition\SKSE\BleedoutExecution.log`.
* `execution_probability`: Chance (0 to 100) that a valid execution will trigger.
* `essential_protection` / `protected_protection`: Prevents accidental executions on essential or protected NPCs.

## 🛠️ Building from Source

This project uses `xmake`. 
1. Clone the repository.
2. Initialize the `CommonLibSSE-NG` submodule (or place it in the `extern` folder).
3. Open a terminal in the project root and run:
   ```cmd
   xmake build
   ```
4. The compiled `.dll` will be available in the `build/windows/x64/releasedbg/` directory.

## 🧠 Technical Details

The core challenge of this mod was circumventing Skyrim's hardcoded execution handlers.
1. `ProcessHitHook`: Replaces the vanilla damage calculation. If the victim is in a valid execution state, it skips the vanilla damage output entirely to prevent the engine from applying ragdoll physics that breaks the animation.
2. `AttackActionHook`: Catches the player's attack input *before* the weapon swing actually starts.
3. **Task Queuing (`SKSE::GetTaskInterface`)**: Calling the engine's `_playPairedIdle` function mid-combat calculation breaks the game state. The plugin defers this call to the next safe game loop tick, ensuring perfect synchronization between the player and the NPC.

## 🙏 Acknowledgements & Credits

* **[Kaputt](https://github.com/Pentalimbed/kaputt** by Pentalimbed (ProfJack): This plugin utilizes Kaputt's animation file structure, IdleTagger concept, and logic for determining the correct paired idle conditions.
* **[Valhalla Combat](https://github.com/dTry/ValhallaCombat)** by dTry: The task-queued paired idle execution method, which single-handedly solved the "instant death on killmove" engine bug, was referenced from Valhalla Combat's brilliant execution handler.
* The **SKSE/CommonLibSSE** community.

## 📄 License
This project is licensed under the MIT License.
