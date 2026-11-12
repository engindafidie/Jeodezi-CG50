/*
 * ============================================================================
 * CASIO fx-CG50 GEOMETRIK JEODEZI ADD-IN (.g3a) TAM C KAYNAK KODU (PrizmSDK)
 * ============================================================================
 * Ozellikler:
 *  - [F1] tusu ile ana menuden aninda GRS80 <-> HAYFORD elipsoid gecisi
 *  - [F2] tusu ile ana menuden aninda DERECE <-> DMS (Derece-Dak-San) <-> GRAD gecisi
 *  - Gizli Yardim Menusu: Ana menudeyken sirasiyla [OPTN] ve [VARS] tuslarina
 *    basildiginda acilan ("SISTEM KALIBRASYON BILGISI" baslikli) gizli formul rehberi
 *  - Anlasilir Turkce menu isimleri ve duzeltilmis jeodezik formuller
 * ============================================================================
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

#define RHO_DEG (180.0 / M_PI)
#define RHO_SEC (180.0 * 3600.0 / M_PI)
#define RHO_GON (200.0 / M_PI)

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

static int g_elip = 0; /* 0: HAYFORD, 1: GRS80 */
static int g_unit = 1; /* 0: Ondalik Derece, 1: DMS (GG.DDSS), 2: Grad (Gon) */
static const char *UNIT_NAMES[3] = {"DERECE", "D.D.S", "GRAD"};

static const Ellipsoid* E(void) {
    return &ELLIPSOIDS[g_elip];
}

/* GG.DDSS -> Ondalik Derece cevirimi */
static double dms_to_deg(double val) {
    double sign = (val < 0) ? -1.0 : 1.0;
    val = fabs(val);
    int d = (int)val;
    double rem = (val - d) * 100.0;
    int m = (int)(rem + 1e-9);
    double s = (rem - m) * 100.0;
    return sign * (d + m / 60.0 + s / 3600.0);
}

/* Ondalik Derece -> DMS metni */
static void format_angle(double deg_val, char *buf, int buf_len) {
    if (g_unit == 0) {
        snprintf(buf, buf_len, "%.7f deg", deg_val);
    } else if (g_unit == 2) {
        snprintf(buf, buf_len, "%.6f gon", deg_val / 0.9);
    } else {
        char sign = (deg_val < 0) ? '-' : ' ';
        double v = fabs(deg_val);
        int d = (int)v;
        double mf = (v - d) * 60.0;
        int m = (int)mf;
        double s = (mf - m) * 60.0;
        if (s >= 59.995) { s = 0.0; m++; }
        if (m >= 60) { m -= 60; d++; }
        snprintf(buf, buf_len, "%c%dd %02d' %05.2f\"", sign, d, m, s);
    }
}

/* Ortak Jeodezik Fonksiyonlar */
static double meridyen_yayi(double fi_deg) {
    const Ellipsoid *el = E();
    double r = fi_deg / RHO_DEG;
    return el->A0 * fi_deg
         - el->A2 * sin(2.0 * r)
         + el->A4 * sin(4.0 * r)
         - el->A6 * sin(6.0 * r)
         - el->A8 * sin(8.0 * r);
}

static double yaya_gore_enlem(double G) {
    const Ellipsoid *el = E();
    double sigma = G / el->A0;
    double sr = sigma / RHO_DEG;
    return sigma
         + el->B2 * sin(2.0 * sr)
         + el->B4 * sin(4.0 * sr)
         + el->B6 * sin(6.0 * sr);
}

static void egrilik_MN(double fi_deg, double *M, double *N, double *Rg, double *V, double *eta2) {
    const Ellipsoid *el = E();
    double r = fi_deg / RHO_DEG;
    double c_r = cos(r);
    *eta2 = el->ep2 * c_r * c_r;
    *V = sqrt(1.0 + *eta2);
    *M = el->c / ((*V) * (*V) * (*V));
    *N = el->c / (*V);
    *Rg = el->c / ((*V) * (*V));
}

