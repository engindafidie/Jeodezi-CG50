bash

mkdir -p /home/claude/jeo && cd /home/claude/jeo && cat > main.c << 'EOF'
/*
 * JEODEZI - Casio fx-CG50 add-in (PrizmSDK / libfxcg)
 * Yon tuslari: program sec | EXE veya F6: ac | F1: elipsoid | F2: aci birimi
 * F4: formul | F5: yardim | MENU: cikis
 * Tum acilar dahilde DERECE olarak tutulur. X kuzey, Y dogu.
 */
#include <fxcg/display.h>
#include <fxcg/keyboard.h>
#include <fxcg/misc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RHO (180.0 / M_PI)
#define MAXL 40
#define NMEM 24
#define MEMW 8

typedef struct {
    const char *name;
    double a, b, c, e2, ep2;
    double A0, A2, A4, A6, A8;
    double B2, B4, B6;
} Ellipsoid;

static const Ellipsoid ELLIPSOIDS[2] = {
    {
        "HAYFORD",
        6378388.0, 6356911.9461279465, 6399936.608108108,
        0.0067226700223333, 0.0067681701972243,
        111136.536655, 16107.03468, 16.97621, 0.02227, 0.00003,
        0.144930071, 0.000213851, 0.000000432
    },
    {
        "GRS80",
        6378137.0, 6356752.314140356, 6399593.625864022,
        0.00669438002290, 0.00673949677548,
        111132.952546, 16038.508694, 16.832600, 0.021981, 0.000031,
        0.144318133, 0.000212049, 0.000000427
    }
};

static const char *UNIT_TAG[3] = {"DEG", "DMS", "GON"};
static const char *UNIT_HINT[3] = {"Birim: ondalik derece", "Birim: GG.DDSS (DMS)", "Birim: grad (gon)"};

/* Genel durum: hepsi main() icinde ilklenir */
static int g_elip, g_unit, g_sel, g_top, g_n;
static const char *g_title;
static double g_mem[NMEM * MEMW];
static char g_out[MAXL][24];

static const Ellipsoid *E(void) { return &ELLIPSOIDS[g_elip]; }
static double sq(double x) { return x * x; }
static double D2R(double d) { return d / RHO; }
static double R2D(double r) { return r * RHO; }
static double sd(double d) { return sin(d / RHO); }
static double cd(double d) { return cos(d / RHO); }
static double td(double d) { return tan(d / RHO); }
static double *memslot(int k) { return &g_mem[k * MEMW]; }

static double norm360(double a) {
    while (a < 0.0) a += 360.0;
    while (a >= 360.0) a -= 360.0;
    return a;
}

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }

/* ------------------------------------------------------------------ */
/* Ekran yardimcilari. PrintXY ilk 2 karakteri yuttugu icin 2 bosluk   */
/* eklenir; w>0 ise satir w sutuna bosluk ile doldurulur.              */
/* ------------------------------------------------------------------ */
static void put(int x, int y, const char *s, int mode, int color, int w) {
    char b[48];
    int i = 0;
    b[0] = ' '; b[1] = ' ';
    while (s[i] && i < 40) { b[i + 2] = s[i]; i++; }
    while (i < w && i < 40) { b[i + 2] = ' '; i++; }
    b[i + 2] = 0;
    PrintXY(x, y, b, mode, color);
}

static void title_bar(void) {
    put(1, 1, g_title, TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 21);
}

/* GG.DDSS -> ondalik derece */
static double dms_to_deg(double val) {
    double sign = (val < 0.0) ? -1.0 : 1.0, rem, s;
    int d, m;
    val = fabs(val);
    d = (int)val;
    rem = (val - d) * 100.0;
    m = (int)(rem + 1e-9);
    s = (rem - m) * 100.0;
    return sign * (d + m / 60.0 + s / 3600.0);
}

/* ondalik derece -> secili birimde metin */
static void fmt_ang(char *b, int cap, double deg) {
    if (g_unit == 0) {
        snprintf(b, cap, "%.7f", deg);
    } else if (g_unit == 2) {
        snprintf(b, cap, "%.6f", deg / 0.9);
    } else {
        const char *sg = (deg < 0.0) ? "-" : "";
        double v = fabs(deg), mf, s;
        int d, m;
        d = (int)v;
        mf = (v - d) * 60.0;
        m = (int)mf;
        s = (mf - m) * 60.0;
        if (s >= 59.995) { s = 0.0; m++; }
        if (m >= 60) { m -= 60; d++; }
        snprintf(b, cap, "%s%dd%02dm%05.2fs", sg, d, m, s);
    }
}

/* ------------------------------------------------------------------ */
/* Sayi girisi. EXE bos birakilirsa onceki deger korunur.              */
/* ------------------------------------------------------------------ */
static int ask(const char *label, double *v, int ang) {
    char buf[24], d[28], l[28];
    int pos = 0, key;
    buf[0] = 0;
    for (;;) {
        Bdisp_AllClr_VRAM();
        title_bar();
        put(1, 3, label, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK, 0);
        if (ang) put(1, 4, UNIT_HINT[g_unit], TEXT_MODE_NORMAL, TEXT_COLOR_GREEN, 0);
        if (ang) fmt_ang(d, sizeof d, *v); else snprintf(d, sizeof d, "%.6f", *v);
        snprintf(l, sizeof l, "Onceki:%s", d);
        put(1, 5, l, TEXT_MODE_NORMAL, TEXT_COLOR_BLUE, 0);
        snprintf(l, sizeof l, ">%s", buf);
        put(1, 6, l, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK, 0);
        put(2 + pos, 6, " ", TEXT_MODE_INVERT, TEXT_COLOR_BLACK, 0);
        put(1, 8, "EXE:Tamam  EXIT:Iptal", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 21);
        Bdisp_PutDisp_DD();
        GetKey(&key);
        if (key == KEY_CTRL_EXE) {
            if (pos > 0) {
                double x = strtod(buf, 0);
                if (ang) {
                    if (g_unit == 1) x = dms_to_deg(x);
                    else if (g_unit == 2) x = x * 0.9;
                }
                *v = x;
            }
            return 1;
        } else if (key == KEY_CTRL_EXIT) {
            return 0;
        } else if (key == KEY_CTRL_DEL) {
            if (pos > 0) buf[--pos] = 0;
        } else if (key == KEY_CTRL_AC) {
            pos = 0; buf[0] = 0;
        } else if (key >= KEY_CHAR_0 && key <= KEY_CHAR_9 && pos < 18) {
            buf[pos++] = (char)('0' + (key - KEY_CHAR_0)); buf[pos] = 0;
        } else if (key == KEY_CHAR_DP && pos < 18) {
            buf[pos++] = '.'; buf[pos] = 0;
        } else if ((key == KEY_CHAR_MINUS || key == KEY_CHAR_PMINUS) && pos == 0) {
            buf[pos++] = '-'; buf[pos] = 0;
        }
    }
}

