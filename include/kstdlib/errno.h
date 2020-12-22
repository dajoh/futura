#ifndef __ERRNO_H__
#define __ERRNO_H__

int* __errno_location();
#define errno (*__errno_location())

#define ERANGE 1
#define EDOM   2
#define EILSEQ 3

#endif
