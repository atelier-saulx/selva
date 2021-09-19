#pragma once
#ifndef _UTIL_LPF_H_
#define _UTIL_LPF_H_

static inline float lpf_geta(float period, float sample_interval) {
    const float km1 = period / sample_interval;
    const float a = km1 / (km1 + 1.0f);

    return a;
}

float lpf_calc_next(float a, float prev, float sample);

#endif /* _UTIL_LPF_H_ */