/* Kullanici sayi giris ekrani */
static double input_number(const char *prompt, int is_angle) {
    char buf[32] = "";
    int pos = 0;
    int key;
    while (1) {
        Bdisp_AllClr_VRAM();
        PrintXY(1, 1, "-- VERI GIRISI --", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
        PrintXY(1, 2, (char*)prompt, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        if (is_angle) {
            if (g_unit == 0) PrintXY(1, 3, "Birim: Ondalik Derece", TEXT_MODE_NORMAL, TEXT_COLOR_GREEN);
            else if (g_unit == 1) PrintXY(1, 3, "Birim: GG.DDSS (DMS)", TEXT_MODE_NORMAL, TEXT_COLOR_GREEN);
            else PrintXY(1, 3, "Birim: Grad (Gon)", TEXT_MODE_NORMAL, TEXT_COLOR_GREEN);
        }
        char disp[36];
        snprintf(disp, sizeof(disp), ">> %s_", buf);
        PrintXY(1, 5, disp, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        PrintXY(1, 8, "[EXE]:Onayla  [DEL]:Sil", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
        GetKey(&key);
        if (key == KEY_CTRL_EXE) {
            break;
        } else if (key == KEY_CTRL_DEL && pos > 0) {
            buf[--pos] = '\0';
        } else if (key >= KEY_CHAR_0 && key <= KEY_CHAR_9 && pos < 20) {
            buf[pos++] = '0' + (key - KEY_CHAR_0);
            buf[pos] = '\0';
        } else if (key == KEY_CHAR_DP && pos < 20) {
            buf[pos++] = '.';
            buf[pos] = '\0';
        } else if ((key == KEY_CHAR_MINUS || key == KEY_CHAR_PMINUS) && pos == 0) {
            buf[pos++] = '-';
            buf[pos] = '\0';
        }
    }
    double val = atof(buf);
    if (is_angle) {
        if (g_unit == 1) return dms_to_deg(val);
        if (g_unit == 2) return val * 0.9;
    }
    return val;
}

static void wait_exe(void) {
    int key;
    PrintXY(1, 8, "[EXE] Ana Menu...", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
    while (1) {
        GetKey(&key);
        if (key == KEY_CTRL_EXE || key == KEY_CTRL_EXIT) break;
    }
}

/* GIZLI YARDIM EKRANI: Ana menudeyken OPTN + VARS kombinasyonu ile acilir */
static void show_secret_help(void) {
    int page = 1, key;
    while (1) {
        Bdisp_AllClr_VRAM();
        if (page == 1) {
            PrintXY(1, 1, "==KALIBRASYON [1/4]==", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
            PrintXY(1, 2, "V=sqrt(1+e'^2*cos^2F)", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 3, "M=c/V^3  |  N=c/V", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 4, "R_ort=sqrt(M*N)=c/V^2", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 5, "tan(Fj)=(1-e^2)*tanF", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 6, "Paralel: L=N*cosF*dLa", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        } else if (page == 2) {
            PrintXY(1, 1, "==KALIBRASYON [2/4]==", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
            PrintXY(1, 2, "Soldner: m=1+Y^2/2R^2", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 3, "Gauss  : m=1+Y^2/2R^2", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 4, "YG = YS + Y^3/(6R^2)", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 5, "YS = YG - Y^3/(6R^2)", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 6, "Yakinsama: c=dL*sinF", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        } else if (page == 3) {
            PrintXY(1, 1, "==KALIBRASYON [3/4]==", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
            PrintXY(1, 2, "1.T.O: U=ScosA,V=SsinA", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 3, "2.T.O: Fm=(F1+F2)/2", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 4, "dF=F2-F1, dL=L2-L1", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 5, "A21 = A12 + dA +- 180", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        } else {
            PrintXY(1, 1, "==KALIBRASYON [4/4]==", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
            PrintXY(1, 2, "Claraut: N*cosF*sinA=K", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 3, "Fmax: N*cos(Fmax)=K", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 4, "dR=(ro/4R^2)(Y1+Y2)dX", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 5, "dS=-S(Y1^2+Y1Y2+Y2^2)", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
            PrintXY(1, 6, "   /(6R^2)", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        }
        PrintXY(1, 8, "[F1]:Onceki [F2]:Sonraki", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
        GetKey(&key);
        if (key == KEY_CTRL_F1 && page > 1) page--;
        else if (key == KEY_CTRL_F2 && page < 4) page++;
        else if (key == KEY_CTRL_EXIT || key == KEY_CTRL_EXE) break;
    }
}

/* Ornek Modul 1: Egrilik Yaricaplari M, N, R */
static void calc_egrilik(void) {
    double fi = input_number("Enlem (Fi):", 1);
    double M, N, Rg, V, eta2;
    egrilik_MN(fi, &M, &N, &Rg, &V, &eta2);
    char l1[32], l2[32], l3[32], l4[32];
    snprintf(l1, sizeof(l1), "M = %.4f m", M);
    snprintf(l2, sizeof(l2), "N = %.4f m", N);
    snprintf(l3, sizeof(l3), "R = %.4f m", Rg);
    snprintf(l4, sizeof(l4), "V = %.7f", V);
    Bdisp_AllClr_VRAM();
    PrintXY(1, 1, "--EGRILIK YARICAPI--", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
    PrintXY(1, 3, l1, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
    PrintXY(1, 4, l2, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
    PrintXY(1, 5, l3, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
    PrintXY(1, 6, l4, TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
    wait_exe();
}

int main(void) {
    int key;
    while (1) {
        Bdisp_AllClr_VRAM();
        char hdr[32];
        snprintf(hdr, sizeof(hdr), "F1:%s F2:%s", E()->name, UNIT_NAMES[g_unit]);
        PrintXY(1, 1, hdr, TEXT_MODE_INVERT, TEXT_COLOR_BLUE);
        PrintXY(1, 2, "1:Egrilik(M,N) 2:YayUz", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        PrintXY(1, 3, "3:FLH<->XYZ    4:Soldnr", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        PrintXY(1, 4, "5:Soldner T.O  6:Gauss", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        PrintXY(1, 5, "7:Gauss T.O/In 8:ElipTO", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        PrintXY(1, 6, "9:Claraut/Ucg  0:Pafta", TEXT_MODE_NORMAL, TEXT_COLOR_BLACK);
        PrintXY(1, 8, "[F1]:Elip [F2]:AciBirim", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);

        GetKey(&key);
        if (key == KEY_CTRL_F1) {
            g_elip = 1 - g_elip;
        } else if (key == KEY_CTRL_F2) {
            g_unit = (g_unit + 1) % 3;
        } else if (key == KEY_CTRL_OPTN) {
            /* Gizli 2-Tus Kombinasyonu: OPTN ardindan VARS */
            int key2;
            GetKey(&key2);
            if (key2 == KEY_CTRL_VARS) {
                show_secret_help();
            }
        } else if (key == KEY_CHAR_1) {
            calc_egrilik();
        }
    }
    return 0;
}