#define NEED(lbl, i, ang) do { if (!ask(lbl, &v[i], ang)) return; } while (0)

/* Kucuk secim menusu (en cok 5 madde); iptalde -1 */
static int pick(const char *title, const char **it, int n) {
    int sel = 0, key, i;
    for (;;) {
        Bdisp_AllClr_VRAM();
        put(1, 1, title, TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 21);
        for (i = 0; i < n; i++)
            put(1, 3 + i, it[i], (i == sel) ? TEXT_MODE_INVERT : TEXT_MODE_NORMAL, TEXT_COLOR_BLUE, 21);
        put(1, 8, "Yon:sec EXE:tamam", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 21);
        Bdisp_PutDisp_DD();
        GetKey(&key);
        if (key == KEY_CTRL_DOWN) sel = (sel + 1) % n;
        else if (key == KEY_CTRL_UP) sel = (sel + n - 1) % n;
        else if (key == KEY_CTRL_EXE) return sel;
        else if (key == KEY_CTRL_EXIT) return -1;
        else if (key >= KEY_CHAR_1 && key <= KEY_CHAR_9 && (key - KEY_CHAR_1) < n) return key - KEY_CHAR_1;
    }
}

/* ------------------------------------------------------------------ */
/* Sonuc satirlari                                                     */
/* ------------------------------------------------------------------ */
static void out_clear(void) { g_n = 0; }

static void out_num(const char *lab, double val, int dec) {
    if (g_n < MAXL) { snprintf(g_out[g_n], 24, "%s=%.*f", lab, dec, val); g_n++; }
}

static void out_ang(const char *lab, double deg) {
    char t[24];
    fmt_ang(t, sizeof t, deg);
    if (g_n < MAXL) { snprintf(g_out[g_n], 24, "%s=%s", lab, t); g_n++; }
}

static void out_azi(const char *lab, double deg) { out_ang(lab, norm360(deg)); }

static void out_txt(const char *s) {
    if (g_n < MAXL) { snprintf(g_out[g_n], 24, "%s", s); g_n++; }
}

static void show_out(void) {
    int top = 0, key, i;
    for (;;) {
        Bdisp_AllClr_VRAM();
        title_bar();
        for (i = 0; i < 6 && top + i < g_n; i++)
            put(1, 2 + i, g_out[top + i], TEXT_MODE_NORMAL, TEXT_COLOR_BLACK, 0);
        put(1, 8, "Yon:kaydir EXIT:geri", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 21);
        Bdisp_PutDisp_DD();
        GetKey(&key);
        if (key == KEY_CTRL_DOWN) { if (top + 6 < g_n) top++; }
        else if (key == KEY_CTRL_UP) { if (top > 0) top--; }
        else if (key == KEY_CTRL_EXIT || key == KEY_CTRL_EXE) return;
    }
}

static void load_text(const char *t) {
    int c = 0;
    g_n = 0;
    while (*t && g_n < MAXL) {
        if (*t == '\n') { g_out[g_n][c] = 0; g_n++; c = 0; }
        else if (c < 22) g_out[g_n][c++] = *t;
        t++;
    }
    if (c > 0 && g_n < MAXL) { g_out[g_n][c] = 0; g_n++; }
}

/* ------------------------------------------------------------------ */
/* Ortak jeodezik fonksiyonlar                                         */
/* ------------------------------------------------------------------ */
static double meridyen_yayi(double fi_deg) {
    const Ellipsoid *el = E();
    double r = fi_deg / RHO;
    return el->A0 * fi_deg
         - el->A2 * sin(2.0 * r)
         + el->A4 * sin(4.0 * r)
         - el->A6 * sin(6.0 * r)
         - el->A8 * sin(8.0 * r);
}

/* meridyen yayi G (m) -> ayak noktasi enlemi (derece) */
static double yaya_gore_enlem(double G) {
    const Ellipsoid *el = E();
    double sigma = G / el->A0;
    double sr = sigma / RHO;
    return sigma
         + el->B2 * sin(2.0 * sr)
         + el->B4 * sin(4.0 * sr)
         + el->B6 * sin(6.0 * sr);
}

static void egrilik_MN(double fi_deg, double *M, double *N, double *Rg, double *V, double *eta2) {
    const Ellipsoid *el = E();
    double c_r = cos(fi_deg / RHO);
    *eta2 = el->ep2 * c_r * c_r;
    *V = sqrt(1.0 + *eta2);
    *M = el->c / ((*V) * (*V) * (*V));
    *N = el->c / (*V);
    *Rg = el->c / ((*V) * (*V));
}

/* ------------------------------------------------------------------ */
/* 01 Egrilik                                                          */
/* ------------------------------------------------------------------ */
static void m_egrilik(void) {
    double *v = memslot(0), M, N, Rg, V, e2;
    NEED("Enlem Fi:", 0, 1);
    egrilik_MN(v[0], &M, &N, &Rg, &V, &e2);
    out_clear();
    out_num("M", M, 4);
    out_num("N", N, 4);
    out_num("R", Rg, 4);
    out_num("V", V, 8);
    out_num("ETA2", e2, 8);
    out_num("T", td(v[0]), 8);
    show_out();
}

/* 02 Meridyen yayi */
static void m_meridyen(void) {
    static const char *it[2] = {"1:Yay uzunlugu G12", "2:G -> Fi (ters)"};
    double *v = memslot(1), g1, g2;
    int k = pick(g_title, it, 2);
    if (k < 0) return;
    out_clear();
    if (k == 0) {
        NEED("Fi 1:", 0, 1);
        NEED("Fi 2:", 1, 1);
        g1 = meridyen_yayi(v[0]);
        g2 = meridyen_yayi(v[1]);
        out_num("G1", g1, 4);
        out_num("G2", g2, 4);
        out_num("G12", g2 - g1, 4);
    } else {
        NEED("Yay uzunlugu G (m):", 2, 0);
        out_ang("SIGMA", v[2] / E()->A0);
        out_ang("FI", yaya_gore_enlem(v[2]));
    }
    show_out();
}

