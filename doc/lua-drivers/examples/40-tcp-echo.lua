--  40-tcp-echo.lua — real TCP I/O from a Lua driver. Listens on a port, then
--  (via a 0.5s timer) connects to itself and sends "ping"; the server accepts,
--  echoes "echo:ping" back, and the client logs it. Exercises the full TCP
--  path: listen / accept / connect / write / read, all on the king loop.

local d = { name = "tcp-echo", priority = 40 }

local PORT = 31338

function d.talk(ctx)
  ctx:tcp_listen(PORT, function(conn)
    ctx:log("server: accepted a connection")
    conn:recv(function(data)
      ctx:log("server: got '" .. data .. "', echoing")
      conn:send("echo:" .. data)
    end)
  end)
  ctx:log("listening on tcp :" .. PORT)

  ctx:after(500, function(c)
    ctx:tcp_connect("127.0.0.1", PORT, function(conn)
      c:log("client: connected to self, sending ping")
      conn:recv(function(data)
        c:log("client: got '" .. data .. "'")
      end)
      conn:send("ping")
    end)
  end)
end

function d.kick(ctx, wire, card)
  return false
end

function d.exit(ctx)
  ctx:log("exiting")
end

return d
