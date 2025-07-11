#include <X11/Xlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define WIDTH           800
#define HEIGHT          600
#define HEX_RADIUS      200.0
#define BALL_RADIUS     20.0
#define GRAVITY         980.0   /* px/s² */
#define RESTITUTION     0.8
#define FRICTION_COEF   0.2
#define ANGULAR_VELOCITY 0.5    /* rad/s */
#define FRAME_RATE      60

int main() {
    Display *dpy;
    int screen;
    Window win;
    GC gc;
    Pixmap buffer;
    XEvent ev;
    double hx[6], hy[6];
    double bx, by, vx, vy;
    double angle = 0.0;
    double cx = WIDTH / 2.0, cy = HEIGHT / 2.0;
    struct timespec req = {0, (long)(1e9 / FRAME_RATE)};

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    screen = DefaultScreen(dpy);
    win = XCreateSimpleWindow(
        dpy,
        RootWindow(dpy, screen),
        0, 0, WIDTH, HEIGHT, 1,
        BlackPixel(dpy, screen),
        WhitePixel(dpy, screen)
    );
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    XMapWindow(dpy, win);
    gc = DefaultGC(dpy, screen);

    buffer = XCreatePixmap(
        dpy, win, WIDTH, HEIGHT,
        DefaultDepth(dpy, screen)
    );

    /* Precompute unrotated hexagon vertices */
    for (int i = 0; i < 6; ++i) {
        double theta = 2.0 * M_PI * i / 6.0;
        hx[i] = HEX_RADIUS * cos(theta);
        hy[i] = HEX_RADIUS * sin(theta);
    }

    /* Ball starts at center, at rest */
    bx = cx;  by = cy;
    vx = vy = 0.0;

    while (1) {
        /* Handle keypress to exit */
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == KeyPress) goto cleanup;
        }

        double dt = 1.0 / FRAME_RATE;
        /* Physics update */
        vy += GRAVITY * dt;
        bx += vx * dt;
        by += vy * dt;

        /* Compute rotated hexagon in screen coords */
        double px[6], py[6];
        for (int i = 0; i < 6; ++i) {
            double rx = hx[i] * cos(angle)
                      - hy[i] * sin(angle);
            double ry = hx[i] * sin(angle)
                      + hy[i] * cos(angle);
            px[i] = cx + rx;
            py[i] = cy + ry;
        }

        /* Collision with each hexagon edge */
        for (int i = 0; i < 6; ++i) {
            int j = (i + 1) % 6;
            double dx = px[j] - px[i];
            double dy = py[j] - py[i];
            double len = sqrt(dx*dx + dy*dy);
            double tx = dx / len, ty = dy / len;
            /* inward normal (left-hand perp for CCW) */
            double nx = ty, ny = -tx;

            double fx = bx - px[i], fy = by - py[i];
            double proj_t = fx*tx + fy*ty;
            /* only if foot falls within segment */
            if (proj_t < 0 || proj_t > len) continue;

            double proj_n = fx*nx + fy*ny;
            if (proj_n < BALL_RADIUS) {
                /* push ball out of wall */
                double pen = BALL_RADIUS - proj_n;
                bx += pen * nx;
                by += pen * ny;
                /* decompose velocity */
                double v_n = vx*nx + vy*ny;
                double v_t = vx*tx + vy*ty;
                /* reflect normal with restitution */
                v_n = -RESTITUTION * v_n;
                /* reduce tangential by friction */
                v_t *= (1.0 - FRICTION_COEF);
                /* reassemble */
                vx = v_n*nx + v_t*tx;
                vy = v_n*ny + v_t*ty;
            }
        }

        angle += ANGULAR_VELOCITY * dt;

        /* Draw to off-screen pixmap */
        XSetForeground(dpy, gc, WhitePixel(dpy, screen));
        XFillRectangle(dpy, buffer, gc, 0, 0, WIDTH, HEIGHT);

        XSetForeground(dpy, gc, BlackPixel(dpy, screen));
        /* Hexagon edges */
        for (int i = 0; i < 6; ++i) {
            int j = (i + 1) % 6;
            XDrawLine(
                dpy, buffer, gc,
                (int)px[i], (int)py[i],
                (int)px[j], (int)py[j]
            );
        }
        /* Ball */
        int bx_i = (int)(bx - BALL_RADIUS);
        int by_i = (int)(by - BALL_RADIUS);
        int dia = (int)(2 * BALL_RADIUS);
        XFillArc(
            dpy, buffer, gc,
            bx_i, by_i, dia, dia,
            0, 360*64
        );

        /* Blit to window */
        XCopyArea(
            dpy, buffer, win, gc,
            0, 0, WIDTH, HEIGHT, 0, 0
        );
        XFlush(dpy);

        nanosleep(&req, NULL);
    }

cleanup:
    XFreePixmap(dpy, buffer);
    XCloseDisplay(dpy);
    return 0;
}
