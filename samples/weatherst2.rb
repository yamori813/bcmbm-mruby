#
# yabmbm-mruby mruby script
#
# Weather Station used by  MPL115A2 and SHT30 on WCA-G
#

APIKEY = "naisyo"

NONET = false

# GPIO I2C Pin used SW3

SCL = 3
SDA = 5

LED10 = (1 << 6)

# i2c address

MPLADDR = 0x60
SHTADDR = 0x44

# utility function

def delay(yabm, val)
  start = yabm.count()
  while yabm.count() < start + val do
  end
end

def dot2str(p)
  pstr = (p / 100).to_s + "."
  syo = p % 100
  if syo < 10 then
    pstr = pstr + "0" + syo.to_s
  else
    pstr = pstr + syo.to_s
  end
  return pstr
end

def mplstr(pressure)
  frec = ((pressure & 0xf) * 1000) / 16
  if frec == 0  then
    pa =  (pressure >> 4).to_s + "000"
  elsif frec < 100
    pa =  (pressure >> 4).to_s + "0" + frec.to_s
  else
    pa =  (pressure >> 4).to_s + frec.to_s
  end
  return pa.insert(-3, ".")
end

class SHT3x
  def init yabm
    @y = yabm
  end
  def chkcrc dat
    crc8 = 0xff
    for i in 0..1 do
      crc8 = crc8 ^ dat[i]
      8.times {
        if crc8 & 0x80 == 0x80 then
          crc8 = crc8 << 1
          crc8 = crc8 ^ 0x31
        else
          crc8 = crc8 << 1
        end
      }
    end
    if (crc8 & 0xff) == dat[2] then
      return true
    else
      return false
    end
  end
  def getStatus
    @y.i2cwrite(SHTADDR, 0xf3, 0x2d)
    delay(@y, 100)
    arr = @y.i2creads(SHTADDR, 3)
    return (arr[0] << 8) | arr[1]
  end
  def getCelsiusAndHumidity
    while @y.i2cchk(SHTADDR) == 0 do
      delay(@y, 1)
    end
    @y.i2cwrite(SHTADDR, 0x24, 0x00)
    delay(@y, 500)
    while 1 do
      arr = @y.i2creads(SHTADDR, 6)
      if arr then
        break
      end
      delay(@y, 1)
    end
    t = ((arr[0] << 8) | arr[1]) * 17500 / 65535 - 4500
    h = ((arr[3] << 8) | arr[4]) * 10000 / 65535
    return t, h
  end
end

class MPL115
  def init yabm
    @y = yabm
    while @y.i2cchk(MPLADDR) == 0 do
      delay(@y, 1)
    end
    @a0 = @y.i2cread(MPLADDR, 0x04) << 8 | @y.i2cread(MPLADDR, 0x05)
    @b1 = @y.i2cread(MPLADDR, 0x06) << 8 | @y.i2cread(MPLADDR, 0x07)
    @b2 = @y.i2cread(MPLADDR, 0x08) << 8 | @y.i2cread(MPLADDR, 0x09)
    @c12 = @y.i2cread(MPLADDR, 0x0a) << 8 | @y.i2cread(MPLADDR, 0x0b)
  end

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

  def readPressure
    while @y.i2cchk(MPLADDR) == 0 do
      delay(@y, 1)
    end
    @y.i2cwrite(MPLADDR, 0x12, 0x01)
    delay(@y, 10)
    padc = @y.i2cread(MPLADDR, 0x00) << 8 | @y.i2cread(MPLADDR, 0x01)
    tadc = @y.i2cread(MPLADDR, 0x02) << 8 | @y.i2cread(MPLADDR, 0x03)

    pcomp = calculatePCompShort(padc, tadc, @a0, @b1, @b2, @c12)
    pressure = ((pcomp * 1041) >> 14) + 800
    return pressure
  end
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

sht = SHT3x.new
sht.init yabm

mpl = MPL115.new
mpl.init yabm

interval = 20
count = 0

while 1 do

  reg = yabm.gpiogetdat
  yabm.gpiosetdat(reg & ~LED10)

  t, h = sht.getCelsiusAndHumidity
  yabm.print count.to_s + " " + dot2str(t) + " " + dot2str(h) + " "

  pressure = mpl.readPressure
  yabm.print mplstr(pressure) + " "

  if !NONET then
    para = "api_key=" + APIKEY
    para = para + "&field1=" + count.to_s
    para = para + "&field2=" + dot2str(t)
    para = para + "&field3=" + dot2str(h)
    para = para + "&field4=" + mplstr(pressure)
    res = SimpleHttp.new("https", "api.thingspeak.com", 443).request("GET", "/update?" + para, {'User-Agent' => "test-agent"})
  end

  reg = yabm.gpiogetdat
  yabm.gpiosetdat(reg | LED10)

  delay(yabm, 1000 * interval)
  count = count + 1
end

rescue => e
  yabm.print e.to_s
end
