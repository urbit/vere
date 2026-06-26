--  20-net.lua — real network + filesystem I/O entirely through luv (`uv`):
--  UDP, TCP, unix sockets, DNS (getaddrinfo), an fs watcher, and async file IO.
--  None of this is hand-written in the runtime anymore — it's the luv library.

local d = { name = "net", priority = 20 }

local UPORT, TPORT = 39990, 39991

function d.talk(ctx)
  d.handles = {}

  --  UDP echo to self
  local u = uv.new_udp()
  u:bind("0.0.0.0", UPORT)
  u:recv_start(function(err, data, addr)
    if data then ctx:log("udp recv '" .. data .. "' from " .. addr.ip .. ":" .. addr.port) end
  end)
  d.handles.udp = u

  --  TCP echo server
  local srv = uv.new_tcp()
  srv:bind("0.0.0.0", TPORT)
  srv:listen(128, function()
    local c = uv.new_tcp(); srv:accept(c)
    c:read_start(function(err, data)
      if data then ctx:log("tcp server got '" .. data .. "'"); c:write("echo:" .. data) end
    end)
  end)
  d.handles.tcp = srv

  --  unix-domain echo server
  local sock = ctx:pier_path() .. "/lua-data/net.sock"
  uv.fs_mkdir(ctx:pier_path() .. "/lua-data", 448, function() end)   -- 0700
  pcall(function() uv.fs_unlink(sock) end)
  local psrv = uv.new_pipe(false)
  local ok = pcall(function()
    psrv:bind(sock)
    psrv:listen(128, function()
      local c = uv.new_pipe(false); psrv:accept(c)
      c:read_start(function(err, data)
        if data then ctx:log("pipe server got '" .. data .. "'"); c:write("pong") end
      end)
    end)
  end)
  ctx:log("listening: udp " .. UPORT .. ", tcp " .. TPORT .. ", pipe=" .. tostring(ok))
  d.handles.pipe = psrv

  --  fs watcher on a data file
  local wf = ctx:pier_path() .. "/lua-data/watched.txt"
  uv.fs_open(wf, "w", 420, function(err, fd)
    if fd then uv.fs_write(fd, "init", 0, function() uv.fs_close(fd, function() end) end) end
  end)
  local ev = uv.new_fs_event()
  ev:start(wf, {}, function(err, name) ctx:log("watch fired: " .. tostring(name)) end)
  d.handles.fsev = ev

  --  after a beat: DNS + clients + an async write that trips the watcher
  local t = uv.new_timer()
  t:start(600, 0, function()
    uv.getaddrinfo("localhost", nil, { family = "inet", socktype = "stream" },
      function(err, res)
        if res and res[1] then ctx:log("dns localhost -> " .. res[1].addr)
        else ctx:log("dns failed: " .. tostring(err)) end
      end)

    u:send("hello-udp", "127.0.0.1", UPORT)

    local cli = uv.new_tcp()
    cli:connect("127.0.0.1", TPORT, function()
      cli:read_start(function(err, data) if data then ctx:log("tcp client got '" .. data .. "'") end end)
      cli:write("ping")
    end)

    local pc = uv.new_pipe(false)
    pc:connect(sock, function(err)
      if not err then
        pc:read_start(function(e, data) if data then ctx:log("pipe client got '" .. data .. "'") end end)
        pc:write("ping")
      end
    end)

    uv.fs_open(wf, "w", 420, function(err, fd)
      if fd then uv.fs_write(fd, "changed", 0, function()
        uv.fs_close(fd, function() ctx:log("async wrote watched.txt") end)
      end) end
    end)

    t:close()
  end)
end

function d.kick(ctx, wire, card)
  return false
end

function d.exit(ctx)
  for _, h in pairs(d.handles or {}) do pcall(function() h:close() end) end
end

return d
