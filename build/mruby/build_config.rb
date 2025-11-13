
MRuby::CrossBuild.new('broadcom') do |conf|
  toolchain :gcc
  conf.cc.command = 'mips-gcc'
#  conf.archiver.command = 'mips-ar'
  conf.archiver.command = 'mips-unknown-freebsd13.0-ar'

  cc.defines << %w(MRB_USE_ETEXT_RO_DATA_P)
  cc.defines << %w(MRB_NO_STDIO)
  cc.defines << %w(MRB_NO_FLOAT)
  cc.defines << %w(YABM_BROADCOM)
  conf.cc.flags << "-EL -G 0"
  conf.cc.flags << "-fno-pic -mno-abicalls"
# for debug
#  conf.cc.flags << "-O0 -g3 -fno-pic -mno-abicalls"
  conf.cc.flags << "-pipe -mlong-calls"
  conf.cc.include_paths = ["#{root}/include", "../newlib-3.0.0.20180831/newlib/libc/include"]

  conf.gem :github => 'yamori813/mruby-yabm'
  conf.gem :github => 'yamori813/mruby-simplehttp'
# use in mruby-simplehttp'
  conf.gem :core => "mruby-string-ext"

end
