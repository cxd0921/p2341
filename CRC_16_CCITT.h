#ifndef _CRC_16_CCITT
#define _CRC_16_CCITT




unsigned short CRC16_CCITT (unsigned char *pdata, int len);
int crcCheck(unsigned char *pData, unsigned int dataLen);
void getMD5sum(const char* filename, char* md5_str);


#endif