/* 03 Cografi -> Kartezyen */
static void m_flh_xyz(void) {
    const Ellipsoid *el = E();
    double *v = memslot(2), N, Vv, cf, sf, X, Y, Z;
    NEED("Fi:", 0, 1);
    NEED("La:", 1, 1);
    NEED("H (m):", 2, 0);
    cf = cd(v[0]); sf = sd(v[0]);
    Vv = sqrt(1.0 + el->ep2 * cf * cf);
    N = el->c / Vv;
    X = (N + v[2]) * cf * cd(v[1]);
    Y = (N + v[2]) * cf * sd(v[1]);
    Z = (N * (1.0 - el->e2) + v[2]) * sf;
    out_clear();
    out_num("N", N, 4);
    out_num("X", X, 4);
    out_num("Y", Y, 4);
    out_num("Z", Z, 4);
    show_out();
}

/* 04 Kartezyen -> Cografi */
static void m_xyz_flh(void) {
    const Ellipsoid *el = E();
    double *v = memslot(3), p, phi, lam, N, H, s;
    int i;
    NEED("X (m):", 0, 0);
    NEED("Y (m):", 1, 0);
    NEED("Z (m):", 2, 0);
    p = sqrt(v[0] * v[0] + v[1] * v[1]);
    lam = atan2(v[1], v[0]);
    phi = atan2(v[2], p * (1.0 - el->e2));
    H = 0.0;
    for (i = 0; i < 12; i++) {
        s = sin(phi);
        N = el->a / sqrt(1.0 - el->e2 * s * s);
        if (fabs(phi) < 1.3) H = p / cos(phi) - N;
        else H = v[2] / s - N * (1.0 - el->e2);
        phi = atan2(v[2] + el->e2 * N * s, p);
    }
    out_clear();
    out_ang("FI", R2D(phi));
    out_ang("LA", R2D(lam));
    out_num("H", H, 4);
    show_out();
}

/* 05 Cografi -> Gauss */
static void m_cog_gauss(void) {
    const Ellipsoid *el = E();
    double *v = memslot(4), K, c, T, e2, N, G, Y, dX, m;
    NEED("Fi:", 0, 1);
    NEED("La:", 1, 1);
    NEED("La0 (orta meridyen):", 2, 1);
    K = D2R(v[1] - v[2]);
    c = cd(v[0]); T = td(v[0]);
    e2 = el->ep2 * c * c;
    N = el->c / sqrt(1.0 + e2);
    G = meridyen_yayi(v[0]);
    Y = N * c * K
      + (1.0 - T * T + e2) * N * c * c * c * K * K * K / 6.0
      + (5.0 - 18.0 * T * T + T * T * T * T) * N * c * c * c * c * c * K * K * K * K * K / 120.0;
    dX = N * T * c * c * K * K / 2.0
       + (5.0 - T * T + 9.0 * e2 + 4.0 * e2 * e2) * N * T * sq(c * c) * sq(K * K) / 24.0;
    m = 1.0 + c * c * (1.0 + e2) * K * K / 2.0
      + sq(c * c) * (5.0 - 4.0 * T * T + 14.0 * e2 - 28.0 * e2 * T * T) * sq(K * K) / 24.0;
    out_clear();
    out_num("G", G, 4);
    out_num("DX", dX, 4);
    out_num("X", G + dX, 4);
    out_num("Y", Y, 4);
    out_num("m", m, 9);
    show_out();
}

/* 06 Gauss -> Cografi */
static void m_gauss_cog(void) {
    const Ellipsoid *el = E();
    double *v = memslot(5), ff, c, T, e2, N, Y, B1, B2, B3, B4, B5, dla, dfi, C, m;
    NEED("X (m):", 0, 0);
    NEED("Y (m):", 1, 0);
    NEED("La0 (orta meridyen):", 2, 1);
    Y = v[1];
    ff = yaya_gore_enlem(v[0]);
    c = cd(ff); T = td(ff);
    e2 = el->ep2 * c * c;
    N = el->c / sqrt(1.0 + e2);
    B1 = RHO / (N * c);
    B2 = RHO * T * (-1.0 - e2) / (2.0 * N * N);
    B3 = -RHO * (1.0 + 2.0 * T * T + e2) / (6.0 * N * N * N * c);
    B4 = RHO * T * (5.0 + 3.0 * T * T + 6.0 * e2 - 6.0 * e2 * T * T) / (24.0 * sq(N * N));
    B5 = RHO * (5.0 + 28.0 * T * T + 24.0 * sq(T * T)) / (120.0 * sq(N * N) * N * c);
    dla = B1 * Y + B3 * Y * Y * Y + B5 * sq(Y * Y) * Y;
    dfi = B2 * Y * Y + B4 * sq(Y * Y);
    C = T * RHO / N * Y - T * RHO * (1.0 + T * T - e2) / (3.0 * N * N * N) * Y * Y * Y;
    m = 1.0 + (1.0 + e2) * Y * Y / (2.0 * N * N) + sq(Y * Y) / (24.0 * sq(N * N));
    out_clear();
    out_ang("FI.B", ff);
    out_ang("DFI", dfi);
    out_ang("DLA", dla);
    out_ang("FI", ff + dfi);
    out_ang("LA", v[2] + dla);
    out_ang("C", C);
    out_num("m", m, 9);
    show_out();
}

/* 07 Cografi -> Soldner */
static void m_cog_sold(void) {
    const Ellipsoid *el = E();
    double *v = memslot(6), K, c, s, T, I, N, Y, dx, XB, gam;
    NEED("Fi0 (baslangic enlem):", 0, 1);
    NEED("La0 (orta meridyen):", 1, 1);
    NEED("Fi:", 2, 1);
    NEED("La:", 3, 1);
    K = D2R(v[3] - v[1]);
    c = cd(v[2]); s = sd(v[2]); T = td(v[2]);
    I = el->ep2 * c * c;
    N = el->c / sqrt(1.0 + I);
    Y = N * c * K - N * c * c * c * T * T * K * K * K / 6.0
      - N * s * s * c * c * c * (8.0 - T * T) * sq(K * K) * K / 120.0;
    dx = N * c * c * T * K * K / 2.0
       + N * c * c * c * s * (5.0 - T * T + 5.0 * I) * sq(K * K) / 24.0;
    XB = meridyen_yayi(v[2]) - meridyen_yayi(v[0]);
    gam = R2D(K * s + (1.0 + I) * s * c * c * K * K * K / 3.0);
    out_clear();
    out_num("Y", Y, 4);
    out_num("DX", dx, 4);
    out_num("XB", XB, 4);
    out_num("X", XB + dx, 4);
    out_ang("GAMA", gam);
    show_out();
}

