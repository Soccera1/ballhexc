#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define BALL_RADIUS 15
#define HEXAGON_RADIUS 200
#define GRAVITY 500.0
#define FRICTION 0.85
#define BOUNCE_DAMPING 0.8
#define ROTATION_SPEED 0.5

typedef struct {
    double x, y;
} Point;

typedef struct {
    Point pos;
    Point vel;
    double radius;
    unsigned long color;
} Ball;

typedef struct {
    Point center;
    double radius;
    double angle;
    Point vertices[6];
    unsigned long color;
} Hexagon;

typedef struct {
    Display *display;
    Window window;
    GC gc;
    int screen;
    unsigned long black, white, red, blue;
} Graphics;

// Get current time in seconds
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Vector operations
Point point_add(Point a, Point b) {
    return (Point){a.x + b.x, a.y + b.y};
}

Point point_sub(Point a, Point b) {
    return (Point){a.x - b.x, a.y - b.y};
}

Point point_mul(Point a, double scalar) {
    return (Point){a.x * scalar, a.y * scalar};
}

double point_dot(Point a, Point b) {
    return a.x * b.x + a.y * b.y;
}

double point_length(Point a) {
    return sqrt(a.x * a.x + a.y * a.y);
}

Point point_normalize(Point a) {
    double len = point_length(a);
    if (len == 0) return (Point){0, 0};
    return (Point){a.x / len, a.y / len};
}

// Rotate point around origin
Point rotate_point(Point p, double angle) {
    double cos_a = cos(angle);
    double sin_a = sin(angle);
    return (Point){
        p.x * cos_a - p.y * sin_a,
        p.x * sin_a + p.y * cos_a
    };
}

