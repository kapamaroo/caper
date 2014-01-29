#include "transient_support.h"

dfloat_t analysis_transient_call_exp(struct transient_exp *data) {
    /*
    dfloat_t *time;
    dfloat_t *exp_out;

    if (time >= 0 && time <= data.td1) 
        exp_out = data.i1;
    else if (time > data.td1  && time <= data.td2) 
        exp_out = data.i1 + (data.i2 - data.i1)*(1-exp(-(time-data.td1)/data.tc1));
    else
        exp_out = data.i1 + (data.i2 - data.i1)*(exp(-(time-data.td2)/data.tc2)-exp(-(time-data.td1)/data.tc1));
    return *exp;
    */
    return 0;
}

dfloat_t analysis_transient_call_sin(struct transient_sin *data) {
    /*
    dfloat_t *time;
    dfloat_t *sin_out;

    if (time >= 0 && time <= data.td) 
        sin_out = data.i1+data.ia*sin(2*M_PI*data.ph/360);
    else
        sin_out = data.i1+data.ia*sin(2*M_PI*data.ph*(time-data.td) + 2*M_PI*data.ph/360)*exp(-(time-data.td)*data.df);
    return *sin_out;
    */
    return 0;
}

dfloat_t analysis_transient_call_pulse(struct transient_pulse *data) {
    /*
    dfloat_t *time;
    dfloat_t *pulse_out;
    int k;

    if (time >= 0 && time <= data.td)
        pulse_out = data.i1;
    else if (time > data.td + k*data.per && time <= data.td + data.tr + k*data.per)
        pulse_out = data.i1;
    else if (time > data.td + data.tr + k*data.per && time <= data.td + data.tr + data.pw + k*data.per)
        pulse_out = data.i2;
    else if (time > data.td + data.tr + data.pw + data.per && time <= data.td + data.tr + data.pw + data.tf + data.per)
        pulse_out = data.i2;
    else 
        pulse_out = data.i1

    return *pulse_out
    */
    return 0;
}

dfloat_t analysis_transient_call_pwl(struct transient_pwl *data) {
    return 0;
}
