# GroundStationQt

Qt native ground station prototype for the single-UAV workflow.

The current prototype includes:

- `Overview`: status header, live video panel, quick actions, manual command area
- `Map / Track`: mission map canvas, track view, waypoint list, mission actions
- `Scripts`: four protected built-in task cards plus a persistent custom-task library with add/view/edit/delete, onboard absolute paths, start/stop ACK and lifecycle status

## Structure

- `src/`
  - `AppState.*`: demo telemetry store used by the UI prototype
  - `ScriptActionModel.*`: script action list model
  - `ZenithProtocol.h`: Zenith message IDs, topic constants and field notes
  - `ZenithProtocolProfile.*`: Qt-facing protocol profile and transport defaults
  - `main.cpp`: app entry
- `qml/`
  - `Main.qml`: shell layout
  - `pages/`: main views
  - `components/`: reusable UI building blocks
- `config/`
  - `script_actions.json`: read-only display metadata and stable IDs for the four built-in aircraft-managed tasks

Custom tasks are stored per Windows user in Qt's `AppDataLocation` as
`custom_tasks.json`, so rebuilding or replacing the portable executable does
not erase the user's task library.
  - `zenith_protocol.json`: transport and topic defaults extracted from the Zenith project
- `docs/`
  - `zenith_adaptation.md`: comparison and adaptation notes

## Build

This project expects Qt 5.15+ or Qt 6 and CMake.
The current environment did not expose `cmake` or Qt tools on `PATH`, so build verification could not be run here.

Example:

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH="C:/Qt/6.6.3/msvc2019_64"
cmake --build build
```

## Zenith Adaptation

The prototype is now aligned to `C:/Users/13655/Downloads/zenith` at the naming/profile level:

- namespace prefix: `/zenith`
- state topic: `/uav{id}/zenith/state`
- command topic: `/uav{id}/zenith/command`
- UDP: `8889`
- TCP: `55555`
- TCP heartbeat: `55556`

## Next Step

1. Replace `AppState` demo timers with a real `ProtocolClient`.
2. Implement Zenith-compatible encode/decode and message dispatch.
3. Feed `UAVState`, `TextInfo`, and `UAVControlState` into the UI stores.
4. Wire `UAVCommand`, `UAVSetup`, and script actions through a `CommandDispatcher`.
