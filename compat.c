#include "compat.h"

#define PI  3.14159265358979323846
#define LN2 0.69314718055994530942

/* ---------- Matematik ---------- */

double fabs(double x) { return x < 0.0 ? -x : x; }

double floor(double x) {
    long long i;
    double d;
    if (x > 9.0e15 || x < -9.0e15) return x;
    i = (long long)x;
    d = (double)i;
    return (d > x) ? d - 1.0 : d;
}

double ceil(double x) {
    double f = floor(x);
    return (f < x) ? f + 1.0 : f;
}

double fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    return x - y * (double)(long long)(x / y);
}

double sqrt(double x) {
    double g, n;
    int i;
    if (x <= 0.0) return 0.0;
    g = (x > 1.0) ? x * 0.5 : 1.0;
    for (i = 0; i < 100; i++) {
        n = 0.5 * (g + x / g);
        if (fabs(n - g) <= 1e-16 * n) { g = n; break; }
        g = n;
    }
    return g;
}

static double reduce_angle(double x) {
    return x - 2.0 * PI * floor(x / (2.0 * PI) + 0.5);
}

double sin(double x) {
    double t, s, x2;
    int n;
    x = reduce_angle(x);
    t = x; s = x; x2 = x * x;
    for (n = 1; n < 20; n++) {
        t *= -x2 / ((2.0 * n) * (2.0 * n + 1.0));
        s += t;
    }
    return s;
}

double cos(double x) {
    double t, s, x2;
    int n;
    x = reduce_angle(x);
    t = 1.0; s = 1.0; x2 = x * x;
    for (n = 1; n < 20; n++) {
        t *= -x2 / ((2.0 * n - 1.0) * (2.0 * n));
        s += t;
    }
    return s;
}

double tan(double x) {
    double c = cos(x);
    if (c == 0.0) return 1e300;
    return sin(x) / c;
}

double atan(double x) {
    int neg = 0, inv = 0, n;
    double add = 0.0, x2, t, s, r;
    if (x < 0.0) { x = -x; neg = 1; }
    if (x > 1.0) { x = 1.0 / x; inv = 1; }
    if (x > 0.4142135623730951) {
        x = (x - 1.0) / (x + 1.0);
        add = PI / 4.0;
    }
    x2 = x * x; t = x; s = x;
    for (n = 1; n < 40; n++) {
        t *= -x2;
        s += t / (2.0 * n + 1.0);
    }
    r = add + s;
    if (inv) r = PI / 2.0 - r;
    return neg ? -r : r;
}

double atan2(double y, double x) {
    if (x > 0.0) return atan(y / x);
    if (x < 0.0) return (y >= 0.0) ? atan(y / x) + PI : atan(y / x) - PI;
    if (y > 0.0) return PI / 2.0;
    if (y < 0.0) return -PI / 2.0;
    return 0.0;
}

double asin(double x) {
    if (x >= 1.0) return PI / 2.0;
    if (x <= -1.0) return -PI / 2.0;
    return atan2(x, sqrt(1.0 - x * x));
}

double acos(double x) {
    return PI / 2.0 - asin(x);
}

double exp(double x) {
    int k, n;
    double r, t, s;
    if (x > 700.0) x = 700.0;
    if (x < -700.0) return 0.0;
    k = (int)floor(x / LN2 + 0.5);
    r = x - k * LN2;
    t = 1.0; s = 1.0;
    for (n = 1; n < 25; n++) {
        t *= r / n;
        s += t;
    }
    while (k > 0) { s *= 2.0; k--; }
    while (k < 0) { s *= 0.5; k++; }
    return s;
}

double log(double x) {
    int k = 0, n;
    double z, z2, t, s;
    if (x <= 0.0) return -1e300;
    while (x > 1.5) { x *= 0.5; k++; }
    while (x < 0.75) { x *= 2.0; k--; }
    z = (x - 1.0) / (x + 1.0);
    z2 = z * z;
    t = z; s = z;
    for (n = 1; n < 30; n++) {
        t *= z2;
        s += t / (2.0 * n + 1.0);
    }
    return 2.0 * s + k * LN2;
}

double log10(double x) {
    return log(x) / 2.302585092994046;
}

double pow(double x, double y) {
    int i, neg = 0;
    double r = 1.0, b = x;
    if (y == (double)(int)y && y > -1000.0 && y < 1000.0) {
        i = (int)y;
        if (i < 0) { neg = 1; i = -i; }
        while (i > 0) {
            if (i & 1) r *= b;
            b *= b;
            i >>= 1;
        }
        return neg ? 1.0 / r : r;
    }
    if (x <= 0.0) return 0.0;
    return exp(y * log(x));
}

