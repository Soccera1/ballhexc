#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h> // For usleep

// --- Configuration Constants ---
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define FRAME_RATE 60
#define TIME_STEP (1.0f / FRAME_RATE)

// Physics
#define GRAVITY 250.0f      // A little stronger for a more dynamic feel
#define RESTITUTION 0.85f   // Bounciness (0.0 = no bounce, 1.0 = perfect bounce)
#define FRICTION 0.05f      // How much velocity is lost parallel to a wall

// Geometry
#define HEXAGON_RADIUS 300.0f
#define BALL_RADIUS 20.0f
#define HEXAGON_ROT_SPEED 0.4f // Radians per second

// --- Data Structures ---

// A simple 2D vector for positions, velocities, etc.
typedef struct {
  double x, y;
} Vec2D;

// Represents the state of the ball
typedef struct {
  Vec2D pos;
  Vec2D vel;
  double radius;
} Ball;

// Represents the state of the hexagon
typedef struct {
  Vec2D center;
  double radius;
  double angle; // Current rotation angle in radians
  double angular_velocity;
} Hexagon;

// --- Global Variables ---
static Display *display;
static Window window;
static GC gc;
static Pixmap buffer; // For double-buffering
static int screen;
static Atom wm_delete_window;

// --- Function Prototypes ---
void init_x();
void create_gc();
void setup_window();
void run_event_loop();
void cleanup_x();
void draw_scene(const Ball *ball, const Hexagon *hexagon);
void update_physics(Ball *ball, Hexagon *hexagon);

// --- Main Function ---
int main() {
  init_x();
  setup_window();

  // Initialize simulation objects
  Ball ball = {
    .pos = {WINDOW_WIDTH / 2.0, WINDOW_HEIGHT / 2.0 - 100},
    .vel = {50.0, -50.0}, // Initial velocity
    .radius = BALL_RADIUS
  };

  Hexagon hexagon = {
    .center = {WINDOW_WIDTH / 2.0, WINDOW_HEIGHT / 2.0},
    .radius = HEXAGON_RADIUS,
    .angle = 0.0,
    .angular_velocity = HEXAGON_ROT_SPEED
  };

  run_event_loop(&ball, &hexagon);

  cleanup_x();
  return 0;
}

// --- X11 and Application Logic ---

/**
 * @brief The main loop: handles events, updates physics, and draws the scene.
 */
void run_event_loop(Ball *ball, Hexagon *hexagon) {
  XEvent event;
  int running = 1;

  while (running) {
    // Handle all pending X events
    while (XPending(display)) {
      XNextEvent(display, &event);
      switch (event.type) {
      case Expose:
        // Window needs to be redrawn
        break;
      case KeyPress: {
        KeySym keysym = XLookupKeysym(&event.xkey, 0);
        if (keysym == XK_q || keysym == XK_Escape) {
          running = 0;
        }
        break;
      }
      case ClientMessage:
        // Handle window close button
        if (event.xclient.data.l[0] == wm_delete_window) {
          running = 0;
        }
        break;
      }
    }

    // Update game state
    update_physics(ball, hexagon);

    // Draw the new state
    draw_scene(ball, hexagon);

    // Control frame rate
    usleep(1000000 / FRAME_RATE);
  }
}

/**
 * @brief Updates the position and velocity of objects based on physics.
 */
