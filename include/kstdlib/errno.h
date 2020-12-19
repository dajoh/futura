#ifndef __ERRNO_H__
#define __ERRNO_H__

int* __k_errno_location();
#define k_errno (*__k_errno_location())

#define ERANGE 1
#define EDOM   2
#define EILSEQ 3

#endif
