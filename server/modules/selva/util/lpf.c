#include "lpf.h"

/*
 * tau = (k - 1) * h
 * h = sampling interval
 * k - 1 = t_tau / h
 * alpha = tau / (tau + h) = (k - 1) / k
 *
 * The current value:
 * q_k = alpha * q_{k-1} + (1 - alpha) * n_k
 *
 * q_{k-1} = the previous filter value
 * n_k = the sampled value
 */

float lpf_geta(float sample_interval, float period) {
    const float km1 = period / sample_interval;
    const float a = km1 / (km1 + 1.0f);

    return a;
}

float lpf_calc_next(float a, float prev, float sample) {
    return a * prev + (1.0f - a) * sample;
}
