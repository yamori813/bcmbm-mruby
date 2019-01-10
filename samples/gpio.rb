begin

yabm = YABM.new

count = 0
last = 0

while 1 do

  start = yabm.count() 
  while yabm.count() < start + 1000 do
  end
  val = yabm.gpiogetdat
  if val != last
    yabm.print val.to_s + "\n"
    last = val
  end
end

rescue => e
  yabm.print e.to_s
end
