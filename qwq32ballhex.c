#include <X11/Xlib.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>

#define WIDTH 600
#define HEIGHT 600
#define HEX_RADIUS 200.0
#define BALL_RADIUS 10.0
#define GRAVITY 0.5
#define FRICTION 0.9
#define DT 0.016 // ~60 FPS

typedef struct {
    double x, y;
} Point;

Point original_hex[6];
Point rotated_hex[6];
Point ball = { 0.0, 0.0 };
double ball_vel[2] = { 5.0, 0.0 };
double phi = 0.0;

Display *dpy;
Window win;
GC gc;

void init_hex() {
    for (int i = 0; i < 6; i++) {
        double angle = 2 * M_PI * i / 6;
        original_hex[i].x = HEX_RADIUS * cos(angle);
        original_hex[i].y = HEX_RADIUS * sin(angle);
    }
}

void rotate_hex(double angle) {
    double cos_a = cos(angle);
    double sin_a = sin(angle);
    for (int i = 0; i < 6; i++) {
        double x = original_hex[i].x;
        double y = original_hex[i].y;
        rotated_hex[i].x = x * cos_a - y * sin_a;
        rotated_hex[i].y = x * sin_a + y * cos_a;
    }
}

void handle_collision() {
    for (int i = 0; i < 6; i++) {
        Point v0 = rotated_hex[i];
        Point v1 = rotated_hex[(i+1)%6];
        double dx = v1.x - v0.x;
        double dy = v1.y - v0.y;
        double px = ball.x - v0.x;
        double py = ball.y - v0.y;
        double cross = dx * py - dy * px;
        double length = sqrt(dx*dx + dy*dy);
        if (length == 0) continue;
        double dist = cross / length;
        if (fabs(dist) < BALL_RADIUS) {
            double nx = dy / length;
            double ny = -dx / length;
            double v_dot_n = ball_vel[0] * nx + ball_vel[1] * ny;
            ball_vel[0] -= 2 * v_dot_n * nx * (1 + FRICTION);
            ball_vel[1] -= 2 * v_dot_n * ny * (1 + FRICTION);
            break;
        }
    }
}

int main() {
    dpy = XOpenDisplay(NULL);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, 0), 0, 0, WIDTH, HEIGHT, 0, 0, 0);
    gc = XCreateGC(dpy, win, 0, NULL);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(dpy, win);

    init_hex();

    while (1) {
        XEvent e;
        while (XPending(dpy)) {
            XNextEvent(dpy, &e);
            if (e.type == ConfigureNotify) {
                // Handle resize
            }
            if (e.type == ClientMessage || e.type == DestroyNotify) {
                exit(0);
            }
        }

        // Physics update
        ball_vel[1] += GRAVITY * DT;
        ball.x += ball_vel[0] * DT;
        ball.y += ball_vel[1] * DT;

        handle_collision();

        phi += 0.01; // Rotation speed

        // Draw
        XClearWindow(dpy, win);
        rotate_hex(phi);

        // Draw hexagon
        XPoint hex_points[6];
        for (int i = 0; i < 6; i++) {
            hex_points[i].x = rotated_hex[i].x + WIDTH/2;
            hex_points[i].y = rotated_hex[i].y + HEIGHT/2;
        }
        XDrawLines(dpy, win, gc, hex_points, 6, CoordModeOrigin);

        // Draw ball
        int x = ball.x + WIDTH/2 - BALL_RADIUS;
        int y = ball.y + HEIGHT/2 - BALL_RADIUS;
        XFillArc(dpy, win, gc, x, y, 2*BALL_RADIUS, 2*BALL_RADIUS, 0, 360*64);

        XFlush(dpy);
        usleep(16); // ~60 FPS
    }

    return 0;
}