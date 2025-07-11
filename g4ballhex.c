#include <X11/Xlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define WIDTH 800
#define HEIGHT 600
#define NUM_SIDES 6
#define HEX_RADIUS 200.0
#define BALL_RADIUS 10.0
#define G 98.0 // pixels per second^2, scaled for visibility
#define DT 0.01
#define OMEGA 0.5 // rad/s
#define RESTITUTION 0.8
#define MU 0.3

typedef struct {
  double x, y;
} Point;

typedef struct {
  double x, y, vx, vy;
} Ball;

Point rotate_point(Point p, Point center, double angle) {
  double s = sin(angle), c = cos(angle);
  double px = p.x - center.x;
  double py = p.y - center.y;
  double nx = px * c - py * s;
  double ny = px * s + py * c;
  return (Point){center.x + nx, center.y + ny};
}

void get_hex_vertices(Point vertices[], Point center, double angle) {
  for (int i = 0; i < NUM_SIDES; i++) {
    double theta = 2 * M_PI * i / NUM_SIDES + angle;
    vertices[i].x = center.x + HEX_RADIUS * cos(theta);
    vertices[i].y = center.y + HEX_RADIUS * sin(theta);
  }
}

Point closest_on_segment(Point pos, Point p1, Point p2) {
  Point dir = {p2.x - p1.x, p2.y - p1.y};
  double len2 = dir.x * dir.x + dir.y * dir.y;
  if (len2 == 0)
    return p1;
  double s = ((pos.x - p1.x) * dir.x + (pos.y - p1.y) * dir.y) / len2;
  s = fmax(0, fmin(1, s));
  return (Point){p1.x + s * dir.x, p1.y + s * dir.y};
}

Point wall_velocity(Point p, Point center, double omega) {
  double dx = p.x - center.x;
  double dy = p.y - center.y;
  return (Point){-omega * dy, omega * dx};
}

void resolve_collision(Ball *ball, Point p1, Point p2, Point center,
                       double omega) {
  Point closest = closest_on_segment((Point){ball->x, ball->y}, p1, p2);
  Point to_ball = {ball->x - closest.x, ball->y - closest.y};
  double dist = sqrt(to_ball.x * to_ball.x + to_ball.y * to_ball.y);
  if (dist >= BALL_RADIUS || dist == 0)
    return;
  Point normal = {to_ball.x / dist, to_ball.y / dist};
  double penetration = BALL_RADIUS - dist;
  ball->x += normal.x * penetration;
  ball->y += normal.y * penetration;
  Point v_ball = {ball->vx, ball->vy};
  Point v_wall = wall_velocity(closest, center, omega);
  Point v_rel = {v_ball.x - v_wall.x, v_ball.y - v_wall.y};
  double v_n = v_rel.x * normal.x + v_rel.y * normal.y;
  if (v_n >= 0)
    return;
  double j_n = -(1 + RESTITUTION) * v_n;
  Point tangent = {-normal.y, normal.x};
  double v_t = v_rel.x * tangent.x + v_rel.y * tangent.y;
  double j_t = -v_t;
  double mu_jn = MU * fabs(j_n);
  if (fabs(j_t) > mu_jn) {
    j_t = (v_t > 0 ? -mu_jn : mu_jn);
  }
  ball->vx += j_n * normal.x + j_t * tangent.x;
  ball->vy += j_n * normal.y + j_t * tangent.y;
}

int main() {
  Display *display = XOpenDisplay(NULL);
  if (!display)
    exit(1);
  int screen = DefaultScreen(display);
  Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 0,
                                      0, WIDTH, HEIGHT, 1,
                                      BlackPixel(display, screen),
                                      WhitePixel(display, screen));
  XSelectInput(display, window, KeyPressMask | ExposureMask);
  XMapWindow(display, window);
  GC gc = XCreateGC(display, window, 0, NULL);
  XSetForeground(display, gc, BlackPixel(display, screen));
  Ball ball = {WIDTH / 2.0, HEIGHT / 2.0, 0, 0};
  Point center = {WIDTH / 2.0, HEIGHT / 2.0};
  double time = 0.0;
  Point vertices[NUM_SIDES];
  Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", True);
  XSetWMProtocols(display, window, &wm_delete, 1);
  while (1) {
    while (XPending(display)) {
      XEvent event;
      XNextEvent(display, &event);
      if (event.type == ClientMessage)
        exit(0);
      if (event.type == KeyPress)
        exit(0);
    }
    time += DT;
    double angle = OMEGA * time;
    get_hex_vertices(vertices, center, angle);
    ball.vy += G * DT;
    ball.x += ball.vx * DT;
    ball.y += ball.vy * DT;
    for (int i = 0; i < NUM_SIDES; i++) {
      Point p1 = vertices[i];
      Point p2 = vertices[(i + 1) % NUM_SIDES];
      resolve_collision(&ball, p1, p2, center, OMEGA);
    }
    XClearWindow(display, window);
    XPoint points[NUM_SIDES + 1];
    for (int i = 0; i < NUM_SIDES; i++) {
      points[i].x = (short)vertices[i].x;
      points[i].y = (short)vertices[i].y;
    }
    points[NUM_SIDES] = points[0];
    XDrawLines(display, window, gc, points, NUM_SIDES + 1, CoordModeOrigin);
    XFillArc(display, window, gc, (int)(ball.x - BALL_RADIUS),
             (int)(ball.y - BALL_RADIUS), (int)(2 * BALL_RADIUS),
             (int)(2 * BALL_RADIUS), 0, 360 * 64);
    XFlush(display);
    usleep((int)(DT * 1000000));
  }
  XCloseDisplay(display);
}