void update_physics(Ball *ball, Hexagon *hexagon) {
  // 1. Update hexagon rotation
  hexagon->angle += hexagon->angular_velocity * TIME_STEP;
  if (hexagon->angle > 2.0 * M_PI)
    hexagon->angle -= 2.0 * M_PI;

  // 2. Apply gravity to the ball
  ball->vel.y += GRAVITY * TIME_STEP;

  // 3. Update ball position
  ball->pos.x += ball->vel.x * TIME_STEP;
  ball->pos.y += ball->vel.y * TIME_STEP;

  // 4. Collision detection and response with hexagon walls
  for (int i = 0; i < 6; ++i) {
    // Calculate the two vertices of the current edge
    double angle1 = hexagon->angle + i * (M_PI / 3.0);
    double angle2 = hexagon->angle + (i + 1) * (M_PI / 3.0);

    Vec2D v1 = {hexagon->center.x + hexagon->radius * cos(angle1),
                hexagon->center.y + hexagon->radius * sin(angle1)};
    Vec2D v2 = {hexagon->center.x + hexagon->radius * cos(angle2),
                hexagon->center.y + hexagon->radius * sin(angle2)};

    // Get the outward-pointing normal of the edge
    Vec2D edge = {v2.x - v1.x, v2.y - v1.y};
    Vec2D normal = {edge.y, -edge.x};
    double len = sqrt(normal.x * normal.x + normal.y * normal.y);
    normal.x /= len;
    normal.y /= len;

    // Vector from the first vertex of the edge to the ball's center
    Vec2D ball_to_v1 = {ball->pos.x - v1.x, ball->pos.y - v1.y};

    // Project this vector onto the normal to get the distance to the line
    double dist = ball_to_v1.x * normal.x + ball_to_v1.y * normal.y;

    // If the ball is "inside" the wall, a collision has occurred
    if (dist < ball->radius) {
      // a. Correct position to prevent sinking into the wall
      double overlap = ball->radius - dist;
      ball->pos.x += normal.x * overlap;
      ball->pos.y += normal.y * overlap;

      // b. Calculate velocity components
      double v_dot_n = ball->vel.x * normal.x + ball->vel.y * normal.y;
      Vec2D v_normal = {normal.x * v_dot_n, normal.y * v_dot_n};
      Vec2D v_tangent = {ball->vel.x - v_normal.x, ball->vel.y - v_normal.y};

      // c. Apply restitution (bouncing) and friction
      v_normal.x *= -RESTITUTION;
      v_normal.y *= -RESTITUTION;
      v_tangent.x *= (1.0 - FRICTION);
      v_tangent.y *= (1.0 - FRICTION);

      // d. Recombine to get the new velocity
      ball->vel.x = v_normal.x + v_tangent.x;
      ball->vel.y = v_normal.y + v_tangent.y;
    }
  }
}

/**
 * @brief Draws all objects to the screen using a double buffer.
 */
void draw_scene(const Ball *ball, const Hexagon *hexagon) {
  // 1. Clear the back buffer (draw a black rectangle)
  XSetForeground(display, gc, BlackPixel(display, screen));
  XFillRectangle(display, buffer, gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

  // 2. Draw the hexagon
  XPoint points[7];
  for (int i = 0; i < 7; ++i) {
    // The 7th point connects back to the first
    int edge_idx = i % 6;
    double angle = hexagon->angle + edge_idx * (M_PI / 3.0);
    points[i].x = (short)(hexagon->center.x + hexagon->radius * cos(angle));
    points[i].y = (short)(hexagon->center.y + hexagon->radius * sin(angle));
  }
  XSetForeground(display, gc, WhitePixel(display, screen));
  XDrawLines(display, buffer, gc, points, 7, CoordModeOrigin);

  // 3. Draw the ball
  XSetForeground(display, gc, 0xFF4136); // A nice red color
  XFillArc(display, buffer, gc, (int)(ball->pos.x - ball->radius),
           (int)(ball->pos.y - ball->radius), (unsigned int)(ball->radius * 2),
           (unsigned int)(ball->radius * 2), 0, 360 * 64);

  // 4. Copy the back buffer to the window
  XCopyArea(display, buffer, window, gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
            0);
  XFlush(display);
}

// --- X11 Initialization and Cleanup ---

/**
 * @brief Connects to the X server.
 */
void init_x() {
  display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }
  screen = DefaultScreen(display);
}

/**
 * @brief Creates the main window and the double buffer.
 */
void setup_window() {
  window = XCreateSimpleWindow(
      display, RootWindow(display, screen), 10, 10, WINDOW_WIDTH,
      WINDOW_HEIGHT, 1, BlackPixel(display, screen),
      WhitePixel(display, screen));

  XStoreName(display, window, "Hexagon Ball Physics");

  // Create the pixmap for double buffering
  buffer = XCreatePixmap(display, window, WINDOW_WIDTH, WINDOW_HEIGHT,
                         DefaultDepth(display, screen));

  // Select the kinds of events we are interested in
  XSelectInput(display, window, ExposureMask | KeyPressMask);

  // Allow the window manager to send a delete message
  wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wm_delete_window, 1);

  XMapWindow(display, window);
  create_gc();
}

/**
 * @brief Creates the Graphics Context for drawing.
 */
void create_gc() {
  gc = XCreateGC(display, window, 0, NULL);
  XSetBackground(display, gc, WhitePixel(display, screen));
  XSetForeground(display, gc, BlackPixel(display, screen));
  XSetLineAttributes(display, gc, 2, LineSolid, CapRound, JoinRound);
}

/**
 * @brief Cleans up X11 resources.
 */
void cleanup_x() {
  XFreePixmap(display, buffer);
  XFreeGC(display, gc);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
}
