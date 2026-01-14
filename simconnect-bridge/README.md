# SimConnect Bridge

Small WebSocket bridge that reads MSFS SimConnect data and streams it to the Electron app.

## Setup
1) Install the MSFS SDK (Developer Mode -> SDK -> Install).
2) Copy the DLLs from the SDK:
   - Managed: `.../MSFS SDK/SimConnect SDK/lib/managed/Microsoft.FlightSimulator.SimConnect.dll`
     -> `simconnect-bridge/lib/Microsoft.FlightSimulator.SimConnect.dll`
   - Native: `.../MSFS SDK/SimConnect SDK/lib/SimConnect.dll`
     -> `simconnect-bridge/lib/SimConnect.dll`
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
