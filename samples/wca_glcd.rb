# clear by 12 char

CLEARLEN = 12

begin

yabm = YABM.new

yabm.setbaud(9600, 1)

loop do
  str = "Hello mruby"
  yabm.print2 str
  yabm.msleep 5_000
  yabm.print2 " " * (CLEARLEN - str.length)

  str = "on bcmbm"
  yabm.print2 str
  yabm.msleep 5_000
  yabm.print2 " " * (CLEARLEN - str.length)
end

rescue => e
  yabm.print e.to_s
end
