begin

yabm = YABM.new

addr = "10.0.1.140"
mask = "255.255.255.0"
gw = "10.0.1.1"
dns = "10.0.1.1"
ntpaddr = "10.0.1.18"

yabm.netstart(addr, mask, gw, dns)

yabm.print "HTTP Test on BCM/mips" + "\r\n"

yabm.sntp(ntpaddr)

host = "httpbin.org"

ip = yabm.lookup(host)

yabm.print host + ":" + ip + "\r\n"

count = 1

while 1 do

  yabm.print "start https " + count.to_s + "\r\n"

# https://httpbin.org/ip
  head = "GET /ip HTTP/1.1\r\nHost: " + host + "\r\n\r\n"
  res = yabm.https(host, ip, 443, head)
  yabm.print res

  count = count + 1

  start = yabm.count() 
  while yabm.count() < start + 10000 do
  end
end

rescue => e
  yabm.print e.to_s
end
