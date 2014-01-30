#include "transient_support.h"
#include "datatypes.h"
#include <math.h>
#include <assert.h>

static inline dfloat_t linear(const dfloat_t ystart, const dfloat_t yend,
                              const dfloat_t xstart, const dfloat_t xend,
                              const dfloat_t time) {
    dfloat_t a = (yend - ystart);
    dfloat_t b = ystart - (xend - xstart) * a * xstart;
    dfloat_t out = (xend - xstart) * a * time + b;
    return out;
}

dfloat_t analysis_transient_call_exp(struct transient_exp *data, const dfloat_t abs_time) {
    const dfloat_t time = abs_time;
    dfloat_t out;

    if (0 <= time && time <= data->td1)
        return data->i1;
    else if (data->td1 < time  && time <= data->td2)
        out = data->i1 + (data->i2 - data->i1) *
            (1-exp(-(time-data->td1)/data->tc1));
    else
        out = data->i1 + (data->i2 - data->i1) *
            (exp(-(time-data->td2)/data->tc2) - exp(-(time-data->td1)/data->tc1));
    return out;
}

dfloat_t analysis_transient_call_sin(struct transient_sin *data, const dfloat_t abs_time) {
    const dfloat_t time = abs_time;
    dfloat_t out;

    if (0 <= time && time <= data->td)
        out = data->i1 + data->ia * sin(2*M_PI*data->ph/360);
    else
        out = data->i1 + data->ia *
            sin(2*M_PI*data->fr*(time-data->td) + 2*M_PI*data->ph/360) *
            exp(-(time-data->td)*data->df);
    return out;
}

dfloat_t analysis_transient_call_pulse(struct transient_pulse *data, const dfloat_t abs_time) {
    const dfloat_t time = abs_time;
    dfloat_t out = data->i1;

    unsigned int k = floor(time / data->per);
    //printf("DEBUG:  k=%u    time=%lf\n",k,time);

    dfloat_t l0 =            k * data->per;
    dfloat_t r0 = data->td + k * data->per;
    //printf("DEBUG:  l0=%lf    r0=%lf\n",l0,r0);

    if (l0 <= time && time <= r0)
        return data->i1;

    dfloat_t l1 = data->td            + k * data->per;
    dfloat_t r1 = data->td + data->tr + k * data->per;
    //printf("DEBUG:  l1=%lf    r1=%lf\n",l1,r1);

    if (l1 <= time && time <= r1) {
        out = linear(data->i1,data->i2,l1,r1,time);
        return out;
    }

    dfloat_t l2 = r1;
    dfloat_t r2 = r1 + data->pw;
    //printf("DEBUG:  l2=%lf    r2=%lf\n",l2,r2);

    if (l2 <= time && time <= r2)
        return data->i2;

    dfloat_t l3 = r2;
    dfloat_t r3 = r2 + data->tf;
    //printf("DEBUG:  l3=%lf    r3=%lf\n",l3,r3);

    if (l3 <= time && time <= r3) {
        out = linear(data->i2,data->i1,l3,r3,time);
        return out;
    }

    dfloat_t l4 = r3;
    dfloat_t r4 = data->td + data->per + k * data->per;
    //printf("DEBUG:  l4=%lf    r4=%lf\n",l4,r4);

    if (l4 <= time && time <= r4)
        return data->i1;


    //should never reach here
    assert(0);
    return out;
}

dfloat_t analysis_transient_call_pwl(struct transient_pwl *data, const dfloat_t abs_time) {
    const dfloat_t time = abs_time;
    dfloat_t out;

    if (time < data->pair[0].time)
        return data->pair[0].value;

    unsigned long i;
    for (i=1; i<data->next; ++i) {
        if (time <= data->pair[i].time) {
            out = linear(data->pair[i-1].value,data->pair[i].value,
                         data->pair[i-1].time,data->pair[i].time,
                         time);
            return out;
        }
    }

    out = data->pair[data->next-1].value;
    return out;
}
