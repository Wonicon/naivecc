#ifndef __DEBUG_H__
#define __DEBUG_H__

#define DEBUG
#ifdef DEBUG
#define LOG(x) printf(#x)
#else
#define LOG(x) ;
#endif

#endif
