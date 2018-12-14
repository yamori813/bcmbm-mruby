begin

yabm = YABM.new(1)

addr = "10.0.1.140"
mask = "255.255.255.0"
gw = "10.0.1.1"
dns = "10.0.1.1"

yabm.netstart(addr, mask, gw, dns)

yabm.print "Hello Bear Metal mruby on BCM/mips"

count = 1

while 1 do

  yabm.print "simple http " + count.to_s + "\r\n"

  s = SimpleHttp.new("http", "httpbin.org", 80)
  if s
    res = s.request("GET", "/ip", {'User-Agent' => "test-agent"})
    yabm.print "GET done " + res.status.to_s + "\r\n"
  else
    yabm.print "SimpleHttp error\r\n"
  end

  count = count + 1

  start = yabm.count() 
  while yabm.count() < start + 5000 do
  end
end

rescue => e
  yabm.print e.to_s
end
