/**
  * _printf(): enables debug printf on secrman.c
  * _printf2(): enables debug printf on keyman.c
  * _printf3(): enables debug printf on MechaAuth.c
  * _printf(): enables debug printf on CardAuth.c
  */


#ifdef DEBUG
#define _printf(format, args...) printf("SECRMAN:%s: " format, __FUNCTION__, ##args)
#define _printf2(format, args...) printf("SECRMAN:%s: " format, __FUNCTION__, ##args)
#define _printf3(format, args...) printf("SECRMAN:%s: " format, __FUNCTION__, ##args)
#define _printf4(format, args...) printf("SECRMAN:%s: " format, __FUNCTION__, ##args)
#else
#define _printf(args...)
#define _printf2(args...)
#define _printf3(args...)
#define _printf4(args...)
#endif

#define PRINT_8BYTE_BUFF_FMT "%X %X %X %X %X %X %X %X"
#define PRINT_8BYTE_BUFF(x) x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]
