#ifndef COMPAT_H
#define COMPAT_H

#include <stdarg.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int snprintf(char *buf, unsigned int size, const char *fmt, ...);
int vsnprintf(char *buf, unsigned int size, const char *fmt, va_list ap);
double strtod(const char *s, char **end);

double fabs(double x);
double floor(double x);
double ceil(double x);
double fmod(double x, double y);
double sqrt(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double atan(double x);
double atan2(double y, double x);
double asin(double x);
double acos(double x);
double exp(double x);
double log(double x);
double log10(double x);
double pow(double x, double y);

#endif
