--  70-scry.lua — read Arvo's namespace with ctx:scry (the read-side complement
--  to ctx:plan). care is the vane+care mote ("cy" = clay arch, "cz" = clay
--  hash, "cx" = clay file); desk is a desk string (or nil for vane scries);
--  path is "/a/b/c". Ship and case (now) are injected automatically.

local d = { name = "scry", priority = 70 }

function d.talk(ctx)
  ctx:after(1000, function(c)
    c:scry("cy", "base", "/", function(res)
      if res then
        c:log("scry %cy base / -> result (cell=" .. tostring(noun.is_cell(res))
              .. " mug=" .. noun.mug(res) .. ")")
      else
        c:log("scry %cy base / -> nil")
      end
    end)

    c:scry("cz", "base", "/", function(res)
      c:log("scry %cz base / -> " .. (res and ("mug=" .. noun.mug(res)) or "nil"))
    end)
  end)
end

function d.kick(ctx, wire, card)
  return false
end

return d
