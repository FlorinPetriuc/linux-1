#ifndef _NBA_DEBUG_H_
#define _NBA_DEBUG_H_

#define DEBUG_NBA

#ifdef DEBUG_NBA

#define NBA_PRINT(x, ...) pr_err("%s:%s - %d " x "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#else

#define NBA_PRINT(x, ...)

#endif

#endif
