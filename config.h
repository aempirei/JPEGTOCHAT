#ifdef _WIN32
#define strcasecmp stricmp
#define snprintf _snprintf
#define sleep(a) Sleep(a*100)
#include <windows.h>
#include "getopt.h"
#else
#include <unistd.h>
#endif
