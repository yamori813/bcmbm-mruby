#
# yabmbm-mruby mruby script
#
# get pressure data from i2c mpl115a2 to update thingspeak channel
# on WCA-G

APIKEY = "naisyo"

NONET = true

# GPIO I2C Pin used SW3

SCL = 3
SDA = 5

LED10 = (1 << 6)

# i2c address

MPLADDR = 0x60

# utility function

# This calculate code is based c source code in NXP AN3785 document

def calculatePCompLong(padc, tadc, a0, b1, b2, c12)
  if a0 >= 0x8000 then
    a0 = a0 - 0x10000
  end
  if b1 >= 0x8000 then
    b1 = b1 - 0x10000
  end
  if b2 >= 0x8000 then
    b2 = b2 - 0x10000
  end
  if c12 >= 0x8000 then
    c12 = c12 - 0x10000
  end
  padc = padc >> 6
  tadc = tadc >> 6
# ******* STEP 1 : c12x2 = c12 * Tadc
  lt1 = c12
  lt2 = tadc
  lt3 = lt1 * lt2
  c12x2 = lt3 >> 11
# ******* STEP 2 : a1 = b1 + c12x2
  lt1 = b1
  lt2 = c12x2
  lt3 = lt1 + lt2
  a1 = lt3
# ******* STEP 3 : a1x1 = a1 * Padc
  lt1 = a1
  lt2 = padc
  lt3 = lt1 * lt2
  a1x1 = lt3
# ******* STEP 4 : y1 = a0 + a1x1
  lt1 = a0 << 10
  lt2 = a1x1
  lt3 = lt1 + lt2
  y1 = lt3
# ******* STEP 5 : a2x2 = b2 * Tadc
  lt1 = b2
  lt2 = tadc
  lt3 = lt1 * lt2;
  a2x2 = lt3 >> 1
# ******* STEP 6 : PComp = y1 + a2x2
  lt1 = y1
  lt2 = a2x2
  lt3 = lt1 + lt2
  pcomp = lt3 >> 9

  return pcomp
end

def calculatePCompShort(padc, tadc, a0, b1, b2, c12)
  if a0 >= 0x8000 then
    a0 = a0 - 0x10000
  end
  if b1 >= 0x8000 then
    b1 = b1 - 0x10000
  end
  if b2 >= 0x8000 then
    b2 = b2 - 0x10000
  end
  if c12 >= 0x8000 then
    c12 = c12 - 0x10000
  end
  padc = padc >> 6
  tadc = tadc >> 6
  c12x2 = (c12 * tadc) >> 11
  a1 = b1 + c12x2;
  a1x1 = a1 * padc
  y1 = (a0 << 10) + a1x1
  a2x2 = (b2 * tadc) >> 1
  pcomp = (y1 + a2x2) >> 9
  return pcomp
end

begin

# start processing

yabm = YABM.new

if !NONET then
  yabm.netstartdhcp

# sync date by ntp use https X.509
  ntpaddr = yabm.lookup("ntp.nict.jp")
  yabm.sntp(ntpaddr)
end

yabm.i2cinit(SCL, SDA, 1)

a0 = yabm.i2cread(MPLADDR, 1, 0x04) << 8 | yabm.i2cread(MPLADDR, 1, 0x05)
b1 = yabm.i2cread(MPLADDR, 1, 0x06) << 8 | yabm.i2cread(MPLADDR, 1, 0x07)
b2 = yabm.i2cread(MPLADDR, 1, 0x08) << 8 | yabm.i2cread(MPLADDR, 1, 0x09)
c12 = yabm.i2cread(MPLADDR, 1, 0x0a) << 8 | yabm.i2cread(MPLADDR, 1, 0x0b)

yabm.print "a0 = " + a0.to_s + "\n"
yabm.print "b1 = " + b1.to_s + "\n"
yabm.print "b2 = " + b2.to_s + "\n"
yabm.print "c12 = " + c12.to_s + "\n"

interval = 20
count = 0

loop do

  reg = yabm.gpiogetdat
  yabm.gpiosetdat(reg & ~LED10)

  while yabm.i2cchk(MPLADDR) == 0
    yabm.msleep(1)
  end
  yabm.i2cwrite(MPLADDR, 0x12, 0x01)
  yabm.msleep(100)

  padc = yabm.i2cread(MPLADDR, 1, 0x00) << 8 | yabm.i2cread(MPLADDR, 1, 0x01)
  tadc = yabm.i2cread(MPLADDR, 1, 0x02) << 8 | yabm.i2cread(MPLADDR, 1, 0x03)

  pcomp = calculatePCompShort(padc, tadc, a0, b1, b2, c12)
#  pcomp = calculatePCompLong(padc, tadc, a0, b1, b2, c12)
  pressure = ((pcomp * 1041) >> 14) + 800

  frec = ((pressure & 0xf) * 1000) / 16
  if frec == 0  then
    pa =  (pressure >> 4).to_s + "000"
  elsif frec < 100 
    pa =  (pressure >> 4).to_s + "0" + frec.to_s
  else
    pa =  (pressure >> 4).to_s + frec.to_s
  end
  hpa = pa.insert(-3, ".")
  yabm.print count.to_s + " " + hpa + " "

  if !NONET then
    res = SimpleHttp.new("https", "api.thingspeak.com", 443).request("GET", "/update?api_key=" + APIKEY + "&field1=" + hpa + "&field2=" + count.to_s, {'User-Agent' => "test-agent"})
  end

  reg = yabm.gpiogetdat
  yabm.gpiosetdat(reg | LED10)

  yabm.msleep(1000 * interval)
  count += 1
end

rescue => e
  yabm.print e.to_s
end
