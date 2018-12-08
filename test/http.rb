begin

yabm = YABM.new(1)

addr = 10 << 24 | 0 << 16 | 1 << 8 | 140
mask = 255 << 24 | 255 << 16 | 255 << 8 | 0
gw = 10 << 24 | 0 << 16 | 1 << 8 | 1
dns = 10 << 24 | 0 << 16 | 1 << 8 | 1

yabm.netstart(addr, mask, gw, dns)

yabm.print "HTTP Test on BCM/mips" + "\r\n"

count = 1

while 1 do

  yabm.print "start http " + count.to_s + "\r\n"

  host = "httpbin.org"
  ip = yabm.lookup(host)
  yabm.print host + ":" + ip.to_s + "\r\n"
# http://httpbin.org/ip
  head = "GET /ip HTTP/1.1\r\nHost: " + host + "\r\n\r\n"
  res = yabm.http(ip, 80, head)
  yabm.print res

  yabm.print "start https " + count.to_s + "\r\n"

# https://httpbin.org/ip
  head = "GET /ip HTTP/1.1\r\nHost: " + host + "\r\n\r\n"
  res = yabm.https(host, ip, 443, head)
  yabm.print res

  count = count + 1

  start = yabm.count() 
  while yabm.count() < start + 5000 do
  end
end

rescue => e
  yabm.print e.to_s
end
