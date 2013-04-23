#include<stddef.h>
void base64_encode(unsigned char const* src, unsigned int len,unsigned char *dst,unsigned int dst_len);
void base64_decode(unsigned char const *s,unsigned int len,unsigned char *dst,unsigned int dst_len);
