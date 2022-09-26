/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _UTIL_LPF_H_
#define _UTIL_LPF_H_

/**
 * Calculate the alpha coefficient for the lpf.
 */
__constfn static inline float lpf_geta(float period, float sample_interval);
static inline float lpf_geta(float period, float sample_interval) {
    return __builtin_expf(-(sample_interval / period));
}

/**
 * Calculate the next output value of the lpf.
 * @param prev is the previous output of this function.
 * @param a is the coefficient calculated by lpf_geta().
 * @param sample is the current sample.
 */
__constfn static inline float lpf_calc_next(float a, float prev, float sample);
static inline float lpf_calc_next(float a, float prev, float sample) {
    return a * prev + (1.0f - a) * sample;
}

#endif /* _UTIL_LPF_H_ */
