begin

rtl = YABM.new(1)

addr = 10 << 24 | 0 << 16 | 1 << 8 | 140
mask = 255 << 24 | 255 << 16 | 255 << 8 | 0
gw = 10 << 24 | 0 << 16 | 1 << 8 | 1
dns = 10 << 24 | 0 << 16 | 1 << 8 | 1

rtl.netstart(addr, mask, gw, dns)

dist = 10 << 24 | 0 << 16 | 1 << 8 | 37

rtl.print "Hello Bear Metal mruby on BCM/mips"

# transfer test

rtl.udpinit
10.times do |i|
rtl.udpsend(dist, 514, "Hello", 5)
end

# recive test
# cho -n "MORIMORI" | nc -w 0 -u 10.0.1.140 7000

rtl.udpbind(7000)

i = 0
while i < 5 do
  udpstr = rtl.udprecv()
  if udpstr.length != 0 then
    rtl.print udpstr
    i = i + 1
  end
end

rescue => e
  rtl.print e.to_s
end
