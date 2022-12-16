#pragma once
#ifndef CRC32C_H
#define CRC32C_H

/**
 * Compute CRC-32C.
 */
uint32_t crc32c(uint32_t crc, void const *buf, size_t len);

#endif /* CRC32C_H */
