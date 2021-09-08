#pragma once
#ifndef AUTO_FREE_H
#define AUTO_FREE_H

void _wrapFree(void *p);
#define __auto_free __attribute__((cleanup(_wrapFree)))

#endif /* AUTO_FREE_H */
