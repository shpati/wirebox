/*
 * WireBox GUI - WireWorld simulator and viewer.  Pure C, Win32 + GDI only.
 *
 * Build (MinGW-w64):
 *   gcc -O2 -mwindows wireworld_gui.c -o wireworld_gui.exe -lgdi32 -lcomdlg32 -lshell32 -luser32
 * Build (MSVC):
 *   cl /O2 wireworld_gui.c user32.lib gdi32.lib comdlg32.lib shell32.lib /link /SUBSYSTEM:WINDOWS
 *
 * Input format (same as the original WireBox): one grid row per line,
 *   e = electron head, ~ = electron tail, # = wire, anything else (. or space) = blank.
 * The grid size is taken from the file; there is no fixed ROW/COL.
 *
 * Usage: wireworld_gui [file] [-i file] [-n cycles] [-r steps_per_sec] [-o prefix]
 *   -n  stop after this many cycles (default: run until stopped)
 *   -r  simulation speed, 0 = as fast as possible (default 100)
 *   -o  prefix for saved snapshots (default "output")
 *
 * Keys:  Space run/pause | N or Right: one step | Up/Down (+/-): speed
 *        R reset | F fit | 1 100% | T save text | P save PGM | Ctrl+O open | H help
 * Mouse: wheel zooms at cursor, left-drag pans. Files can be dropped on the window.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- core begin ---------------------------------------------------------- */
/*
 * Simulation core. Cell states double as palette indices of the 8-bpp DIB that
 * is shown on screen, so the grid IS the bitmap: no per-frame conversion.
 * The grid has a 1-cell blank border, so neighbour lookups need no bounds checks.
 *
 * A generation costs O(#electron heads), not O(width*height): only wire cells
 * touching a head can become a head, and those are collected while counting.
 */
enum { C_EMPTY = 0, C_WIRE = 1, C_HEAD = 2, C_TAIL = 3 };

typedef struct {
    int w, h, stride, rows;          /* stride = bytes per padded row (multiple of 4) */
    unsigned char *cells;            /* stride*rows, padded by 1 on every side         */
    unsigned char *cnt;              /* head-neighbour counters, all zero between steps */
    unsigned char *init;             /* copy of the initial state for Reset             */
    int *pool, *heads, *tails, *cand;/* index lists into cells                          */
    int nh, nt, off[8];
    unsigned long long gen;
} Sim;

static void sim_setup_offsets(Sim *s)
{
    const int d = s->stride;
    const int o[8] = { -d - 1, -d, -d + 1, -1, 1, d - 1, d, d + 1 };
    memcpy(s->off, o, sizeof o);
}

static void sim_rebuild(Sim *s)
{
    int x, y;
    s->nh = s->nt = 0;
    s->gen = 0;
    for (y = 1; y <= s->h; y++) {
        const unsigned char *r = s->cells + (size_t)y * s->stride;
        for (x = 1; x <= s->w; x++) {
            if (r[x] == C_HEAD)      s->heads[s->nh++] = y * s->stride + x;
            else if (r[x] == C_TAIL) s->tails[s->nt++] = y * s->stride + x;
        }
    }
}

static void sim_step(Sim *s)
{
    unsigned char *c = s->cells, *cn = s->cnt;
    int *heads = s->heads, *tails = s->tails, *cand = s->cand;
    const int nh = s->nh, nt = s->nt;
    const int o0 = s->off[0], o1 = s->off[1], o2 = s->off[2], o3 = s->off[3];
    const int o4 = s->off[4], o5 = s->off[5], o6 = s->off[6], o7 = s->off[7];
    int nc = 0, m = 0, i;

#define TOUCH(q) do { if (c[q] == C_WIRE && !cn[q]++) cand[nc++] = (q); } while (0)
    for (i = 0; i < nh; i++) {
        const int p = heads[i];
        TOUCH(p + o0); TOUCH(p + o1); TOUCH(p + o2); TOUCH(p + o3);
        TOUCH(p + o4); TOUCH(p + o5); TOUCH(p + o6); TOUCH(p + o7);
    }
#undef TOUCH
    for (i = 0; i < nt; i++) c[tails[i]] = C_WIRE;   /* tail -> wire */
    for (i = 0; i < nh; i++) c[heads[i]] = C_TAIL;   /* head -> tail */
    for (i = 0; i < nc; i++) {                       /* wire -> head if 1 or 2 head neighbours */
        const int q = cand[i];
        const unsigned n = cn[q];
        cn[q] = 0;
        if (n - 1u < 2u) { c[q] = C_HEAD; cand[m++] = q; }
    }
    /* rotate the three buffers instead of copying */
    s->cand = tails;
    s->tails = heads;  s->nt = nh;
    s->heads = cand;   s->nh = m;
    s->gen++;
}
/* ---- core end ------------------------------------------------------------ */

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>