/* 08 Soldner -> Cografi */
static void m_sold_cog(void) {
    const Ellipsoid *el = E();
    double *v = memslot(7), G, fb, c, T, Q, V2, N, y, t1, t2, dl, gm;
    NEED("Fi0 (baslangic enlem):", 0, 1);
    NEED("La0 (orta meridyen):", 1, 1);
    NEED("X (m):", 2, 0);
    NEED("Y (m):", 3, 0);
    y = v[3];
    G = meridyen_yayi(v[0]) + v[2];
    fb = yaya_gore_enlem(G);
    c = cd(fb); T = td(fb);
    Q = el->ep2 * c * c;
    V2 = 1.0 + Q;
    N = el->c / sqrt(V2);
    t1 = V2 * T * y * y / (2.0 * N * N);
    t2 = V2 * T * (1.0 + 3.0 * T * T + Q - 9.0 * Q * T * T) * sq(y * y) / (24.0 * sq(N * N));
    dl = y / (N * c) - T * T * y * y * y / (3.0 * N * N * N * c)
       + T * T * (1.0 + 3.0 * T * T) * sq(y * y) * y / (15.0 * sq(N * N) * N * c);
    gm = T * y / N - T * (1.0 + 2.0 * T * T + Q) * y * y * y / (6.0 * N * N * N);
    out_clear();
    out_ang("FI.B", fb);
    out_ang("FI", fb - R2D(t1 - t2));
    out_ang("LA", v[1] + R2D(dl));
    out_ang("GAMA", R2D(gm));
    show_out();
}

/* 09 Soldner 1. temel odev */
static void m_sold_to1(void) {
    const Ellipsoid *el = E();
    double *v = memslot(8), F, X, Y, A, S, Z, U, V, O, T, R, Y2, X2, T21, ca, sa;
    NEED("Fi (ortalama enlem):", 0, 1);
    NEED("X1 (m):", 1, 0);
    NEED("Y1 (m):", 2, 0);
    NEED("T12 (yon acisi):", 3, 1);
    NEED("S12 (m):", 4, 0);
    F = v[0]; X = v[1]; Y = v[2]; A = v[3]; S = v[4];
    ca = cd(A); sa = sd(A);
    Z = Y; U = S * ca; V = S * sa;
    O = el->ep2 * cd(F) * cd(F);
    T = td(F);
    R = el->c / (1.0 + O);
    Y2 = Y + V - U * U * (Y + V / 3.0) / (2.0 * R * R)
       + (2.0 * O * T * T * Y * U * U * U) / (3.0 * R * R * R)
       - (Y * Y * U * U * (Y + 3.0 * V)) / (6.0 * sq(R * R));
    X2 = X + U + U * (Y2 * Y2 - V * V / 3.0) / (2.0 * R * R)
       - O * T * Y2 * Y2 * U * U / (R * R * R)
       + Y2 * Y2 * U * (5.0 * Y2 * Y2 + 4.0 * U * U - 2.0 * V * V) / (24.0 * sq(R * R));
    T21 = 180.0 - RHO * U * (Z + V / 2.0) / (R * R)
        + 2.0 * O * T * Z * U * U * RHO / (R * R * R)
        - Z * Z * U * RHO * (Z + 3.0 * U) / (3.0 * sq(R * R)) + A;
    out_clear();
    out_num("Y2", Y2, 4);
    out_num("X2", X2, 4);
    out_azi("T21", T21);
    show_out();
}

/* 10 Soldner 2. temel odev */
static void m_sold_to2(void) {
    const Ellipsoid *el = E();
    double *v = memslot(9), G0, G, Fm, Q, T, eta2, R, dX, dY, A, B, S, DT, r;
    NEED("X1 (m):", 0, 0);
    NEED("Y1 (m):", 1, 0);
    NEED("X2 (m):", 2, 0);
    NEED("Y2 (m):", 3, 0);
    NEED("Fi0 (baslangic enlem):", 4, 1);
    G0 = meridyen_yayi(v[4]);
    Q = (v[1] + v[3]) / 2.0;
    G = G0 + (v[0] + v[2]) / 2.0;
    Fm = yaya_gore_enlem(G);
    T = td(Fm);
    eta2 = el->ep2 * cd(Fm) * cd(Fm);
    R = el->c / (1.0 + eta2);
    dX = v[2] - v[0];
    dY = v[3] - v[1];
    A = dY - dX * dX * dY / (12.0 * R * R)
      + eta2 * T * Q * dX * dX * dX / (3.0 * R * R * R)
      - Q * Q * dX * dX * dY / (24.0 * sq(R * R));
    B = dX - dX * (Q * Q - dY * dY / 12.0) / (2.0 * R * R)
      + Q * Q * dX * (Q * Q - dX * dX - dY * dY / 2.0) / (24.0 * sq(R * R));
    S = sqrt(A * A + B * B);
    DT = -dX * RHO * (Q + Q * Q * Q / (6.0 * R * R)) / (R * R);
    r = norm360(R2D(atan2(A, B)));
    out_clear();
    out_ang("FI.M", Fm);
    out_num("A", A, 4);
    out_num("B", B, 4);
    out_num("S", S, 4);
    out_ang("DT", DT);
    out_azi("T12", r - DT / 2.0);
    out_azi("T21", r + DT / 2.0 - 180.0);
    show_out();
}

