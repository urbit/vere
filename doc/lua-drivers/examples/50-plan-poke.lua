--  50-plan-poke.lua — the round trip into the main system. After 2s this
--  driver injects an event into Arvo with ctx:plan: a dill %flog %text (the
--  kernel's "print a line to the console" path). The event travels
--  Lua -> ctx:plan -> u3_auto -> serf -> Arvo -> dill. On a ship with an
--  attached terminal the text prints; headless it's delivered to dill silently.
--  (See doc/lua-drivers/10-plan-injection.md for how delivery to the target
--  vane is verified directly via the stack trace.)

local d = { name = "poke", priority = 50 }

function d.talk(ctx)
  ctx:log("injecting a dill %flog %text into Arvo in 2s")
  ctx:after(2000, function(c)
    local tape = noun.tape("hello from a lua driver, via ctx:plan!")
    local task = noun.cell(noun.cord("text"), tape)   -- [%text tape]
    local card = noun.cell(noun.cord("flog"), task)   -- [%flog [%text tape]]
    c:plan("d", noun.list("lua"), card)               -- -> dill (vane %d)
    c:log("injected")
  end)
end

function d.kick(ctx, wire, card)
  return false
end

return d