/* fixed-size window: no resize border, no maximize button (use WS_OVERLAPPEDWINDOW for a resizable one) */
#define WIN_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

static Sim      S;
static HWND     g_hwnd;
static HDC      g_mem;
static HBITMAP  g_bmp;
static HBRUSH   g_bg;
static double   g_zoom = 1.0, g_px, g_py;       /* view: pixels per cell, pan offset */
static int      g_running = 1, g_rate = 100, g_dirty = 1;
static unsigned long long g_maxcyc = 0;
static char     g_path[MAX_PATH] = "input.txt";
static char     g_out[MAX_PATH] = "output";
static char     g_msg[160];
static double   g_msg_until;
static LARGE_INTEGER g_qf;
static double   g_last, g_acc, g_sps, g_win_t;
static unsigned long long g_win_steps;
static int      g_drag, g_dx, g_dy;

static double now_s(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)g_qf.QuadPart;
}

static void set_msg(const char *s)
{
    snprintf(g_msg, sizeof g_msg, "%s", s);
    g_msg_until = now_s() + 4.0;
}

/* RGBQUAD is B,G,R,reserved.  Index = cell state. */
static const RGBQUAD PAL[4] = {
    { 20, 12, 10, 0 },     /* blank: deep midnight  (10,12,20)   */
    { 138, 100, 84, 0 },   /* wire : slate blue     (84,100,138) */
    { 255, 225, 90, 0 },   /* head : electric cyan  (90,225,255) */
    { 130, 80, 255, 0 }    /* tail : hot pink       (255,80,130) */
};

/* ------------------------------------------------------------------ loading */
static void free_aux(Sim *s)
{
    free(s->cnt); free(s->init); free(s->pool);
}

