#include <malloc.h>
#include <string.h>

#define cfe_flushcache(a)
#define KMALLOC(x,y) memalign(y, x)
#define KFREE free
#define printf
//#define xprintf
#define xsprintf
#define xstrncpy

#include "cpu_config.h"
#include "lib_types.h"
#include "lib_queue.h"
#include "lib_hssubr.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"
#include "cfe_timer.h"

#include "bsp_config.h" 
