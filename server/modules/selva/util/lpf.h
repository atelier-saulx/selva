#pragma once
#ifndef _UTIL_LPF_H_
#define _UTIL_LPF_H_

float lpf_geta(float sample_interval, float period);
float lpf_calc_next(float a, float prev, float sample);

#endif /* _UTIL_LPF_H_ */