static int load_file(const char *path, char *err)
{
    FILE *f;
    long sz;
    char *buf, *p, *s, *e, *t, *end;
    int w = 0, h = 0, lines = 0, x, y, stride, rows;
    size_t bytes, ne = 0, i;
    unsigned char *bits = NULL, *row;
    HBITMAP bmp;
    Sim T;
    struct { BITMAPINFOHEADER hd; RGBQUAD c[4]; } bi;

    f = fopen(path, "rb");
    if (!f) { strcpy(err, "Cannot open input file."); return 0; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); strcpy(err, "Input file is empty."); return 0; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); free(buf); strcpy(err, "Cannot read input file."); return 0;
    }
    fclose(f);
    end = buf + sz;                    /* the file may contain NUL bytes (blank = hex 00): never use NUL as a terminator */
    p = buf;
    if (sz >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;

    /* pass 1: dimensions (trailing blanks/NUL/CR ignored, trailing empty lines dropped) */
    for (s = p; s < end; s = e < end ? e + 1 : end) {
        for (e = s; e < end && *e != '\n'; e++) ;
        for (t = e; t > s && (t[-1] == '\r' || t[-1] == ' ' || t[-1] == '\t' || t[-1] == '.' || t[-1] == 0); t--) ;
        lines++;
        if (t > s) { h = lines; if ((int)(t - s) > w) w = (int)(t - s); }
    }
    if (w == 0 || h == 0) { free(buf); strcpy(err, "No cells found in input file."); return 0; }
    if (w > 30000 || h > 30000 || (double)(w + 2) * (h + 2) > 4e8) {
        free(buf); strcpy(err, "Grid is too large."); return 0;
    }

    stride = (w + 2 + 3) & ~3;
    rows = h + 2;
    memset(&bi, 0, sizeof bi);
    bi.hd.biSize = sizeof(BITMAPINFOHEADER);
    bi.hd.biWidth = stride;
    bi.hd.biHeight = -rows;                    /* top-down */
    bi.hd.biPlanes = 1;
    bi.hd.biBitCount = 8;
    bi.hd.biCompression = BI_RGB;
    bi.hd.biClrUsed = 4;
    memcpy(bi.c, PAL, sizeof PAL);
    bmp = CreateDIBSection(NULL, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    if (!bmp || !bits) { free(buf); strcpy(err, "Out of memory (bitmap)."); return 0; }

    /* pass 2: fill the grid straight into the bitmap */
    for (y = 0, s = p; y < h && s < end; y++, s = e < end ? e + 1 : end) {
        for (e = s; e < end && *e != '\n'; e++) ;
        row = bits + (size_t)(y + 1) * stride + 1;
        for (x = 0; x < w && s + x < e; x++) {
            unsigned char v;
            switch (s[x]) {
            case '#': v = C_WIRE; break;
            case 'e': case 'E': v = C_HEAD; break;
            case '~': v = C_TAIL; break;
            default:  v = C_EMPTY;
            }
            row[x] = v;
            ne += (v != C_EMPTY);
        }
    }
    free(buf);

    memset(&T, 0, sizeof T);
    T.w = w; T.h = h; T.stride = stride; T.rows = rows; T.cells = bits;
    bytes = (size_t)stride * rows;
    T.cnt = (unsigned char *)calloc(bytes, 1);
    T.init = (unsigned char *)malloc(bytes);
    T.pool = (int *)malloc(3 * (ne + 1) * sizeof(int));
    if (!T.cnt || !T.init || !T.pool) {
        free_aux(&T); DeleteObject(bmp); strcpy(err, "Out of memory."); return 0;
    }
    memcpy(T.init, bits, bytes);
    T.heads = T.pool; T.tails = T.pool + (ne + 1); T.cand = T.pool + 2 * (ne + 1);
    (void)i;
    sim_setup_offsets(&T);
    sim_rebuild(&T);

    /* commit */
    SelectObject(g_mem, bmp);
    if (g_bmp) DeleteObject(g_bmp);
    g_bmp = bmp;
    free_aux(&S);
    S = T;
    return 1;
}

static int pick_file(HWND owner, char *path)
{
    OPENFILENAMEA o;
    char tmp[MAX_PATH] = "";
    memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = owner;
    o.lpstrFilter = "WireWorld text (*.txt)\0*.txt\0All files\0*.*\0";
    o.lpstrFile = tmp;
    o.nMaxFile = MAX_PATH;
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&o)) return 0;
    strcpy(path, tmp);
    return 1;
}

/* --------------------------------------------------------------------- view */
static void client_size(int *cw, int *ch)
{
    RECT r;
    GetClientRect(g_hwnd, &r);
    *cw = r.right > 1 ? r.right : 1;
    *ch = r.bottom > 1 ? r.bottom : 1;
}

static void fit_view(void)
{
    int cw, ch;
    double zx, zy, z;
    client_size(&cw, &ch);
    zx = (double)cw / S.w; zy = (double)ch / S.h;
    z = zx < zy ? zx : zy;
    if (z >= 1.0) z = floor(z);
    if (z > 64.0) z = 64.0;
    g_zoom = z;
    g_px = (cw - S.w * z) / 2.0;
    g_py = (ch - S.h * z) / 2.0;
}

static void zoom_at(double nz, int mx, int my)
{
    if (nz < 1.0 / 64) nz = 1.0 / 64;
    if (nz > 64.0) nz = 64.0;
    g_px = mx - (mx - g_px) * nz / g_zoom;
    g_py = my - (my - g_py) * nz / g_zoom;
    g_zoom = nz;
}

/* integer steps above 100% keep pixels square; fractional below */
static double zoom_in(double z)
{
    if (z < 1.0) { z *= 1.25; return z > 0.99 ? 1.0 : z; }
    if (z >= 8.0) return floor(z * 1.25 + 0.5);
    return floor(z + 1e-9) + 1.0;
}
static double zoom_out(double z)
{
    if (z > 8.0) return floor(z / 1.25 + 0.5);
    if (z > 1.0) return ceil(z - 1e-9) - 1.0;
    return z / 1.25;
}

