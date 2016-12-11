#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <pthread.h>
#include "server.h"

void init_display();
void draw_screen();

pthread_t *server_thread;

Display *display;
Window win;
GC gc;
int screen;
XEvent event;

int win_x = 0, win_y = 0, win_width = 800, win_height = 500;

int main(int argc, char *argv[]) {
	if (argc == 2) {
		char *port = argv[1];
		init_display();

		server_thread = malloc(sizeof(pthread_t *));
		server_args *args = malloc(sizeof(server_args *));
		args->port = port;
		args->display = display;
		args->win = win;
		args->gc = gc;


		args->request_text_x = win_x + 20;
		args->request_text_y = win_y + 50;
		args->response_text_x = win_width/2 + 20;
		args->response_text_y = win_y + 50;
		args->win_width = win_width;

		pthread_create(server_thread, NULL, (void *) &start_server, args);

		while (1) {
			XNextEvent(display, &event);

			if (event.type == Expose && event.xexpose.count == 0) {
				draw_screen();
			}
		}

	} else {
		printf("Invalid number of arguments...\n");
	}

}

void init_display() {
	unsigned long black, white;
	display = XOpenDisplay(NULL);

	if (display == NULL) {
		printf("Error creating display window...\n");
		exit(0);
	}

	screen = DefaultScreen(display);

	black = BlackPixel(display, screen);
	white = WhitePixel(display, screen);

	win = XCreateSimpleWindow(display, RootWindow(display, screen),
		win_x, win_y, win_width, win_height, 2, black, white);


	XSetStandardProperties(display, win, "Server Status", "", None, NULL, 0, NULL);
	XSelectInput(display, win, ExposureMask | ButtonPressMask | KeyPressMask);

	gc = XCreateGC(display, win, 0, 0);

	XSetBackground(display, gc, white);
	XSetForeground(display, gc, black);

	XClearWindow(display, win);
	XMapRaised(display, win);

	XMapWindow(display, win);
	XSync(display, False);

}

void draw_screen() {
	int x_center = win_width/2;
	int y_center = win_width/2;

	XDrawLine(display, win, gc, x_center, win_y, y_center, win_height);
	XDrawLine(display, win, gc, win_x, win_y + 30, win_width, win_y + 30);
	XDrawString(display, win, gc, x_center/2 - 24, 20, "Requests", 8);
	XDrawString(display, win, gc, x_center + x_center/2 - 27, win_y + 20, "Responses", 9);

}