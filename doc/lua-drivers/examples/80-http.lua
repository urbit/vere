--  80-http.lua — the HTTP(S) client (ctx:http) is kept in the runtime because
--  luv has no TLS/HTTP. Here it's tested self-contained: a tiny HTTP server
--  built on luv TCP, then a GET to it via ctx:http. opts may carry
--  { headers = {...}, body = "..." }; fn(status, body, headers).

local d = { name = "http", priority = 80 }

local PORT = 39080

function d.talk(ctx)
  d.srv = uv.new_tcp()
  d.srv:bind("0.0.0.0", PORT)
  d.srv:listen(128, function()
    local c = uv.new_tcp(); d.srv:accept(c)
    c:read_start(function(err, req)
      if req then
        local body = "hello-from-http"
        c:write("HTTP/1.1 200 OK\r\nContent-Length: " .. #body
                .. "\r\nConnection: close\r\n\r\n" .. body)
      end
    end)
  end)
  ctx:log("http server (luv tcp) listening on :" .. PORT)

  local t = uv.new_timer()
  t:start(700, 0, function()
    ctx:http("GET", "http://127.0.0.1:" .. PORT .. "/", nil,
      function(status, body, headers)
        ctx:log("http GET -> status=" .. status .. " body='" .. body .. "'")
      end)
    t:close()
  end)
end

function d.kick(ctx, wire, card)
  return false
end

function d.exit(ctx)
  if d.srv then d.srv:close() end
end

return d
