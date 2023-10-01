begin

yabm = YABM.new

addr = "10.10.10.123"
mask = "255.255.255.0"
gw = "10.10.10.3"
dns = "10.10.10.3"

yabm.netstart(addr, mask, gw, dns)

dist = "10.10.10.3"

yabm.print "Hello Bear Metal mruby on BCM/mips"

# transfer test

yabm.udpinit
10.times do |i|
  yabm.print "*"
  yabm.msleep 1_000
yabm.udpsend(dist, 514, "Hello", 5)
end

# recive test
# echo -n "MORIMORI" | nc -w 0 -u 10.10.10.123 7000

yabm.udpbind(7000)

i = 0
loop do
  yabm.print "."
  udpstr = yabm.udprecv()
  yabm.msleep 1_000
  if udpstr.length != 0 then
    yabm.print udpstr
    i = i + 1
  end
end

rescue => e
  yabm.print e.to_s
end
