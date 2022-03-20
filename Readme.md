This projct use these libraries.

newlib-3.0.0.20180831  
lwip-2.1.2  
bearssl-0.6  
mruby (git submodule)  

Build tools.  

gcc 4.9.2

I build on FreeBSD/amd64 11.2 used by linux emuration.  

Work on BCM4712, BCM53510, BCM5352, BCM5354.  

Build cfe image.  

% make image  

Defalte script is samples/hello.rb.  

Custom script build is this.  

% make image RBSCRIPT=myscript.rb  

Todo  

2038 problem  
real memory size  
UART support  
SPI flash support  
Switch support  
I2C support  
USB Support  
IPv6 support  
etc.  
