begin

yabm = YABM.new

yabm.netstartdhcp

yabm.print "Hello Bear Metal mruby on BCM/mips\r\n"

count = 1

yabm.print yabm.getaddress + "\r\n"

# sync date by ntp use https X.509
ntpaddr = yabm.lookup("ntp.nict.jp")
yabm.sntp(ntpaddr)

while 1 do

  yabm.print "simple https " + count.to_s + "\r\n"

  s = SimpleHttp.new("https", "httpbin.org", 443)
  if s
    res = s.request("GET", "/ip", {'User-Agent' => "test-agent"})
    if res.status.to_s == ""
      yabm.print "ERROR\r\n"
    else
      yabm.print "GET done " + res.status.to_s + "\r\n"
    end
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
