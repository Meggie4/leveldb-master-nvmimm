#include <stdlib.h>
/////////////meggie
#include <stdio.h>
/////////////meggie
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <pthread.h>

//#define DEBUG

//Trouble shooting debug
//use only if trouble shooting
//some specific functions
void DEBUG_T(const char* format, ... ) {
#ifdef DEBUG
        va_list args;
        va_start( args, format );
		FILE *m_file = fopen("/home/meggie/文档/leveldb-master-nvmimm/mylog.txt", "a+b");
        vfprintf(m_file, format, args );
        fclose(m_file);
        //vfprintf(stderr, format, args );
        va_end( args );
#endif
}
