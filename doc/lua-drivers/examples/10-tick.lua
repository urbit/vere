--  10-tick.lua — a Lua IO driver that performs real async I/O inside the
--  running ship: it owns a libuv timer in the king's event loop and fires
--  every second. This is the smallest demonstration that a user-dropped Lua
--  driver is genuinely live in the runtime (not just loaded).
--
--  Drop this into  $pier/lua/  and (re)start your ship; you'll see "tick N"
--  lines appear in the runtime log once per second.

local d = { name = "tick", priority = 10 }

function d.talk(ctx)
  ctx:log("live — starting a 1s repeating timer")
  d.n = 0
  ctx:every(1000, function(c)
    d.n = d.n + 1
    c:log("tick " .. d.n)
  end)
end

function d.kick(ctx, wire, card)
  return false                 -- this driver emits, it doesn't consume effects
end

function d.exit(ctx)
  ctx:log("exiting")
end

return d