/* 11 Gauss 1. temel odev */
static void m_gauss_to1(void) {
    const Ellipsoid *el = E();
    double *v = memslot(10), X, Y, T, S, U0, V0, Q, P, F, R, Sik, d1, Tik, XK, YK, Tki;
    double Q2, P2, F2, R2, d2;
    NEED("X1 (m):", 0, 0);
    NEED("Y1 (m):", 1, 0);
    NEED("T12 (elipsoid yonu):", 2, 1);
    NEED("S12 (elipsoid, m):", 3, 0);
    X = v[0]; Y = v[1]; T = v[2]; S = v[3];
    U0 = X + S * cd(T);
    V0 = Y + S * sd(T);
    Q = (Y + V0) / 2.0;
    P = (X + U0) / 2.0;
    F = yaya_gore_enlem(P);
    R = el->c / (1.0 + el->ep2 * cd(F) * cd(F));
    Sik = S + S * (Y * Y + Y * V0 + V0 * V0) / (6.0 * R * R);
    d1 = RHO * Q * (U0 - X) * (1.0 - Q * Q / (3.0 * R * R)) * 0.5 / (R * R)
       - RHO * (U0 - X) * (V0 - Y) / (12.0 * R * R);
    Tik = T - d1;
    XK = X + Sik * cd(Tik);
    YK = Y + Sik * sd(Tik);
    Tki = norm360(Tik + 180.0);
    Q2 = (Y + YK) / 2.0;
    P2 = (X + XK) / 2.0;
    F2 = yaya_gore_enlem(P2);
    R2 = el->c / (1.0 + el->ep2 * cd(F2) * cd(F2));
    d2 = RHO * Q2 * (X - XK) * (1.0 - Q2 * Q2 / (3.0 * R2 * R2)) * 0.5 / (R2 * R2)
       - RHO * (X - XK) * (Y - YK) / (12.0 * R2 * R2);
    out_clear();
    out_num("S.DUZ", Sik, 4);
    out_azi("T12.D", Tik);
    out_num("X2", XK, 4);
    out_num("Y2", YK, 4);
    out_azi("T21.D", Tki);
    out_azi("T21.E", Tki + d2);
    show_out();
}

/* 12 Gauss 2. temel odev */
static void m_gauss_to2(void) {
    const Ellipsoid *el = E();
    double *v = memslot(11), X, Y, U, V, Q, P, F, R, S, Tik, Tki, dd, d12, d21;
    NEED("X1 (m):", 0, 0);
    NEED("Y1 (m):", 1, 0);
    NEED("X2 (m):", 2, 0);
    NEED("Y2 (m):", 3, 0);
    X = v[0]; Y = v[1]; U = v[2]; V = v[3];
    Q = (Y + V) / 2.0;
    P = (X + U) / 2.0;
    F = yaya_gore_enlem(P);
    R = el->c / (1.0 + el->ep2 * cd(F) * cd(F));
    S = sqrt((U - X) * (U - X) + (V - Y) * (V - Y));
    Tik = norm360(R2D(atan2(V - Y, U - X)));
    Tki = norm360(Tik + 180.0);
    dd = S * (Y * Y + Y * V + V * V) / (6.0 * R * R);
    d12 = RHO * Q * (U - X) * (1.0 - Q * Q / (3.0 * R * R)) * 0.5 / (R * R)
        - RHO * (U - X) * (V - Y) / (12.0 * R * R);
    d21 = RHO * Q * (X - U) * (1.0 - Q * Q / (3.0 * R * R)) * 0.5 / (R * R)
        - RHO * (X - U) * (Y - V) / (12.0 * R * R);
    out_clear();
    out_ang("FI.M", F);
    out_num("R", R, 3);
    out_num("S.DUZ", S, 4);
    out_azi("T12.D", Tik);
    out_azi("T21.D", Tki);
    out_num("S.ELP", S - dd, 4);
    out_azi("T12.E", Tik + d12);
    out_azi("T21.E", Tki + d21);
    show_out();
}

/* 13 Elipsoid 1. temel odev (Taylor serisi) */
static void m_elip_to1(void) {
    const Ellipsoid *el = E();
    double *v = memslot(12);
    double fi, la, S, A, cf, N, W, T, C, R, U, Vv, th, dfi, dla, daz;
    double W2, W3, W4, W5, W6, W7, C2, C3, C4, C5, T2, T4;
    double u2, u3, u4, v2, v3, v4, v5;
    NEED("Fi1:", 0, 1);
    NEED("La1:", 1, 1);
    NEED("S12 (m):", 2, 0);
    NEED("Azimut A12:", 3, 1);
    fi = v[0]; la = v[1]; S = v[2]; A = v[3];
    cf = cd(fi);
    N = el->ep2 * cf * cf;
    W = sqrt(1.0 + N);
    T = td(fi);
    C = el->c;
    R = 180.0 * 3600.0 / M_PI;
    U = S * cd(A);
    Vv = S * sd(A);
    W2 = W * W; W3 = W2 * W; W4 = W2 * W2; W5 = W4 * W; W6 = W3 * W3; W7 = W6 * W;
    C2 = C * C; C3 = C2 * C; C4 = C2 * C2; C5 = C4 * C;
    T2 = T * T; T4 = T2 * T2;
    u2 = U * U; u3 = u2 * U; u4 = u2 * u2;
    v2 = Vv * Vv; v3 = v2 * Vv; v4 = v2 * v2; v5 = v4 * Vv;

    th = R * W3 / C * U;
    th += -R * W4 * T / (2.0 * C2) * v2;
    th += -3.0 * R * W4 * N * T / (2.0 * C2) * u2;
    th += -R * W5 * (1.0 + 3.0 * T2 + N - 9.0 * N * T2) / (6.0 * C3) * U * v2;
    th += R * W5 * N * (T2 - 1.0) / (2.0 * C3) * u3;
    th += R * W6 * T * (1.0 + 3.0 * T2 + N * T - 9.0 * N * T2) / (24.0 * C4) * v4;
    th += -R * W6 * T * (4.0 + 6.0 * T2 - 13.0 * N - 9.0 * T2 * N) / (12.0 * C4) * u2 * v2;
    th += R * W7 * (1.0 + 30.0 * T2 + 45.0 * T4) / (120.0 * C5) * U * v4;
    th += -R * W7 * (2.0 + 15.0 * T2 + 15.0 * T4) / (30.0 * C5) * u3 * v2;
    dfi = th / 3600.0;

    th = R * W / (C * cf) * Vv;
    th += R * W2 * T / (C2 * cf) * U * Vv;
    th += -R * W3 * T2 / (3.0 * C3 * cf) * v3;
    th += R * W3 * (1.0 + 3.0 * T2 + N) / (3.0 * C3 * cf) * u2 * Vv;
    th += -R * W4 * T * (1.0 + 3.0 * T2) / (3.0 * C4 * cf) * U * v3;
    th += R * W4 * T * (2.0 + 3.0 * T2) / (3.0 * C4 * cf) * u3 * Vv;
    th += R * W5 * T * (1.0 + 3.0 * T2) / (15.0 * C5 * cf) * v5;
    th += -R * W5 * (1.0 + 20.0 * T2 + 30.0 * T4) / (15.0 * C5 * cf) * u2 * v3;
    th += R * W5 * (2.0 + 15.0 * T2 + 15.0 * T4) / (15.0 * C5 * cf) * u4 * Vv;
    dla = th / 3600.0;

    th = R * W * T / C * Vv;
    th += R * W2 * (1.0 + 2.0 * T2 + N) / (2.0 * C2) * Vv * U;
    th += -R * W3 * T * (1.0 + 2.0 * T2) / (6.0 * C3) * v3;
    th += R * W3 * T * (5.0 + 6.0 * T2) / (6.0 * C3) * u2 * Vv;
    th += -R * W4 * (1.0 + 20.0 * T2 + 24.0 * T4) / (24.0 * C4) * U * v3;
    th += R * W4 * (5.0 + 28.0 * T2 + 24.0 * T4) / (24.0 * C4) * u3 * Vv;
    daz = th / 3600.0;

    out_clear();
    out_ang("DFI", dfi);
    out_ang("FI2", fi + dfi);
    out_ang("DLA", dla);
    out_ang("LA2", la + dla);
    out_ang("DAZ", daz);
    out_azi("A21", daz + A - 180.0);
    show_out();
}