/* ---------- strtod ---------- */

double strtod(const char *s, char **end) {
    double v = 0.0, f = 0.1;
    int neg = 0, e = 0, eneg = 0, any = 0;
    const char *p = s;

    while (*p == ' ' || *p == '\t' || *p == '\n') p++;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') { p++; }

    while (*p >= '0' && *p <= '9') { v = v * 10.0 + (*p - '0'); p++; any = 1; }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') { v += (*p - '0') * f; f *= 0.1; p++; any = 1; }
    }
    if (any && (*p == 'e' || *p == 'E')) {
        p++;
        if (*p == '-') { eneg = 1; p++; }
        else if (*p == '+') { p++; }
        while (*p >= '0' && *p <= '9') { e = e * 10 + (*p - '0'); p++; }
        while (e-- > 0) v = eneg ? v / 10.0 : v * 10.0;
    }
    if (end) *end = (char *)(any ? p : s);
    return neg ? -v : v;
}

/* ---------- snprintf ---------- */

typedef struct { char *buf; unsigned int cap; unsigned int len; } Out;

static void put(Out *o, char c) {
    if (o->len + 1 < o->cap) o->buf[o->len] = c;
    o->len++;
}

static int u2s(unsigned long long v, char *t, int base, int upper) {
    char r[24];
    int k = 0, i, d;
    do {
        d = (int)(v % (unsigned)base);
        r[k++] = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10);
        v /= (unsigned)base;
    } while (v);
    for (i = 0; i < k; i++) t[i] = r[k - 1 - i];
    return k;
}

int vsnprintf(char *buf, unsigned int size, const char *fmt, va_list ap) {
    Out o;
    o.buf = buf; o.cap = size; o.len = 0;

    while (*fmt) {
        char t[80];
        const char *s = t;
        char c, sign = 0;
        int left = 0, zero = 0, plus = 0, width = 0, prec = -1, n = 0, total, pad, i;

        if (*fmt != '%') { put(&o, *fmt++); continue; }
        fmt++;

        for (;;) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') { }
            else break;
            fmt++;
        }
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++; prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z') fmt++;

        c = *fmt;
        if (!c) break;
        fmt++;

        if (c == '%') { put(&o, '%'); continue; }

        if (c == 'c') {
            t[0] = (char)va_arg(ap, int);
            n = 1;
        } else if (c == 's') {
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (s[n] && (prec < 0 || n < prec)) n++;
        } else if (c == 'd' || c == 'i') {
            int v = va_arg(ap, int);
            unsigned int u = (v < 0) ? (unsigned int)(-(v + 1)) + 1u : (unsigned int)v;
            if (v < 0) sign = '-'; else if (plus) sign = '+';
            n = u2s(u, t, 10, 0);
        } else if (c == 'u' || c == 'x' || c == 'X') {
            unsigned int u = va_arg(ap, unsigned int);
            n = u2s(u, t, c == 'u' ? 10 : 16, c == 'X');
        } else if (c == 'f' || c == 'F' || c == 'g' || c == 'G' || c == 'e' || c == 'E') {
            double v = va_arg(ap, double);
            double r = 0.5, frac;
            unsigned long long ip;
            if (v < 0.0) { sign = '-'; v = -v; } else if (plus) sign = '+';
            if (prec < 0) prec = 6;
            if (prec > 15) prec = 15;
            for (i = 0; i < prec; i++) r /= 10.0;
            v += r;
            if (v > 1.0e18) v = 1.0e18;
            ip = (unsigned long long)v;
            frac = v - (double)ip;
            n = u2s(ip, t, 10, 0);
            if (prec > 0) {
                t[n++] = '.';
                for (i = 0; i < prec; i++) {
                    int d;
                    frac *= 10.0;
                    d = (int)frac;
                    if (d > 9) d = 9;
                    frac -= d;
                    t[n++] = (char)('0' + d);
                }
            }
        } else {
            put(&o, c);
            continue;
        }

        total = n + (sign ? 1 : 0);
        pad = (width > total) ? width - total : 0;

        if (!left && !(zero && c != 's' && c != 'c')) while (pad-- > 0) put(&o, ' ');
        if (sign) put(&o, sign);
        if (!left && zero && c != 's' && c != 'c') while (pad-- > 0) put(&o, '0');
        for (i = 0; i < n; i++) put(&o, s[i]);
        if (left) while (pad-- > 0) put(&o, ' ');
    }

    if (o.cap > 0) o.buf[(o.len < o.cap - 1) ? o.len : o.cap - 1] = 0;
    return (int)o.len;
}

int snprintf(char *buf, unsigned int size, const char *fmt, ...) {
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}
