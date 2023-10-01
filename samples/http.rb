begin

yabm = YABM.new

yabm.netstartdhcp

yabm.print "HTTP Test on BCM/mips" + "\r\n"

host = "httpbin.org"

ip = yabm.lookup(host)

yabm.print host + ":" + ip + "\r\n"

count = 1

loop do

  yabm.print "start http " + count.to_s + "\r\n"

# http://httpbin.org/ip
  head = "GET /ip HTTP/1.1\r\nHost: " + host + "\r\n\r\n"
  res = yabm.http(ip, 80, head)
  yabm.print res

  count += 1

  yabm.msleep 10_000
end

rescue => e
  yabm.print e.to_s
end
