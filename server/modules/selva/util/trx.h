#pragma once
#ifndef _UTIL_TRX_H_
#define _UTIL_TRX_H_
#include <time.h>

typedef struct timespec Trx;

void Trx_Begin(Trx *trx);
void Trx_Stamp(const Trx *trx, struct timespec *ts);
int Trx_IsStamped(const Trx *trx, struct timespec *ts);

#endif /* _UTIL_TRX_H_ */
