using System.Diagnostics;
using System.Net;
using System.Net.WebSockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using Microsoft.FlightSimulator.SimConnect;

internal static class Program
{
  private const int DefaultPort = 13375;
  private const int DefaultRateHz = 2;

  public static async Task Main(string[] args)
  {
    var settings = BridgeSettings.Parse(args, DefaultPort, DefaultRateHz);
    Console.WriteLine($"[Bridge] SimConnect WebSocket: ws://{settings.Host}:{settings.Port}");
    Console.WriteLine($"[Bridge] Rate: {settings.RateHz} Hz");

    using var hub = new WebSocketHub(settings.Host, settings.Port);
    await hub.StartAsync();

    using var cts = new CancellationTokenSource();
    Console.CancelKeyPress += (_, e) =>
    {
      e.Cancel = true;
      cts.Cancel();
    };

    var simWorker = new SimConnectWorker(hub, settings.RateHz);
    await simWorker.RunAsync(cts.Token);

    await hub.StopAsync();
  }
}

internal sealed class BridgeSettings
{
  public string Host { get; set; } = "127.0.0.1";
  public int Port { get; set; } = 13375;
  public int RateHz { get; set; } = 2;

  public static BridgeSettings Parse(string[] args, int defaultPort, int defaultRate)
  {
    var settings = new BridgeSettings
    {
      Port = defaultPort,
      RateHz = defaultRate
    };

    for (int i = 0; i < args.Length; i++)
    {
      string arg = args[i];
      if (arg == "--host" && i + 1 < args.Length)
      {
        settings.Host = args[++i];
      }
      else if (arg == "--port" && i + 1 < args.Length && int.TryParse(args[++i], out int port))
      {
        settings.Port = port;
      }
      else if (arg == "--rate" && i + 1 < args.Length && int.TryParse(args[++i], out int rate))
      {
        settings.RateHz = Math.Max(1, rate);
      }
    }

    return settings;
  }
}

internal sealed class WebSocketHub : IDisposable
{
  private readonly HttpListener _listener;
  private readonly List<WebSocket> _clients = new();
  private readonly object _lock = new();
  private readonly string _url;
  private CancellationTokenSource? _cts;

  public WebSocketHub(string host, int port)
  {
    _listener = new HttpListener();
    _url = $"http://{host}:{port}/";
    _listener.Prefixes.Add(_url);
  }

  public async Task StartAsync()
  {
    _cts = new CancellationTokenSource();
    _listener.Start();
    Console.WriteLine($"[Bridge] Listening on {_url}");
    _ = Task.Run(() => AcceptLoop(_cts.Token));
    await Task.CompletedTask;
  }

  public async Task StopAsync()
  {
    if (_cts != null)
    {
      _cts.Cancel();
      _cts.Dispose();
      _cts = null;
    }

    lock (_lock)
    {
      foreach (var ws in _clients)
      {
        try
        {
          ws.Abort();
        }
        catch
        {
          // Ignore cleanup errors.
        }
      }
      _clients.Clear();
    }

    if (_listener.IsListening)
    {
      _listener.Stop();
    }

    await Task.CompletedTask;
  }

  private async Task AcceptLoop(CancellationToken token)
  {
    while (!token.IsCancellationRequested)
    {
      HttpListenerContext? context = null;
      try
      {
        context = await _listener.GetContextAsync();
      }
      catch when (token.IsCancellationRequested)
      {
        break;
      }
      catch (Exception ex)
      {
        Console.WriteLine($"[Bridge] Listener error: {ex.Message}");
        await Task.Delay(500, token);
        continue;
      }

      if (context == null)
      {
        await Task.Delay(50, token);
        continue;
      }

      if (!context.Request.IsWebSocketRequest)
      {
        context.Response.StatusCode = 400;
        context.Response.Close();
        continue;
      }

      try
      {
        var wsContext = await context.AcceptWebSocketAsync(null);
        var socket = wsContext.WebSocket;
        lock (_lock)
        {
          _clients.Add(socket);
        }
        Console.WriteLine("[Bridge] WebSocket client connected");
        _ = Task.Run(() => ClientLoop(socket, token));
      }
      catch (Exception ex)
      {
        Console.WriteLine($"[Bridge] WebSocket accept error: {ex.Message}");
      }
    }
  }

