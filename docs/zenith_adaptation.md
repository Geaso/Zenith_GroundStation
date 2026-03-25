# Zenith Adaptation Notes

The Qt prototype is now aligned with the Zenith codebase located at:

- `C:/Users/13655/Downloads/zenith/zenith_communication/shard/include/communication.hpp`
- `C:/Users/13655/Downloads/zenith/zenith_communication/shard/include/Struct.hpp`
- `C:/Users/13655/Downloads/zenith/zenith_communication/src/communication_bridge.cpp`
- `C:/Users/13655/Downloads/zenith/zenith_communication/launch/bridge.launch`

## Transport Defaults

- UDP telemetry port: `8889`
- TCP command port: `55555`
- TCP heartbeat port: `55556`
- Local service UDP port: `20168`
- Default ground station IP: `127.0.0.1`
- Default multicast IP: `224.0.0.88`

## Main Protocol Baseline

- package namespace `zenith_*`
- topic prefix `/zenith`
- message ID layout follows the Zenith communication bridge

## Notable Zenith Details

- `UAVState::LocationSource` adds `ODIN = 12` and `PROSIM = 13`
- ROS package namespace is `zenith_msgs`
- ground station topic roots stay under `/zenith/...`
- script loading uses `/uav{id}/zenith/load_cmd`

## Integration Target

The next implementation pass should replace the demo state generator with:

1. `ProtocolClient`
2. `ZenithCodec`
3. `TelemetryStore`
4. `CommandDispatcher`
