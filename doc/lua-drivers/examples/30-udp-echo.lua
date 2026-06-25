--  30-udp-echo.lua — real UDP I/O from a Lua driver. Opens a UDP socket,
--  then (via a 0.5s timer) sends itself a datagram; the receive callback fires
--  with the payload and sender address. Proves bidirectional socket I/O works
--  inside the running ship, on the king's libuv loop.
--
--  A real driver would `ctx:plan(...)` the received data into Arvo instead of
--  just logging it — see doc/lua-drivers/examples/50-plan-poke.lua.

local d = { name = "udp-echo", priority = 30 }

local PORT = 39990

function d.talk(ctx)
  local sock = ctx:udp_open(PORT, function(data, host, port)
    ctx:log("recv '" .. data .. "' from " .. host .. ":" .. port)
  end)
  ctx:log("listening on udp :" .. PORT)
  d.sock = sock

  ctx:after(500, function(c)
    d.sock:send("127.0.0.1", PORT, "hello-from-lua")
    c:log("sent a datagram to self")
  end)
end

function d.kick(ctx, wire, card)
  return false
end

function d.exit(ctx)
  ctx:log("exiting")
end

return d
