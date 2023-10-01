#
# mruby on yabm script
# need compile with subroutine file
#
#

APIKEY = "naisyo"

WDTIMEOUT = 300

#
# main
#

begin

yabm = YABM.new

yabm.netstartdhcp

gpioinit yabm

# sync date by ntp use https X.509
ntpaddr = yabm.lookup("ntp.nict.jp")
yabm.sntp(ntpaddr)

#yabm.msleep 3_000
#ntpaddr6 = yabm.lookup6("ntp.nict.jp")
#yabm.print ntpaddr6 + "\r\n"
#yabm.sntp(ntpaddr6)

count = 0
interval = 30

yabm.watchdogstart(WDTIMEOUT)

loop do
  count = count + 1
  ledon yabm
  yabm.print count.to_s
  res = SimpleHttp.new("https", "api.thingspeak.com", 443).request("GET", "/update?api_key=" + APIKEY + "&field1=" + count.to_s, {'User-Agent' => "test-agent"})
  ledoff yabm
  yabm.print "." + "\r\n"
  yabm.msleep interval * 1000
  yabm.watchdogreset
end

rescue => e
  yabm.print e.to_s
end
