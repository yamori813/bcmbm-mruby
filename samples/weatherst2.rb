#
# yabmbm-mruby mruby script
#
# Weather Station used by  MPL115A2 and SHT30 on WCA-G
#

APIKEY = "naisyo"

NONET = true

# GPIO I2C Pin used SW3

SCL = 3
SDA = 5

LED10 = (1 << 6)

# i2c address

MPLADDR = 0x60
SHTADDR = 0x44

# utility function

def pointstr(p, c)
  if p == 0 then
    return "0." + "0" * c
  elsif p.abs < 10 ** c
    l = c - p.abs.to_s.length + 1
    s = p.to_s.insert(p < 0 ? 1 : 0, "0" * l)
    return s.insert(-1 - c, ".")
  else
    return p.to_s.insert(-1 - c, ".")
  end
end

# senser class

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
    @y.msleep(100)
    arr = @y.i2cread(SHTADDR, 3)
    return (arr[0] << 8) | arr[1]
  end
  def getCelsiusAndHumidity
    while @y.i2cchk(SHTADDR) == 0
      @y.msleep(1)
    end
    @y.i2cwrite(SHTADDR, 0x24, 0x00)
    @y.msleep(500)
    while true
      arr = @y.i2cread(SHTADDR, 6)
      if arr then
        break
      end
      @y.msleep(1)
    end
    t = ((arr[0] << 8) | arr[1]) * 17500 / 65535 - 4500
    h = ((arr[3] << 8) | arr[4]) * 10000 / 65535
    return t, h
  end
end

class MPL115
  def init yabm
    @y = yabm
    while @y.i2cchk(MPLADDR) == 0
      @y.msleep(1)
    end
    @a0 = @y.i2cread(MPLADDR, 1, 0x04) << 8 | @y.i2cread(MPLADDR, 1, 0x05)
    @b1 = @y.i2cread(MPLADDR, 1, 0x06) << 8 | @y.i2cread(MPLADDR, 1, 0x07)
    @b2 = @y.i2cread(MPLADDR, 1, 0x08) << 8 | @y.i2cread(MPLADDR, 1, 0x09)
    @c12 = @y.i2cread(MPLADDR, 1, 0x0a) << 8 | @y.i2cread(MPLADDR, 1, 0x0b)
  end

# This calculate code is based c source code in NXP AN3785 document

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
    while @y.i2cchk(MPLADDR) == 0
      @y.msleep(1)
    end
    @y.i2cwrite(MPLADDR, 0x12, 0x01)
    @y.msleep(10)
    padc = @y.i2cread(MPLADDR, 1, 0x00) << 8 | @y.i2cread(MPLADDR, 1, 0x01)
    tadc = @y.i2cread(MPLADDR, 1, 0x02) << 8 | @y.i2cread(MPLADDR, 1, 0x03)

    pcomp = calculatePCompShort(padc, tadc, @a0, @b1, @b2, @c12)
    pressure = ((pcomp * 1041) >> 14) + 800
    frec = ((pressure & 0xf) * 1000) / 16
    return ((pressure >> 4) * 1000) + frec
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

lastst = 0
lastsh = 0
lastmp = 0

yabm.watchdogstart(300)

loop do

  error = 0

  reg = yabm.gpiogetdat
  yabm.gpiosetdat(reg & ~LED10)

  t, h = sht.getCelsiusAndHumidity
  if count == 0 || (lastst - t).abs < 100 then
    lastst = t
  else
    t = lastst
    error = error | (1 << 0)
  end
  if count == 0 || (lastsh - h).abs < 2000 then
    lastsh = h
  else
    h = lastsh
    error = error | (1 << 1)
  end
  yabm.print count.to_s + " " + pointstr(t, 2) + " " + pointstr(h, 2) + " "

  p = mpl.readPressure
  if count == 0 || (lastmp - p).abs < 1000 then
    lastmp = p
  else
    p = lastmp
    error = error | (1 << 2)
  end
  yabm.print pointstr(p, 2) + " " + error.to_s + "\r\n"

  if !NONET then
    para = "api_key=" + APIKEY
    para = para + "&field1=" + count.to_s
    para = para + "&field2=" + pointstr(t, 2)
    para = para + "&field3=" + pointstr(h, 2)
    para = para + "&field4=" + pointstr(p, 2)
    para = para + "&field5=" + error.to_s
    res = SimpleHttp.new("https", "api.thingspeak.com", 443).request("GET", "/update?" + para, {'User-Agent' => "test-agent"})
  end

  reg = yabm.gpiogetdat
  yabm.gpiosetdat(reg | LED10)

  yabm.watchdogreset

  yabm.msleep(1000 * interval)
  count += 1
end

rescue => e
  yabm.print e.to_s
end
