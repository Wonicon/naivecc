#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef DEBUG
#define LOG(x) printf("\033[0;35;40m" #x "\033[0m" " ")
#else
#define LOG(x) ;
#endif

#endif