static void paint(HDC hdc)
{
    RECT rc;
    int cw, ch, x0, y0, x1, y1;
    GetClientRect(g_hwnd, &rc);
    cw = rc.right; ch = rc.bottom;
    x0 = (int)floor(-g_px / g_zoom);            if (x0 < 0) x0 = 0;
    y0 = (int)floor(-g_py / g_zoom);            if (y0 < 0) y0 = 0;
    x1 = (int)ceil((cw - g_px) / g_zoom);       if (x1 > S.w) x1 = S.w;
    y1 = (int)ceil((ch - g_py) / g_zoom);       if (y1 > S.h) y1 = S.h;
    if (x0 < x1 && y0 < y1) {
        int dx  = (int)floor(g_px + x0 * g_zoom + 0.5), dy  = (int)floor(g_py + y0 * g_zoom + 0.5);
        int dx2 = (int)floor(g_px + x1 * g_zoom + 0.5), dy2 = (int)floor(g_py + y1 * g_zoom + 0.5);
        ExcludeClipRect(hdc, dx, dy, dx2, dy2);           /* background only around the image: no flicker */
        FillRect(hdc, &rc, g_bg);
        SelectClipRgn(hdc, NULL);
        SetStretchBltMode(hdc, COLORONCOLOR);             /* nearest neighbour */
        StretchBlt(hdc, dx, dy, dx2 - dx, dy2 - dy, g_mem, 1 + x0, 1 + y0, x1 - x0, y1 - y0, SRCCOPY);
    } else {
        FillRect(hdc, &rc, g_bg);
    }
}

/* ------------------------------------------------------------- simulation */
static void tick(double t)
{
    double dt = t - g_last, deadline = t + 0.012;
    unsigned long long done = 0;
    g_last = t;
    if (dt > 0.25) dt = 0.25;                          /* window drag / stall: don't burst */
    if (g_rate) g_acc += dt * g_rate; else g_acc = 1e18;
    while (g_acc >= 1.0 && !(g_maxcyc && S.gen >= g_maxcyc)) {
        sim_step(&S);
        g_acc -= 1.0;
        done++;
        if ((done & 3) == 0 && now_s() > deadline) break;
    }
    if (!g_rate || g_acc > g_rate * 0.2 + 1.0) g_acc = 0.0;   /* can't keep up: drop the backlog */
    if (g_maxcyc && S.gen >= g_maxcyc) {
        g_running = 0;
        set_msg("reached cycle limit");
    }
    if (done) g_dirty = 1;
    g_win_steps += done;
    if (t - g_win_t >= 0.5) {
        g_sps = (double)g_win_steps / (t - g_win_t);
        g_win_steps = 0; g_win_t = t;
    }
}

static void toggle_run(void)
{
    if (!g_running && g_maxcyc && S.gen >= g_maxcyc) g_maxcyc = 0;   /* continue past -n */
    g_running = !g_running;
    g_last = g_win_t = now_s();
    g_acc = 0; g_sps = 0; g_win_steps = 0;
}

static void rate_change(int dir)
{
    static const int P[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000 };
    const int n = (int)(sizeof P / sizeof *P);
    int i;
    if (dir > 0) {
        if (g_rate == 0) return;
        for (i = 0; i < n; i++) if (P[i] > g_rate) { g_rate = P[i]; return; }
        g_rate = 0;                                    /* past the last preset: unlimited */
    } else {
        if (g_rate == 0) { g_rate = P[n - 1]; return; }
        for (i = n - 1; i >= 0; i--) if (P[i] < g_rate) { g_rate = P[i]; return; }
        g_rate = P[0];
    }
}

static void reset_sim(void)
{
    memcpy(S.cells, S.init, (size_t)S.stride * S.rows);
    memset(S.cnt, 0, (size_t)S.stride * S.rows);
    sim_rebuild(&S);
    g_dirty = 1;
    g_last = now_s(); g_acc = 0;
}

static void save_snapshot(int pgm)
{
    static const unsigned char GRAY[4] = { 0, 35, 101, 126 };   /* same byte values as the original PGM */
    static const char CH[4] = { '.', '#', 'e', '~' };
    char name[MAX_PATH + 32], msg[MAX_PATH + 64];
    FILE *f;
    int x, y;
    snprintf(name, sizeof name, "%s_%.0f.%s", g_out, (double)S.gen, pgm ? "pgm" : "txt");
    f = fopen(name, pgm ? "wb" : "w");
    if (!f) { set_msg("could not create output file"); return; }
    if (pgm) fprintf(f, "P5\n%d %d\n255\n", S.w, S.h);
    for (y = 1; y <= S.h; y++) {
        const unsigned char *r = S.cells + (size_t)y * S.stride;
        for (x = 1; x <= S.w; x++) {
            if (pgm) fputc(GRAY[r[x]], f); else fputc(CH[r[x]], f);
        }
        if (!pgm) fputc('\n', f);
    }
    fclose(f);
    snprintf(msg, sizeof msg, "saved %s", name);
    set_msg(msg);
}

