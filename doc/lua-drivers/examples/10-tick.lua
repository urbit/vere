--  10-tick.lua — a Lua IO driver doing real async I/O via luv. The full libuv
--  API is available in every driver as the global `uv` (the same binding
--  Neovim exposes as vim.uv), running on the king's event loop. Here: a 1s
--  repeating timer.

local d = { name = "tick", priority = 10 }

function d.talk(ctx)
  ctx:log("live (luv " .. uv.version_string() .. "); starting a 1s timer")
  d.n = 0
  d.timer = uv.new_timer()
  d.timer:start(1000, 1000, function()
    d.n = d.n + 1
    ctx:log("tick " .. d.n)
  end)
end

function d.kick(ctx, wire, card)
  return false
end

function d.exit(ctx)
  if d.timer then d.timer:close() end
  ctx:log("exiting")
end

return d