  private async Task ClientLoop(WebSocket socket, CancellationToken token)
  {
    var buffer = new byte[256];
    try
    {
      while (!token.IsCancellationRequested && socket.State == WebSocketState.Open)
      {
        var result = await socket.ReceiveAsync(new ArraySegment<byte>(buffer), token);
        if (result.MessageType == WebSocketMessageType.Close)
        {
          break;
        }
      }
    }
    catch
    {
      // Ignore client errors.
    }
    finally
    {
      lock (_lock)
      {
        _clients.Remove(socket);
      }
      try
      {
        socket.Abort();
      }
      catch
      {
        // Ignore cleanup errors.
      }
      Console.WriteLine("[Bridge] WebSocket client disconnected");
    }
  }

  public void Broadcast(string message)
  {
    var data = Encoding.UTF8.GetBytes(message);
    List<WebSocket> clients;
    lock (_lock)
    {
      clients = _clients.ToList();
    }

    foreach (var socket in clients)
    {
      if (socket.State != WebSocketState.Open)
      {
        lock (_lock)
        {
          _clients.Remove(socket);
        }
        continue;
      }

      _ = socket.SendAsync(new ArraySegment<byte>(data), WebSocketMessageType.Text, true, CancellationToken.None);
    }
  }

  public void Dispose()
  {
    _listener.Close();
  }
}

internal sealed class SimConnectWorker
{
  private readonly WebSocketHub _hub;
  private readonly int _sendIntervalMs;
  private readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions
  {
    DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
  };

  private long _nextSendMs;
  private readonly Stopwatch _timer = Stopwatch.StartNew();

  public SimConnectWorker(WebSocketHub hub, int rateHz)
  {
    _hub = hub;
    _sendIntervalMs = Math.Max(200, 1000 / Math.Max(1, rateHz));
  }

  public async Task RunAsync(CancellationToken token)
  {
    while (!token.IsCancellationRequested)
    {
      try
      {
        using var simConnect = new SimConnect("Tab5 SimConnect Bridge", IntPtr.Zero, 0, null, 0);
        simConnect.OnRecvOpen += OnOpen;
        simConnect.OnRecvQuit += (_, _) => throw new SimConnectDisconnectedException();
        simConnect.OnRecvException += OnException;
        simConnect.OnRecvSimobjectData += OnData;

        SetupDefinitions(simConnect);

        Console.WriteLine("[Bridge] Connected to SimConnect");

        while (!token.IsCancellationRequested)
        {
          simConnect.ReceiveMessage();
          Thread.Sleep(5);
        }
      }
      catch (SimConnectDisconnectedException)
      {
        Console.WriteLine("[Bridge] SimConnect disconnected");
      }
      catch (DllNotFoundException)
      {
        Console.WriteLine("[Bridge] SimConnect.dll not found. Copy it into simconnect-bridge/lib/.");
      }
      catch (Exception ex)
      {
        Console.WriteLine($"[Bridge] SimConnect error: {ex.Message}");
      }

      if (token.IsCancellationRequested)
      {
        break;
      }

      await Task.Delay(2000, token);
    }
  }

