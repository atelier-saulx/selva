/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef SELVA_TIMESTAMP_H
#define SELVA_TIMESTAMP_H

/**
 * Get current UNIX time in ms.
 */
long long ts_now(void);

void ts_monotime(struct timespec *spec);
void ts_monorealtime(struct timespec *spec);
long long ts_monorealtime_now(void);

#endif /* SELVA_TIMESTAMP_H */
