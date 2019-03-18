#include "rtklib.h"
/* show message --------------------------------------------------------------*/

extern int showmsg(char *format, ...)
{
    va_list arg;
    va_start(arg,format); vfprintf(stderr,format,arg); va_end(arg);
    fprintf(stderr,"\r");
    return 0;
}

extern void settspan(gtime_t ts, gtime_t te) {}
extern void settime(gtime_t time) {}

