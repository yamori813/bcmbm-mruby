#
# yabmbm-mruby mruby script
# need compile with subroutine file
#
#

APIKEY = "naisyo"

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

#start = yabm.count()
#while yabm.count() < start + 3 * 1000 do
#end
#ntpaddr6 = yabm.lookup6("ntp.nict.jp")
#yabm.print ntpaddr6 + "\r\n"
#yabm.sntp(ntpaddr6)

count = 0
interval = 30

yabm.watchdogstart(300)

while 1 do
  count = count + 1
  ledon yabm
  yabm.print count.to_s
  res = SimpleHttp.new("https", "api.thingspeak.com", 443).request("GET", "/update?api_key=" + APIKEY + "&field1=" + count.to_s, {'User-Agent' => "test-agent"})
  ledoff yabm
  yabm.print "." + "\r\n"
  start = yabm.count()
  while yabm.count() < start + interval * 1000 do
  end
  yabm.watchdogreset
end

rescue => e
  yabm.print e.to_s
end
