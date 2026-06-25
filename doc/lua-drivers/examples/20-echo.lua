--  20-echo.lua — a Lua IO driver that consumes effects. Any effect whose wire
--  begins with the `%echo` knot is logged and claimed (so it stops travelling
--  down the driver chain); everything else is passed through untouched.
--
--  Demonstrates the `kick(ctx, wire, card)` side of the contract and the noun
--  inspection API. `priority = 20` puts it after `10-tick.lua` in the chain.

local d = { name = "echo", priority = 20 }

function d.kick(ctx, wire, card)
  if noun.is_cell(wire) and noun.eq(noun.head(wire), noun.cord("echo")) then
    ctx:log("claimed an %echo effect")
    return true                -- handled; don't pass to later drivers
  end
  return false                 -- not ours; let the next driver try
end

return d
