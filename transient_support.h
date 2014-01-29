#ifndef _TRANSIENT_SUPPORT_H_
#define _TRANSIENT_SUPPORT_H_

#include "precision.h"

struct transient_exp;
struct transient_sin;
struct transient_pulse;
struct transient_pwl;

dfloat_t analysis_transient_call_exp(struct transient_exp *data, const dfloat_t abs_time);
dfloat_t analysis_transient_call_sin(struct transient_sin *data, const dfloat_t abs_time);
dfloat_t analysis_transient_call_pulse(struct transient_pulse *data, const dfloat_t abs_time);
dfloat_t analysis_transient_call_pwl(struct transient_pwl *data, const dfloat_t abs_time);

#endif
