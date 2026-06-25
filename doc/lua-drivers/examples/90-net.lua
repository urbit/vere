--  90-net.lua — exercises hostname TCP (DNS), unix domain sockets, fs watching,
--  and async file IO in one driver, all self-contained.

local d = { name = "net", priority = 90 }

local TPORT = 39091

function d.talk(ctx)
  ctx:mkdir("lua-data")
  local sock = ctx:pier_path() .. "/lua-data/test.sock"
  ctx:remove("lua-data/test.sock")              -- in case a stale socket exists

  --  unix-domain echo server
  ctx:pipe_listen(sock, function(conn)
    conn:recv(function(data)
      ctx:log("pipe server got '" .. data .. "', replying")
      conn:send("pong")
    end)
  end)

  --  tcp server (target for the DNS/hostname connect below)
  ctx:tcp_listen(TPORT, function(conn)
    conn:recv(function(data) conn:send("dns-ok") end)
  end)

  --  fs watch on a data file
  ctx:write("lua-data/watched.txt", "initial")
  ctx:watch("lua-data/watched.txt", function(name, kind)
    ctx:log("watch fired: " .. name .. " (" .. kind .. ")")
  end)

  ctx:after(500, function(c)
    --  hostname TCP: "localhost" goes through uv_getaddrinfo (DNS)
    c:tcp_connect("localhost", TPORT, function(conn)
      conn:recv(function(data) c:log("tcp(localhost via DNS) got '" .. data .. "'") end)
      conn:send("hi")
    end)

    --  unix-domain client
    c:pipe_connect(sock, function(conn)
      conn:recv(function(data) c:log("pipe client got '" .. data .. "'") end)
      conn:send("ping")
    end)
  end)

  --  async write (off the loop) -> should also trip the watch above
  ctx:after(900, function(c)
    c:write_async("lua-data/watched.txt", "changed by async write", function(ok)
      c:log("write_async ok=" .. tostring(ok))
      c:read_async("lua-data/watched.txt", function(data)
        c:log("read_async got '" .. (data or "<nil>") .. "'")
      end)
    end)
  end)
end

function d.kick(ctx, wire, card)
  return false
end

return d
