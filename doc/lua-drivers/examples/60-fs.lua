--  60-fs.lua — pier-scoped filesystem access. A driver can read, write, list,
--  and stat files anywhere under its pier (paths are relative to the pier root;
--  ".." and absolute paths are rejected). Useful for driver config, persistent
--  state, scratch files, or inspecting the pier.
--
--  NB: write to your own subdir (here `lua-data/`), NOT the watched `lua/`
--  folder — changing a file in `lua/` triggers a hot reload.

local d = { name = "fs", priority = 60 }

function d.talk(ctx)
  ctx:log("pier root is " .. ctx:pier_path())

  ctx:mkdir("lua-data")
  ctx:write("lua-data/state.txt", "written by a lua driver")
  ctx:log("read back: " .. (ctx:read("lua-data/state.txt") or "<nil>"))

  local entries = ctx:list(".")
  ctx:log("pier root has " .. #entries .. " entries")

  local urb = ctx:stat(".urb")
  if urb then
    ctx:log(".urb is a " .. urb.kind)
  end
end

function d.kick(ctx, wire, card)
  return false
end

return d
