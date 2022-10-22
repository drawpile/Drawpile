#ifndef RNGDOUBLE_H
#define RNGDOUBLE_H

#include "mypaint-config.h"

#if MYPAINT_CONFIG_USE_GLIB
#include <glib.h>
#else // not MYPAINT_CONFIG_USE_GLIB
#include "mypaint-glib-compat.h"
#endif


G_BEGIN_DECLS

typedef struct RngDouble RngDouble;

RngDouble* rng_double_new(long seed);
void rng_double_free(RngDouble *self);

void rng_double_set_seed(RngDouble *self, long seed);
double rng_double_next(RngDouble* self);
void rng_double_get_array(RngDouble *self, double aa[], int n);

G_END_DECLS

#endif // RNGDOUBLE_H
