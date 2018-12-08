begin

rtl = RTL8196C.new(1)

addr = 10 << 24 | 0 << 16 | 1 << 8 | 140
mask = 255 << 24 | 255 << 16 | 255 << 8 | 0
gw = 10 << 24 | 0 << 16 | 1 << 8 | 1
dns = 10 << 24 | 0 << 16 | 1 << 8 | 1

rtl.netstart(addr, mask, gw, dns)

rtl.print "HTTP Test on BCM/mips" + "\r\n"

count = 1

while 1 do

  rtl.print "start http " + count.to_s + "\r\n"

  host = "httpbin.org"
  ip = rtl.lookup(host)
  rtl.print host + ":" + ip.to_s + "\r\n"
# http://httpbin.org/ip
  head = "GET /ip HTTP/1.1\r\nHost: " + host + "\r\n\r\n"
  res = rtl.http(ip, 80, head)
  rtl.print res

  rtl.print "start https " + count.to_s + "\r\n"

# https://httpbin.org/ip
  head = "GET /ip HTTP/1.1\r\nHost: " + host + "\r\n\r\n"
  res = rtl.https(host, ip, 443, head)
  rtl.print res

  count = count + 1

  start = rtl.count() 
  while rtl.count() < start + 5000 do
  end
end

rescue => e
  rtl.print e.to_s
end