// Initialize graphics
int init_graphics(Graphics *gfx) {
    gfx->display = XOpenDisplay(NULL);
    if (!gfx->display) {
        fprintf(stderr, "Cannot open display\n");
        return 0;
    }
    
    gfx->screen = DefaultScreen(gfx->display);
    gfx->black = BlackPixel(gfx->display, gfx->screen);
    gfx->white = WhitePixel(gfx->display, gfx->screen);
    
    // Create colors
    Colormap colormap = DefaultColormap(gfx->display, gfx->screen);
    XColor red_color, blue_color;
    XAllocNamedColor(gfx->display, colormap, "red", &red_color, &red_color);
    XAllocNamedColor(gfx->display, colormap, "blue", &blue_color, &blue_color);
    gfx->red = red_color.pixel;
    gfx->blue = blue_color.pixel;
    
    gfx->window = XCreateSimpleWindow(
        gfx->display,
        RootWindow(gfx->display, gfx->screen),
        100, 100, WINDOW_WIDTH, WINDOW_HEIGHT,
        1, gfx->black, gfx->white
    );
    
    XSelectInput(gfx->display, gfx->window, 
                 ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(gfx->display, gfx->window);
    
    gfx->gc = XCreateGC(gfx->display, gfx->window, 0, NULL);
    
    return 1;
}

// Update hexagon vertices based on rotation
void update_hexagon(Hexagon *hex) {
    for (int i = 0; i < 6; i++) {
        double vertex_angle = hex->angle + i * M_PI / 3.0;
        hex->vertices[i].x = hex->center.x + hex->radius * cos(vertex_angle);
        hex->vertices[i].y = hex->center.y + hex->radius * sin(vertex_angle);
    }
}

// Check collision between ball and hexagon wall
int check_collision(Ball *ball, Hexagon *hex, Point *collision_point, 
                   Point *normal) {
    for (int i = 0; i < 6; i++) {
        Point p1 = hex->vertices[i];
        Point p2 = hex->vertices[(i + 1) % 6];
        
        // Line segment from p1 to p2
        Point edge = point_sub(p2, p1);
        Point to_ball = point_sub(ball->pos, p1);
        
        // Project ball position onto edge
        double edge_length_sq = point_dot(edge, edge);
        if (edge_length_sq == 0) continue;
        
        double t = point_dot(to_ball, edge) / edge_length_sq;
        t = fmax(0.0, fmin(1.0, t)); // Clamp to segment
        
        Point closest_point = point_add(p1, point_mul(edge, t));
        Point to_closest = point_sub(ball->pos, closest_point);
        double distance = point_length(to_closest);
        
        if (distance < ball->radius) {
            *collision_point = closest_point;
            *normal = point_normalize(to_closest);
            return 1;
        }
    }
    return 0;
}

// Handle ball collision with hexagon wall
void handle_collision(Ball *ball, Point collision_point, Point normal) {
    // Move ball out of collision
    Point correction = point_mul(normal, ball->radius);
    ball->pos = point_add(collision_point, correction);
    
    // Calculate reflection
    double dot_product = point_dot(ball->vel, normal);
    Point reflection = point_sub(ball->vel, point_mul(normal, 2.0 * dot_product));
    
    // Apply bouncing with damping and friction
    ball->vel = point_mul(reflection, BOUNCE_DAMPING);
    
    // Apply friction (reduce velocity perpendicular to normal)
    Point tangent = (Point){-normal.y, normal.x};
    double tangent_velocity = point_dot(ball->vel, tangent);
    Point friction_force = point_mul(tangent, -tangent_velocity * (1.0 - FRICTION));
    ball->vel = point_add(ball->vel, friction_force);
}

// Update ball physics
void update_ball(Ball *ball, Hexagon *hex, double dt) {
    // Apply gravity
    ball->vel.y += GRAVITY * dt;
    
    // Update position
    ball->pos = point_add(ball->pos, point_mul(ball->vel, dt));
    
    // Check collision with hexagon
    Point collision_point, normal;
    if (check_collision(ball, hex, &collision_point, &normal)) {
        handle_collision(ball, collision_point, normal);
    }
    
    // Keep ball roughly in window bounds (backup constraint)
    if (ball->pos.x < ball->radius) {
        ball->pos.x = ball->radius;
        ball->vel.x = -ball->vel.x * BOUNCE_DAMPING;
    }
    if (ball->pos.x > WINDOW_WIDTH - ball->radius) {
        ball->pos.x = WINDOW_WIDTH - ball->radius;
        ball->vel.x = -ball->vel.x * BOUNCE_DAMPING;
    }
    if (ball->pos.y < ball->radius) {
        ball->pos.y = ball->radius;
        ball->vel.y = -ball->vel.y * BOUNCE_DAMPING;
    }
    if (ball->pos.y > WINDOW_HEIGHT - ball->radius) {
        ball->pos.y = WINDOW_HEIGHT - ball->radius;
        ball->vel.y = -ball->vel.y * BOUNCE_DAMPING;
    }
}

// Draw hexagon
void draw_hexagon(Graphics *gfx, Hexagon *hex) {
    XPoint points[7];
    for (int i = 0; i < 6; i++) {
        points[i].x = (int)hex->vertices[i].x;
        points[i].y = (int)hex->vertices[i].y;
    }
    points[6] = points[0]; // Close the polygon
    
    XSetForeground(gfx->display, gfx->gc, hex->color);
    XDrawLines(gfx->display, gfx->window, gfx->gc, points, 7, CoordModeOrigin);
}

// Draw ball
void draw_ball(Graphics *gfx, Ball *ball) {
    XSetForeground(gfx->display, gfx->gc, ball->color);
    XFillArc(gfx->display, gfx->window, gfx->gc,
             (int)(ball->pos.x - ball->radius),
             (int)(ball->pos.y - ball->radius),
             (int)(ball->radius * 2),
             (int)(ball->radius * 2),
             0, 360 * 64);
}

// Clear screen
void clear_screen(Graphics *gfx) {
    XSetForeground(gfx->display, gfx->gc, gfx->white);
    XFillRectangle(gfx->display, gfx->window, gfx->gc, 
                   0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

int main() {
    Graphics gfx;
    if (!init_graphics(&gfx)) {
        return 1;
    }
    
    // Initialize ball
    Ball ball = {
        .pos = {WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 50},
        .vel = {100, 0},
        .radius = BALL_RADIUS,
        .color = gfx.red
    };
    
    // Initialize hexagon
    Hexagon hexagon = {
        .center = {WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2},
        .radius = HEXAGON_RADIUS,
        .angle = 0,
        .color = gfx.blue
    };
    
    double last_time = get_time();
    int running = 1;
    
    while (running) {
        // Handle events
        while (XPending(gfx.display)) {
            XEvent event;
            XNextEvent(gfx.display, &event);
            
            switch (event.type) {
                case KeyPress: {
                    KeySym key = XLookupKeysym(&event.xkey, 0);
                    if (key == XK_q || key == XK_Escape) {
                        running = 0;
                    }
                    break;
                }
                case Expose:
                    break;
            }
        }
        
        // Calculate delta time
        double current_time = get_time();
        double dt = current_time - last_time;
        last_time = current_time;
        
        // Limit delta time to prevent instability
        if (dt > 0.016) dt = 0.016;
        
        // Update hexagon rotation
        hexagon.angle += ROTATION_SPEED * dt;
        update_hexagon(&hexagon);
        
        // Update ball physics
        update_ball(&ball, &hexagon, dt);
        
        // Render
        clear_screen(&gfx);
        draw_hexagon(&gfx, &hexagon);
        draw_ball(&gfx, &ball);
        
        XFlush(gfx.display);
        usleep(16000); // ~60 FPS
    }
    
    XCloseDisplay(gfx.display);
    return 0;
}
