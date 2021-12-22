#pragma once
#ifndef AUTO_FREE_H
#define AUTO_FREE_H

/**
 * Wrap RedisModule_Free().
 */
void _wrapFree(void *p);

/**
 * Pointer variable attribute to free the object pointed by the pointer.
 * The value must have been allocated with one of theRedisModule allocation
 * functions.
 */
#define __auto_free __attribute__((cleanup(_wrapFree)))

#endif /* AUTO_FREE_H */
