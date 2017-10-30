
#ifndef BUF_H
#define BUF_H


/* Use 'W' as magic number */
#define BUFF_IOC_MAGIC  'W'
/* Please use a different 8-bit number in your code */

#define BUFF_IOCRESET    _IO(BUFF_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define BUFF_GETNUMDATA     _IO(BUFF_IOC_MAGIC,  1)
#define BUFF_GETNUMREADER   _IO(BUFF_IOC_MAGIC,  2)
#define BUFF_GETBUFSIZE     _IO(BUFF_IOC_MAGIC,   3)
#define BUFF_SETBUFSIZE     _IOW(BUFF_IOC_MAGIC,   4,int)




#endif