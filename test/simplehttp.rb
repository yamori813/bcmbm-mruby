begin

rtl = RTL8196C.new(1)

addr = 10 << 24 | 0 << 16 | 1 << 8 | 140
mask = 255 << 24 | 255 << 16 | 255 << 8 | 0
gw = 10 << 24 | 0 << 16 | 1 << 8 | 1
dns = 10 << 24 | 0 << 16 | 1 << 8 | 1

rtl.netstart(addr, mask, gw, dns)

dist = 10 << 24 | 0 << 16 | 1 << 8 | 37

rtl.print "Hello Bear Metal mruby on BCM/mips"

count = 1

while 1 do

  rtl.print "simple http " + count.to_s + "\r\n"

  s = SimpleHttp.new("http", "httpbin.org", 80)
  if s
    res = s.request("GET", "/ip", {'User-Agent' => "test-agent"})
    rtl.print "GET done\r\n"
  else
    rtl.print "SimpleHttp error\r\n"
  end

  count = count + 1

  start = rtl.count() 
  while rtl.count() < start + 5000 do
  end
end

rescue => e
  rtl.print e.to_s
end