/* 14 Elipsoid 2. temel odev (Gauss orta enlem) */
static void m_elip_to2(void) {
    const Ellipsoid *el = E();
    double *v = memslot(13);
    double L, Fm, dF, O, Vv, N, T, V4, A, B, S, M, dal, cm, sm;
    NEED("Fi1:", 0, 1);
    NEED("La1:", 1, 1);
    NEED("Fi2:", 2, 1);
    NEED("La2:", 3, 1);
    L = v[3] - v[1];
    Fm = (v[0] + v[2]) / 2.0;
    dF = v[2] - v[0];
    cm = cd(Fm); sm = sd(Fm);
    O = el->ep2 * cm * cm;
    Vv = sqrt(1.0 + O);
    N = el->c / Vv;
    T = td(Fm);
    V4 = sq(Vv * Vv);
    A = (1.0 - sq(L * sm) / (24.0 * RHO * RHO)
         + dF * dF * (1.0 - O - 9.0 * O * T * T) / (24.0 * RHO * RHO * V4))
        * cm * L * N / RHO;
    B = (1.0 + (1.0 - 2.0 * O) * sq(L * cm) / (24.0 * RHO * RHO)
         + (O - O * T * T) * dF * dF / (24.0 * RHO * RHO * V4))
        * dF * cd(L / 2.0) * el->c / (Vv * Vv * Vv * RHO);
    S = sqrt(A * A + B * B);
    M = R2D(atan2(A, B));
    dal = (1.0 + sq(L * cm) * (1.0 + O) / (12.0 * RHO * RHO)
           + (3.0 + 8.0 * O) * dF * dF / (24.0 * RHO * RHO * V4)) * sm * L;
    out_clear();
    out_num("S", S, 4);
    out_ang("DALFA", dal);
    out_azi("A12", M - dal / 2.0);
    out_azi("A21", M + dal / 2.0 + 180.0);
    show_out();
}

/* 15 Clairaut */
static void m_clairaut(void) {
    static const char *it[2] = {"1:Fi max (tepe nokta)", "2:Fi2 (A2 verilince)"};
    const Ellipsoid *el = E();
    double *v = memslot(14), K, N1, arg, den, phi;
    int k = pick(g_title, it, 2);
    if (k < 0) return;
    NEED("Azimut A12:", 0, 1);
    NEED("Fi1:", 1, 1);
    if (k == 1) NEED("Azimut A2:", 2, 1);
    N1 = el->c / sqrt(1.0 + el->ep2 * sq(cd(v[1])));
    K = N1 * cd(v[1]) * sd(v[0]);
    if (k == 0) den = el->c * el->c - K * K * el->ep2;
    else den = el->c * el->c * sq(sd(v[2])) - K * K * el->ep2;
    out_clear();
    out_num("K", K, 4);
    if (den <= 0.0 || K * K / den > 1.0) {
        out_txt("HATA: cozum yok");
    } else {
        arg = sqrt(K * K / den);
        phi = R2D(acos(arg));
        if (v[1] < 0.0) phi = -phi;
        out_ang("FI", phi);
    }
    show_out();
}

/* 16 Pafta: kenar uzunluklari ve alan */
static void m_pafta(void) {
    const Ellipsoid *el = E();
    double *v = memslot(15), dl, N1, N2, e, s1, s2, F1, F2;
    NEED("Fi1:", 0, 1);
    NEED("Fi2:", 1, 1);
    NEED("La1:", 2, 1);
    NEED("La2:", 3, 1);
    dl = D2R(v[3] - v[2]);
    N1 = el->c / sqrt(1.0 + el->ep2 * sq(cd(v[0])));
    N2 = el->c / sqrt(1.0 + el->ep2 * sq(cd(v[1])));
    e = sqrt(el->e2);
    s1 = sd(v[0]); s2 = sd(v[1]);
    F1 = s1 / (2.0 * (1.0 - el->e2 * s1 * s1)) + log((1.0 + e * s1) / (1.0 - e * s1)) / (4.0 * e);
    F2 = s2 / (2.0 * (1.0 - el->e2 * s2 * s2)) + log((1.0 + e * s2) / (1.0 - e * s2)) / (4.0 * e);
    out_clear();
    out_num("S(F1)", fabs(dl * N1 * cd(v[0])), 3);
    out_num("S(F2)", fabs(dl * N2 * cd(v[1])), 3);
    out_num("DG", fabs(meridyen_yayi(v[1]) - meridyen_yayi(v[0])), 3);
    out_num("ALAN", fabs(el->b * el->b * dl * (F2 - F1)), 2);
    show_out();
}

