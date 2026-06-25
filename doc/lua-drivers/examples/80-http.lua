--  80-http.lua — the HTTP client (ctx:http), tested self-contained: the driver
--  stands up a tiny HTTP server with tcp_listen, then makes a GET to itself and
--  logs the status and body. ctx:http runs libcurl on the king's loop; opts may
--  carry { headers = {...}, body = "..." }; fn(status, body, headers).

local d = { name = "http", priority = 80 }

local PORT = 39080

function d.talk(ctx)
  ctx:tcp_listen(PORT, function(conn)
    conn:recv(function(req)
      local body = "hello-from-http"
      conn:send("HTTP/1.1 200 OK\r\n"
                .. "Content-Length: " .. #body .. "\r\n"
                .. "Connection: close\r\n\r\n" .. body)
    end)
  end)
  ctx:log("http server listening on :" .. PORT)

  ctx:after(700, function(c)
    c:http("GET", "http://127.0.0.1:" .. PORT .. "/", nil,
      function(status, body, headers)
        c:log("http GET -> status=" .. status .. " body='" .. body .. "'")
      end)
  end)
end

function d.kick(ctx, wire, card)
  return false
end

return d
