#pragma once
#ifndef _SELVA_SHARED_H_
#define _SELVA_SHARED_H_

struct RedisModuleString;

/**
 * Share a RedisModuleString.
 * Anything passed to this function is expected to be needed forever read-only
 * and there is no way to free anything nor determine if something could be
 * freed. This storage model is ideal for things like type strings that are
 * unlikely to ever change or change extremely rarely but are still used
 * everywhere.
 *
 * @returns If rms is not shared yet it will be added to the internal data structure;
 *          If rms is shared a pointer to the previously shared RedisModuleString is returned;
 *          A NULL pointer is returned if adding rms to the internal data structure fails.
 */
struct RedisModuleString *Share_RMS(const char *key_str, size_t key_len, struct RedisModuleString *rms);

#endif /* _SELVA_SHARED_H_ */
