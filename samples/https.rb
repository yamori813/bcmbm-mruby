begin

yabm = YABM.new

yabm.netstartdhcp

yabm.print "HTTPS Test on BCM/mips" + "\r\n"

ntpsvr = "ntp.nict.jp"
ntpaddr = yabm.lookup(ntpsvr)
yabm.print ntpsvr + ":" + ntpaddr + "\r\n"
yabm.sntp(ntpaddr)

host = "httpbin.org"

ip = yabm.lookup(host)

yabm.print host + ":" + ip + "\r\n"

count = 1

loop do

  yabm.print "start https " + count.to_s + "\r\n"

# https://httpbin.org/ip
  head = "GET /ip HTTP/1.1\r\nHost: " + host + "\r\n\r\n"
  res = yabm.https(host, ip, 443, head)
  yabm.print res

  count = count + 1

  yabm.msleep 10_000
end

rescue => e
  yabm.print e.to_s
end