/* 17 Duzlem: aciklik ve uzunluk */
static void m_duzlem(void) {
    static const char *it[2] = {"1:Aci + uzunluk bul", "2:Nokta hesapla"};
    double *v = memslot(16), dx, dy, a;
    int k = pick(g_title, it, 2);
    if (k < 0) return;
    out_clear();
    if (k == 0) {
        NEED("X1 (m):", 0, 0);
        NEED("Y1 (m):", 1, 0);
        NEED("X2 (m):", 2, 0);
        NEED("Y2 (m):", 3, 0);
        dx = v[2] - v[0];
        dy = v[3] - v[1];
        a = norm360(R2D(atan2(dy, dx)));
        out_num("DX", dx, 4);
        out_num("DY", dy, 4);
        out_ang("ACI", a);
        out_ang("ACI21", norm360(a + 180.0));
        out_num("S", sqrt(dx * dx + dy * dy), 4);
    } else {
        NEED("X1 (m):", 0, 0);
        NEED("Y1 (m):", 1, 0);
        NEED("Aci:", 2, 1);
        NEED("Uzunluk S (m):", 3, 0);
        out_num("X2", v[0] + v[3] * cd(v[2]), 4);
        out_num("Y2", v[1] + v[3] * sd(v[2]), 4);
    }
    show_out();
}

/* 18 Gauss redusyonlari */
static void m_reduksiyon(void) {
    static const char *it[4] = {"1:Soldner Y -> Gauss Y", "2:Gauss Y -> Soldner Y",
                                "3:Yon duzeltmesi", "4:Olcek duzeltmesi"};
    double *v = memslot(17), R, Y, d;
    int k = pick(g_title, it, 4);
    if (k < 0) return;
    if (v[7] == 0.0) v[7] = 6373000.0;
    out_clear();
    if (k == 0 || k == 1) {
        NEED("Y (m):", 0, 0);
        NEED("R (m):", 7, 0);
        Y = v[0]; R = v[7];
        if (k == 0) d = Y + Y * Y * Y / (6.0 * R * R) - sq(Y * Y) * Y / (24.0 * sq(R * R));
        else d = Y - Y * Y * Y / (6.0 * R * R) + sq(Y * Y) * Y / (24.0 * sq(R * R));
        out_num("Y.YENI", d, 4);
        out_num("FARK", d - Y, 4);
    } else if (k == 2) {
        NEED("X1 (m):", 0, 0);
        NEED("Y1 (m):", 1, 0);
        NEED("X2 (m):", 2, 0);
        NEED("Y2 (m):", 3, 0);
        NEED("R (m):", 7, 0);
        R = v[7];
        d = RHO * (v[1] + v[3]) * (v[0] - v[2]) / (4.0 * R * R);
        out_ang("DELTA12", d);
        out_ang("DELTA21", -d);
    } else {
        NEED("Y1 (m):", 0, 0);
        NEED("Y2 (m):", 1, 0);
        NEED("R (m):", 7, 0);
        R = v[7];
        d = -(v[0] * v[0] + v[0] * v[1] + v[1] * v[1]) / (6.0 * R * R);
        out_num("DS/S", d, 9);
        out_num("mm/km", d * 1.0e6, 3);
    }
    show_out();
}

/* ------------------------------------------------------------------ */
/* Program tablosu                                                     */
/* ------------------------------------------------------------------ */
typedef struct {
    const char *name;
    void (*fn)(void);
    const char *form;
} Mod;

static const Mod MODS[] = {
    {"Egrilik M,N,R", m_egrilik,
     "ETA2=EP2*cos(F)**2\nV=sqrt(1+ETA2)\nM=c/V**3 (meridyen)\nN=c/V (ana dikey)\nR=c/V**2=sqrt(M*N)\nc=a*a/b (kutup yar.)"},
    {"Meridyen yayi", m_meridyen,
     "G=A0*F-A2*sin(2F)\n+A4sin4F-A6sin6F\n-A8*sin(8F)\nF derece, G metre\nTers: S=G/A0\nF=S+B2*sin(2S)\n+B4*sin(4S)+B6*sin(6S)"},
    {"Cografi > Kartezyen", m_flh_xyz,
     "N=c/V\nX=(N+H)cosF cosL\nY=(N+H)cosF sinL\nZ=(N(1-e2)+H)sinF\ne2=1. eksentr. karesi"},
    {"Kartezyen > Cografi", m_xyz_flh,
     "L=atan2(Y,X)\np=sqrt(X*X+Y*Y)\ns=sinF\nN=a/sqrt(1-e2*s*s)\nF=atan2(Z+e2*N*s,p)\nH=p/cosF-N\nF,N,H iterasyonla"},
    {"Cografi > Gauss", m_cog_gauss,
     "K=La-La0 (radyan)\nc=cosF T=tanF\nY=N*c*K\n+N*c**3*K**3*\n(1-T*T+ETA2)/6+..\nX=G+N*T*c*c*K*K/2\n+N*T*c**4*K**4*\n(5-T*T+9ETA2)/24\nm=1+c*c(1+ETA2)K*K/2"},
    {"Gauss > Cografi", m_gauss_cog,
     "Fb=ayak noktasi enlemi\nFi=Fb+B2*Y**2+B4*Y**4\nLa=La0+B1*Y+B3*Y**3\n+B5*Y**5\nB1=ro/(N*cosF)\nB2=-ro*T(1+ETA2)\n/(2N*N)\nC=ro*T*Y/N-..\nm=1+(1+ETA2)Y*Y/(2N*N)"},
    {"Cografi > Soldner", m_cog_sold,
     "K=La-La0 (radyan)\nY=N*c*K-N*c*s*s*K**3/6\n-N*s*s*c**3(8-T*T)\n*K**5/120\nX=G(F)-G(F0)\n+N*s*c*K*K/2\n+N*s*c**3(5-T*T+5ETA2)\n*K**4/24\nGama=K*s+(1+ETA2)\n*s*c*c*K**3/3"},
    {"Soldner > Cografi", m_sold_cog,
     "G=G(F0)+X -> Fb\nV2=1+ETA2(Fb)\nFi=Fb-V2*T*Y*Y/(2N*N)\n+V2*T(1+3T*T+ETA2\n-9ETA2*T*T)Y**4\n/(24N**4)\nLa=La0+Y/(N*c)\n-T*T*Y**3/(3N**3*c)\n+T*T(1+3T*T)Y**5\n/(15N**5*c)"},
    {"Soldner 1. T.O.", m_sold_to1,
     "U=S*cosA V=S*sinA\nR=c/(1+ETA2)\nY2=Y1+V-U*U(Y1+V/3)\n/(2R*R)+..\nX2=X1+U+U(Y2*Y2\n-V*V/3)/(2R*R)+..\nT21=A+180\n-ro*U(Y1+V/2)/(R*R)+.."},
    {"Soldner 2. T.O.", m_sold_to2,
     "xm,ym orta degerler\nG=G(F0)+xm -> Fm\nR=c/(1+ETA2)\ndX=X2-X1 dY=Y2-Y1\nA=dY-dX*dX*dY\n/(12R*R)+..\nB=dX-dX(ym*ym\n-dY*dY/12)/(2R*R)+..\nS=sqrt(A*A+B*B)\nT12=atan2(A,B)-dT/2"},
    {"Gauss 1. T.O.", m_gauss_to1,
     "U=S*cosT V=S*sinT\nS.duz=S+S(Y1*Y1\n+Y1*Y2+Y2*Y2)/(6R*R)\nT.duz=T-d\nd=ro*ym*dX\n(1-ym*ym/3R*R)\n/(2R*R)\n-ro*dX*dY/(12R*R)"},
    {"Gauss 2. T.O.", m_gauss_to2,
     "S.elp=S.duz-S.duz*\n(Y1*Y1+Y1*Y2+Y2*Y2)\n/(6R*R)\nT.elp=T.duz+d\nd: Gauss 1. T.O. ile\nayni formul"},
    {"Elipsoid 1. T.O.", m_elip_to1,
     "Taylor serisi\nU=S*cosA V=S*sinA\nDFi=sum Bi*U*V terimleri\nDLa=sum Li*U*V terimleri\nDAz=sum Ai*U*V terimleri\nKatsayilar: W,T,ETA2,c"},
    {"Elipsoid 2. T.O.", m_elip_to2,
     "Fm=(F1+F2)/2\ndF=F2-F1 dL=L2-L1\nA=N*cosFm*dL*(1-..)\nB=c/V**3*dF*cos(dL/2)\n*(1+..)\nS=sqrt(A*A+B*B)\nM=atan2(A,B)\nA12=M-dA/2\nA21=M+dA/2+180"},
    {"Clairaut", m_clairaut,
     "K=N1*cosF1*sinA12\nFmax:\ncosF=K/sqrt(c*c\n-K*K*EP2)\nFi2:\ncosF2=K/sqrt(c*c*\nsinA2**2-K*K*EP2)"},
    {"Pafta alan/kenar", m_pafta,
     "Kenar=N*cosF*dL\nMeridyen=G(F2)-G(F1)\nAlan=b*b*dL*(Q2-Q1)\nQ=s/(2(1-e2*s*s))\n+ln((1+e*s)/(1-e*s))\n/(4e)  s=sinF"},
    {"Duzlem aci/uzunluk", m_duzlem,
     "dY=Y2-Y1 dX=X2-X1\nT=atan2(dY,dX)\nS=sqrt(dX*dX+dY*dY)\nX2=X1+S*cosT\nY2=Y1+S*sinT"},
    {"Gauss redusyonlari", m_reduksiyon,
     "Soldner->Gauss:\nYg=Y+Y**3/(6R*R)\n-Y**5/(24R**4)\nGauss->Soldner:\nisaretler ters\nd=ro(Y1+Y2)(X1-X2)\n/(4R*R)\nDS/S=-(Y1*Y1+Y1*Y2\n+Y2*Y2)/(6R*R)"}
};

