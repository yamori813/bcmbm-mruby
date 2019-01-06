begin

yabm = YABM.new(YABM::MODULE_BCM4712)

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
  start = yabm.count() 
  while yabm.count() < start + 1000 do
  end
yabm.udpsend(dist, 514, "Hello", 5)
end

# recive test
# echo -n "MORIMORI" | nc -w 0 -u 10.10.10.123 7000

yabm.udpbind(7000)

i = 0
while 1 do
  yabm.print "."
  udpstr = yabm.udprecv()
  start = yabm.count() 
  while yabm.count() < start + 1000 do
  end
  if udpstr.length != 0 then
    yabm.print udpstr
    i = i + 1
  end
end

rescue => e
  yabm.print e.to_s
end
