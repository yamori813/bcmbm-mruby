#
# mruby on yabm script
#
# 
#

begin

yabm = YABM.new

yabm.netstartdhcp

# sync date by ntp use https X.509
#ntpaddr = yabm.lookup("ntp.nict.jp")
#yabm.sntp(ntpaddr)

yabm.msleep 3_000
ntpaddr6 = yabm.lookup6("ntp.nict.jp")
yabm.print ntpaddr6 + "\r\n"
yabm.sntp(ntpaddr6)

ipv6test = yabm.lookup6("v6.ipv6-test.com")
yabm.print ipv6test + "\r\n"

count = 0
interval = 20

yabm.watchdogstart(100)

loop do
  count += 1
  yabm.print count.to_s
  res = SimpleHttp.new("https", "v6.ipv6-test.com", 443, 1).request("GET", "/api/myip.php", {'User-Agent' => "test-agent"})
  yabm.print " " + res.body.to_s + "\r\n"
  yabm.msleep interval * 1000
  yabm.watchdogreset
end

rescue => e
  yabm.print e.to_s
end
