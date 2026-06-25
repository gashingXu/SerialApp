#ifndef DATACHECK_H_
#define DATACHECK_H_

// 数据校验函数

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

//CRC8校验
uint8_t DataCheck_GetCRC8(const uint8_t *str,uint16_t num,uint8_t *pcrc);
uint8_t DataCheck_GetCRC8_Prog(const uint8_t *str, uint16_t num, uint8_t *pcrc);
uint16_t DataCheck_GetCRC16(const uint8_t *str, uint16_t num, uint8_t *pcrc);
uint16_t DataCheck_GetCRC16_Prog(const uint8_t *str, uint16_t num, uint8_t *pcrc);
uint8_t DataCheck_GetXOR(const uint8_t *str, uint16_t num, uint8_t *pxor);
uint8_t DataCheck_GetSUM(const uint8_t *str, uint16_t num, uint8_t *psum);

#ifdef __cplusplus
}
#endif

#endif /* DATACHECK_H_ */