#define NMOD ((int)(sizeof(MODS) / sizeof(MODS[0])))

static const char HELP_TXT[] =
    "YON: program sec\nEXE veya F6: ac\nF1: elipsoid degistir\nF2: aci birimi\n   DEG / DMS / GON\nF4: formul\nMENU: cikis\n\nGiris ekraninda EXE\nbos birakilirsa onceki\ndeger kalir.\nDMS giris: GG.DDSS\nGON: 400 grad\nX kuzey, Y dogu\nAcilar derece tutulur";

static void draw_menu(void) {
    char b[28], hdr[28];
    int i, idx;
    if (g_sel < g_top) g_top = g_sel;
    if (g_sel >= g_top + 5) g_top = g_sel - 4;
    Bdisp_AllClr_VRAM();
    put(1, 1, "JEODEZI", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 21);
    snprintf(hdr, sizeof hdr, "%s %s", E()->name, UNIT_TAG[g_unit]);
    put(1, 2, hdr, TEXT_MODE_NORMAL, TEXT_COLOR_GREEN, 0);
    snprintf(b, sizeof b, "%d/%d", g_sel + 1, NMOD);
    put(22 - slen(b), 2, b, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK, 0);
    for (i = 0; i < 5; i++) {
        idx = g_top + i;
        if (idx >= NMOD) break;
        snprintf(b, sizeof b, "%02d %s", idx + 1, MODS[idx].name);
        put(1, 3 + i, b, (idx == g_sel) ? TEXT_MODE_INVERT : TEXT_MODE_NORMAL, TEXT_COLOR_BLUE, 21);
    }
    put(1, 8, "", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 21);
    put(1, 8, "ELP", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 0);
    put(5, 8, "ACI", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 0);
    put(12, 8, "FRM", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 0);
    put(15, 8, "YRD", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 0);
    put(19, 8, "SEC", TEXT_MODE_INVERT, TEXT_COLOR_BLUE, 0);
}

int main(void) {
    int key, i;
    g_elip = 1; g_unit = 0; g_sel = 0; g_top = 0; g_n = 0;
    g_title = "JEODEZI";
    for (i = 0; i < NMEM * MEMW; i++) g_mem[i] = 0.0;

    for (;;) {
        draw_menu();
        Bdisp_PutDisp_DD();
        GetKey(&key);
        if (key == KEY_CTRL_DOWN) {
            g_sel = (g_sel + 1) % NMOD;
        } else if (key == KEY_CTRL_UP) {
            g_sel = (g_sel + NMOD - 1) % NMOD;
        } else if (key == KEY_CTRL_F1) {
            g_elip = 1 - g_elip;
        } else if (key == KEY_CTRL_F2) {
            g_unit = (g_unit + 1) % 3;
        } else if (key == KEY_CTRL_F4) {
            g_title = MODS[g_sel].name;
            load_text(MODS[g_sel].form);
            show_out();
        } else if (key == KEY_CTRL_F5) {
            g_title = "YARDIM";
            load_text(HELP_TXT);
            show_out();
        } else if (key == KEY_CTRL_F6 || key == KEY_CTRL_EXE) {
            g_title = MODS[g_sel].name;
            MODS[g_sel].fn();
        } else if (key >= KEY_CHAR_1 && key <= KEY_CHAR_9) {
            if (key - KEY_CHAR_1 < NMOD) g_sel = key - KEY_CHAR_1;
        }
    }
    return 0;
}
EOF
wc -l main.c