  private void SetupDefinitions(SimConnect simConnect)
  {
    simConnect.AddToDataDefinition(DataDefinitionId.SimData, "INDICATED AIRSPEED", "knots", SIMCONNECT_DATATYPE.FLOAT64, 0.0f, SimConnect.SIMCONNECT_UNUSED);
    simConnect.AddToDataDefinition(DataDefinitionId.SimData, "PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE.FLOAT64, 0.0f, SimConnect.SIMCONNECT_UNUSED);
    simConnect.AddToDataDefinition(DataDefinitionId.SimData, "VERTICAL SPEED", "feet per minute", SIMCONNECT_DATATYPE.FLOAT64, 0.0f, SimConnect.SIMCONNECT_UNUSED);
    simConnect.AddToDataDefinition(DataDefinitionId.SimData, "PLANE HEADING DEGREES MAGNETIC", "degrees", SIMCONNECT_DATATYPE.FLOAT64, 0.0f, SimConnect.SIMCONNECT_UNUSED);
    simConnect.AddToDataDefinition(DataDefinitionId.SimData, "FUEL TOTAL QUANTITY", "gallons", SIMCONNECT_DATATYPE.FLOAT64, 0.0f, SimConnect.SIMCONNECT_UNUSED);
    simConnect.AddToDataDefinition(DataDefinitionId.SimData, "SIM ON GROUND", "bool", SIMCONNECT_DATATYPE.INT32, 0.0f, SimConnect.SIMCONNECT_UNUSED);

    simConnect.RegisterDataDefineStruct<SimData>(DataDefinitionId.SimData);

    simConnect.RequestDataOnSimObject(
      DataRequestId.SimData,
      DataDefinitionId.SimData,
      SimConnect.SIMCONNECT_OBJECT_ID_USER,
      SIMCONNECT_PERIOD.SIM_FRAME,
      SIMCONNECT_DATA_REQUEST_FLAG.DEFAULT,
      0,
      0,
      0);
  }

  private void OnOpen(SimConnect sender, SIMCONNECT_RECV_OPEN data)
  {
    Console.WriteLine($"[Bridge] SimConnect: {data.szApplicationName}");
  }

  private void OnException(SimConnect sender, SIMCONNECT_RECV_EXCEPTION data)
  {
    Console.WriteLine($"[Bridge] SimConnect exception: {data.dwException}");
  }

  private void OnData(SimConnect sender, SIMCONNECT_RECV_SIMOBJECT_DATA data)
  {
    if ((DataRequestId)data.dwRequestID != DataRequestId.SimData)
    {
      return;
    }

    if (_timer.ElapsedMilliseconds < _nextSendMs)
    {
      return;
    }
    _nextSendMs = _timer.ElapsedMilliseconds + _sendIntervalMs;

    var simData = (SimData)data.dwData[0];
    var sensors = new List<Sensor>
    {
      new Sensor("sim.ias", "IAS", "kt", Round(simData.IndicatedAirspeed, 1)),
      new Sensor("sim.altitude", "Altitude", "ft", Round(simData.Altitude, 0)),
      new Sensor("sim.vs", "Vertical Speed", "fpm", Round(simData.VerticalSpeed, 0)),
      new Sensor("sim.heading", "Heading", "deg", Round(simData.HeadingMag, 1)),
      new Sensor("sim.fuel_total", "Fuel Total", "gal", Round(simData.FuelTotal, 1)),
      new Sensor("sim.on_ground", "On Ground", "", simData.OnGround)
    };

    var payload = new SimPayload
    {
      Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
      Sensors = sensors
    };

    var json = JsonSerializer.Serialize(payload, _jsonOptions);
    _hub.Broadcast(json);
  }

  private static double Round(double value, int digits)
  {
    if (double.IsNaN(value) || double.IsInfinity(value)) return 0;
    return Math.Round(value, digits);
  }
}

internal sealed class SimConnectDisconnectedException : Exception
{
}

internal enum DataDefinitionId
{
  SimData = 0
}

internal enum DataRequestId
{
  SimData = 0
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal struct SimData
{
  public double IndicatedAirspeed;
  public double Altitude;
  public double VerticalSpeed;
  public double HeadingMag;
  public double FuelTotal;
  public int OnGround;
}

internal sealed class Sensor
{
  [JsonPropertyName("entity_id")]
  public string EntityId { get; set; }

  [JsonPropertyName("name")]
  public string Name { get; set; }

  [JsonPropertyName("unit")]
  public string Unit { get; set; }

  [JsonPropertyName("value")]
  public object Value { get; set; }

  public Sensor(string entityId, string name, string unit, object value)
  {
    EntityId = entityId;
    Name = name;
    Unit = unit;
    Value = value;
  }
}

internal sealed class SimPayload
{
  [JsonPropertyName("type")]
  public string Type { get; set; } = "sim_metrics";

  [JsonPropertyName("timestamp")]
  public long Timestamp { get; set; }

  [JsonPropertyName("sensors")]
  public List<Sensor> Sensors { get; set; } = new();
}