static void update_title(double t)
{
    char buf[512], rate[32];
    const char *base = g_path, *q;
    int n;
    if ((q = strrchr(g_path, '\\')) != NULL) base = q + 1;
    else if ((q = strrchr(g_path, '/')) != NULL) base = q + 1;
    if (g_rate) snprintf(rate, sizeof rate, "%d/s", g_rate); else strcpy(rate, "max");
    n = snprintf(buf, sizeof buf, "%s - cycle %.0f | heads %d | %s", base, (double)S.gen, S.nh,
                 g_running ? "running" : "paused");
    if (g_running) n += snprintf(buf + n, sizeof buf - n, " @ %s (%.0f/s actual)", rate, g_sps);
    else           n += snprintf(buf + n, sizeof buf - n, " (speed %s)", rate);
    n += snprintf(buf + n, sizeof buf - n, " | zoom %.0f%%", g_zoom * 100.0);
    if (t < g_msg_until) snprintf(buf + n, sizeof buf - n, " | %s", g_msg);
    SetWindowTextA(g_hwnd, buf);
}

static void open_path(const char *path)
{
    char err[160];
    if (!load_file(path, err)) { MessageBoxA(g_hwnd, err, "WireBox", MB_OK | MB_ICONERROR); return; }
    strncpy(g_path, path, MAX_PATH - 1);
    g_path[MAX_PATH - 1] = 0;
    g_last = now_s(); g_acc = 0;
    fit_view();
    g_dirty = 1;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void show_help(void)
{
    MessageBoxA(g_hwnd,
        "Space\t\trun / pause\n"
        "N or Right\tsingle step\n"
        "Up/Down, +/-\tspeed (steps per second, last = max)\n"
        "R\t\treset to initial state\n"
        "F\t\tfit to window\n"
        "1\t\t100% zoom\n"
        "T / P\t\tsave snapshot as text / PGM\n"
        "Ctrl+O\t\topen file (or drop a file on the window)\n"
        "Mouse wheel\tzoom at cursor\n"
        "Left drag\tpan\n\n"
        "e = head   ~ = tail   # = wire   . = blank",
        "WireBox", MB_OK | MB_ICONINFORMATION);
}

/* ------------------------------------------------------------------ window */
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        paint(hdc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_SIZE:
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_MOUSEWHEEL: {
        POINT pt;
        int d = (short)HIWORD(wp);
        pt.x = (short)LOWORD(lp); pt.y = (short)HIWORD(lp);
        ScreenToClient(h, &pt);
        zoom_at(d > 0 ? zoom_in(g_zoom) : zoom_out(g_zoom), pt.x, pt.y);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN:
        g_drag = 1; g_dx = (short)LOWORD(lp); g_dy = (short)HIWORD(lp);
        SetCapture(h);
        return 0;
    case WM_MOUSEMOVE:
        if (g_drag) {
            int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
            g_px += x - g_dx; g_py += y - g_dy;
            g_dx = x; g_dy = y;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        g_drag = 0;
        ReleaseCapture();
        return 0;
    case WM_KEYDOWN:
        switch (wp) {
        case VK_SPACE: toggle_run(); break;
        case 'N': case VK_RIGHT:
            g_running = 0; sim_step(&S); g_dirty = 1; break;
        case VK_UP: case VK_ADD: case VK_OEM_PLUS:       rate_change(+1); break;
        case VK_DOWN: case VK_SUBTRACT: case VK_OEM_MINUS: rate_change(-1); break;
        case 'R': reset_sim(); break;
        case 'F': fit_view(); InvalidateRect(h, NULL, FALSE); break;
        case '1': { int cw, ch; client_size(&cw, &ch); zoom_at(1.0, cw / 2, ch / 2); InvalidateRect(h, NULL, FALSE); break; }
        case 'T': save_snapshot(0); break;
        case 'P': save_snapshot(1); break;
        case 'H': case VK_F1: show_help(); break;
        case 'O':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                char p[MAX_PATH];
                if (pick_file(h, p)) open_path(p);
            }
            break;
        }
        update_title(now_s());
        return 0;
    case WM_DROPFILES: {
        char p[MAX_PATH];
        if (DragQueryFileA((HDROP)wp, 0, p, MAX_PATH)) open_path(p);
        DragFinish((HDROP)wp);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show)
{
    WNDCLASSEXA wc;
    MSG msg;
    RECT wa, r;
    char err[160];
    int i, explicit_path = 0, cw, ch;
    double z, mw, mh, last_title = 0, last_paint = 0, t;
    HMODULE u32;
    FARPROC dpi;
    HDC sdc;
    (void)hp; (void)cmd;

    u32 = GetModuleHandleA("user32.dll");               /* sharp pixels on HiDPI displays */
    dpi = u32 ? GetProcAddress(u32, "SetProcessDPIAware") : NULL;
    if (dpi) ((BOOL (WINAPI *)(void))dpi)();
    QueryPerformanceFrequency(&g_qf);

    for (i = 1; i < __argc; i++) {
        const char *a = __argv[i];
        if (!strcmp(a, "-n") && i + 1 < __argc)      g_maxcyc = strtoull(__argv[++i], NULL, 10);
        else if (!strcmp(a, "-r") && i + 1 < __argc) g_rate = atoi(__argv[++i]);
        else if (!strcmp(a, "-o") && i + 1 < __argc) strncpy(g_out, __argv[++i], MAX_PATH - 1);
        else if (!strcmp(a, "-i") && i + 1 < __argc) { strncpy(g_path, __argv[++i], MAX_PATH - 1); explicit_path = 1; }
        else if (!strcmp(a, "-h") || !strcmp(a, "-help") || !strcmp(a, "/?")) {
            MessageBoxA(NULL, "Usage: wireworld_gui [file] [-i file] [-n cycles] [-r steps_per_sec] [-o prefix]\n\n"
                              "Press H inside the program for the key list.", "WireBox", MB_OK);
            return 0;
        } else { strncpy(g_path, a, MAX_PATH - 1); explicit_path = 1; }
    }
    if (g_rate < 0) g_rate = 0;

    sdc = GetDC(NULL);
    g_mem = CreateCompatibleDC(sdc);
    ReleaseDC(NULL, sdc);
    g_bg = CreateSolidBrush(RGB(18, 20, 28));

    if (!load_file(g_path, err)) {
        if (explicit_path || !pick_file(NULL, g_path) || !load_file(g_path, err)) {
            MessageBoxA(NULL, err[0] ? err : "No input file selected.", "WireBox", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = "WireBoxWnd";
    RegisterClassExA(&wc);

    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    mw = (wa.right - wa.left) * 0.92; mh = (wa.bottom - wa.top) * 0.85;
    z = mw / S.w < mh / S.h ? mw / S.w : mh / S.h;
    if (z >= 1.0) z = floor(z);
    if (z > 16.0) z = 16.0;
    cw = (int)(S.w * z + 0.5); ch = (int)(S.h * z + 0.5);
    if (cw < 360) cw = 360;
    if (ch < 240) ch = 240;
    r.left = r.top = 0; r.right = cw; r.bottom = ch;
    AdjustWindowRect(&r, WIN_STYLE, FALSE);
    g_hwnd = CreateWindowExA(0, "WireBoxWnd", "WireBox", WIN_STYLE,
                             CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                             NULL, NULL, hi, NULL);
    if (!g_hwnd) return 1;
    DragAcceptFiles(g_hwnd, TRUE);
    fit_view();
    ShowWindow(g_hwnd, show);

    g_last = g_win_t = now_s();
    for (;;) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        t = now_s();
        if (g_running) tick(t); else g_last = t;
        if (g_dirty && t - last_paint >= 0.015) {       /* cap redraws at ~65 Hz */
            InvalidateRect(g_hwnd, NULL, FALSE);
            UpdateWindow(g_hwnd);
            g_dirty = 0; last_paint = t;
        }
        if (t - last_title >= 0.25) { update_title(t); last_title = t; }
        MsgWaitForMultipleObjects(0, NULL, FALSE, g_running ? (g_rate ? 1 : 0) : 250, QS_ALLINPUT);
    }
done:
    return (int)msg.wParam;
}