#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Constants
#define WIDTH 800
#define HEIGHT 600
#define GRAVITY 0.1
#define FRICTION 0.9
#define HEXAGON_SIZE 200
#define BALL_SIZE 20

// Structure to represent a point
typedef struct {
    double x, y;
} Point;

// Structure to represent the ball
typedef struct {
    Point position;
    Point velocity;
} Ball;

// Function to create a new X11 window
Display* create_window() {
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open display\n");
        exit(1);
    }

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 100, 100, WIDTH, HEIGHT, 1, BlackPixel(display, screen), WhitePixel(display, screen));

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    return display;
}

// Function to draw a hexagon
void draw_hexagon(Display* display, Window window, GC gc, Point center, double size, double angle) {
    Point vertices[6];
    for (int i = 0; i < 6; i++) {
        double a = angle + i * M_PI / 3;
        vertices[i].x = center.x + size * cos(a);
        vertices[i].y = center.y + size * sin(a);
    }

    XPoint x_vertices[6];
    for (int i = 0; i < 6; i++) {
        x_vertices[i].x = (int)vertices[i].x;
        x_vertices[i].y = (int)vertices[i].y;
    }

    XDrawLines(display, window, gc, x_vertices, 6, CoordModeOrigin);
}

// Function to update the ball's position and velocity
void update_ball(Ball* ball) {
    ball->velocity.y += GRAVITY;
    ball->position.x += ball->velocity.x;
    ball->position.y += ball->velocity.y;
}

// Function to check collision with the hexagon walls
void check_collision(Ball* ball, Point center, double size, double angle) {
    for (int i = 0; i < 6; i++) {
        double a = angle + i * M_PI / 3;
        Point vertex1 = {center.x + size * cos(a), center.y + size * sin(a)};
        double a_next = angle + (i + 1) % 6 * M_PI / 3;
        Point vertex2 = {center.x + size * cos(a_next), center.y + size * sin(a_next)};

        // Calculate the normal vector of the wall
        Point normal = {vertex2.y - vertex1.y, vertex1.x - vertex2.x};
        double length = sqrt(normal.x * normal.x + normal.y * normal.y);
        normal.x /= length;
        normal.y /= length;

        // Check if the ball is colliding with the wall
        double distance = (ball->position.x - vertex1.x) * normal.x + (ball->position.y - vertex1.y) * normal.y;
        if (distance < BALL_SIZE / 2) {
            // Calculate the reflection vector
            double dot_product = ball->velocity.x * normal.x + ball->velocity.y * normal.y;
            ball->velocity.x -= 2 * dot_product * normal.x * FRICTION;
            ball->velocity.y -= 2 * dot_product * normal.y * FRICTION;

            // Move the ball to the correct position after collision
            ball->position.x -= distance * normal.x;
            ball->position.y -= distance * normal.y;
        }
    }
}

int main() {
    Display* display = create_window();
    int screen = DefaultScreen(display);
    Window window = RootWindow(display, screen);
    GC gc = DefaultGC(display, screen);

    // Initialize the ball
    Ball ball;
    ball.position.x = WIDTH / 2;
    ball.position.y = HEIGHT / 2;
    ball.velocity.x = 2;
    ball.velocity.y = -5;

    // Initialize the hexagon
    Point center = {WIDTH / 2, HEIGHT / 2};
    double size = HEXAGON_SIZE;
    double angle = 0;

    while (1) {
        // Handle events
        XEvent event;
        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                return 0;
            }
        }

        // Update the ball's position and velocity
        update_ball(&ball);

        // Check collision with the hexagon walls
        check_collision(&ball, center, size, angle);

        // Update the hexagon's angle
        angle += 0.01;

        // Clear the window
        XClearWindow(display, window);

        // Draw the hexagon
        draw_hexagon(display, window, gc, center, size, angle);

        // Draw the ball
        XFillArc(display, window, gc, (int)(ball.position.x - BALL_SIZE / 2), (int)(ball.position.y - BALL_SIZE / 2), BALL_SIZE, BALL_SIZE, 0, 360 * 64);

        // Flush the display
        XFlush(display);

        // Cap the frame rate
        usleep(16000); // 60 FPS
    }

    return 0;
}
