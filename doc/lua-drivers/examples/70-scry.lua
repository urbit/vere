--  70-scry.lua — read Arvo's namespace with ctx:scry (the read-side complement
--  to ctx:plan; kept in the runtime since it needs the pier/serf, not libuv).
--  care is the vane+care mote ("cy" arch, "cz" hash, "cx" file); desk is a desk
--  string or nil; path is "/a/b/c". Ship and case (now) are injected.

local d = { name = "scry", priority = 70 }

function d.talk(ctx)
  d.timer = uv.new_timer()
  d.timer:start(1000, 0, function()
    ctx:scry("cy", "base", "/", function(res)
      if res then
        ctx:log("scry %cy base / -> result (cell=" .. tostring(noun.is_cell(res))
                .. " mug=" .. noun.mug(res) .. ")")
      else
        ctx:log("scry %cy base / -> nil")
      end
    end)
    ctx:scry("cz", "base", "/", function(res)
      ctx:log("scry %cz base / -> " .. (res and ("mug=" .. noun.mug(res)) or "nil"))
    end)
    d.timer:close()
  end)
end

function d.kick(ctx, wire, card)
  return false
end

return d
