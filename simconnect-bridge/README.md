# SimConnect Bridge

Small WebSocket bridge that reads MSFS SimConnect data and streams it to the Electron app.

## Setup
1) Install the MSFS SDK (Developer Mode -> SDK -> Install).
2) Copy `SimConnect.dll` from the SDK to `simconnect-bridge/lib/SimConnect.dll`.
   Common SDK path: `.../MSFS SDK/SimConnect SDK/lib/SimConnect.dll`.
3) Build the bridge:

```
cd simconnect-bridge
dotnet build
```

## Run
```
cd simconnect-bridge
dotnet run
```

Options:
- `--host 127.0.0.1`
- `--port 13375`
- `--rate 2` (Hz)

The bridge serves WebSocket on `ws://127.0.0.1:13375` and emits messages like:
```
{
  "type": "sim_metrics",
  "timestamp": 1730000000000,
  "sensors": [
    {"entity_id":"sim.ias","name":"IAS","unit":"kt","value":120.5}
  ]
}
```
