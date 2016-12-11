#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <netdb.h>
#include <wait.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <pthread.h>
#include <time.h>
#include "server.h"

static void sig_int_handler(int);
static int server_socket_fd = 0;
static pthread_mutex_t mutex;
static int NUM_THREADS = 8;
const char *SERVER_PORT;
Display *display;
Window win;
GC gc;

int request_text_x;
int request_text_y;
int response_text_x;
int response_text_y;
int request_text_y_start;
int response_text_y_start;
int win_width;
int TEXT_VERT_SPACE = 15;
int NUM_ITEMS = 25;

int req_count = 0;
int rsp_count = 0;

int *thread_ids;
pthread_t *threads;

request *request_start;
request *last_request;
int num_requests = 0;

void start_server(server_args *args) {
		SERVER_PORT = args->port;
		display = args->display;
		win = args->win;
		gc = args->gc;
		request_text_x = args->request_text_x;
		request_text_y = 0;
		request_text_y_start = args->request_text_y;
		response_text_x = args->response_text_x;
		response_text_y = 0;
		response_text_y_start = args->response_text_y;
		win_width = args->win_width;

		// creates dummy request start item in linked list
		request_start = malloc(sizeof(request));

		if (signal(SIGINT, sig_int_handler) == SIG_ERR) {
			perror("ERROR: Unable to create signal interupt handler");
			exit(-1);
		}

		server_socket_fd = open_socket();

		// creates the thread pool to listen for requests
		thread_ids = malloc(sizeof(int) * NUM_THREADS);
		threads = malloc(sizeof(pthread_t) * NUM_THREADS);
		int i;
		for (i = 0; i < NUM_THREADS; i++) {
			thread_ids[i] = i;
			pthread_create(&threads[i], NULL, (void *) &thread_loop, &thread_ids[i]);
		}

		// then begins listening for requests
		listen_on_socket(server_socket_fd);

		for (i = 0; i < NUM_THREADS; i++) {
			pthread_join(threads[i], NULL);
		}
		
		close_socket(server_socket_fd);

		free(thread_ids);
		free(threads);
}

void print_request(char *ip, char *time) {
	char str[200];
	sprintf(str, "%d. From %s handled at %s", req_count+1, ip, time);

	XClearArea(display, win,
		(request_text_x - 5), (request_text_y + request_text_y_start - TEXT_VERT_SPACE)+1,
		(request_text_x + win_width/2 - request_text_x*2), (TEXT_VERT_SPACE+1)*2, False);

	XDrawString(display, win, gc, request_text_x, request_text_y + request_text_y_start,
		str, strlen(str));

	XFlush(display);

	request_text_y = ((request_text_y + TEXT_VERT_SPACE + 1));

	if (request_text_y >= (TEXT_VERT_SPACE * NUM_ITEMS)) {
		request_text_y = 0;
	}

	req_count++;
}

void print_response(char *ip, char *time, int thread_id) {
	char str[200];
	sprintf(str, "%d. To %s handled at %s by thread %d", rsp_count+1, ip, time, thread_id+1);

	XClearArea(display, win,
		(response_text_x - 5), (response_text_y + response_text_y_start - TEXT_VERT_SPACE)+1,
		(response_text_x + win_width/2 - response_text_x), (TEXT_VERT_SPACE+1)*2, False);

	XDrawString(display, win, gc, response_text_x, response_text_y + response_text_y_start,
		str, strlen(str));

	XFlush(display);

	response_text_y = ((response_text_y + TEXT_VERT_SPACE + 1));

	if (response_text_y >= (TEXT_VERT_SPACE * NUM_ITEMS)) {
		response_text_y = 0;
	}

	rsp_count++;
}

void thread_loop(int *thread_id) {
	request *req;

	printf("Thread %d initialized...\n", *thread_id + 1);

	while (1) {

		if (num_requests > 0) {
			pthread_mutex_lock(&mutex);

			req = get_request();
			if (req) {
				send_response(server_socket_fd, *req->client_fd);
				close_socket(*req->client_fd);
				free(req);
				print_response(req->ip, get_time(), *thread_id);
			}

			pthread_mutex_unlock(&mutex);
		}

	}

}

void listen_on_socket(int socket_fd) {
	if (DEBUG)	printf("Listening...\n");
	int listen_status;
	while ((listen_status = listen(socket_fd, BACKLOG)) == 0) {
		queue_request(mutex);
	}
	if (listen_status == -1) {
		printf("ERROR while listening on PORT %s: %s\n", SERVER_PORT, strerror(errno));
		exit(1);
	}
}

void queue_request(pthread_mutex_t mutex) {
	accepted_req *res = accept_client(server_socket_fd);
	int client_fd = res->client_fd;
	char *ip = malloc(sizeof(char) * 100);
	strcpy(ip, res->ip);
	free(res);

	if (DEBUG)	printf("Client accepted...\n");

	request *new_req = malloc(sizeof(request));
	new_req->client_fd = &client_fd;
	new_req->ip = ip;

	/* ENTERING CRITICAL ZONE */
	pthread_mutex_lock(&mutex);
	if (DEBUG)	printf("Queuing request, locked mutex...\n");

	if (num_requests == 0) 
		request_start->next = new_req;
	else
		last_request->next = new_req;

	last_request = new_req;

	num_requests++;

	pthread_mutex_unlock(&mutex);
	/* EXITED CRITICAL ZONE */

	if (DEBUG)	printf("Unlocked mutex...\n");

	print_request(ip, get_time());
}

char *get_time() {
	time_t rawtime;
	struct tm *timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	char time[50];
	strftime(time, sizeof(time), "%I:%M %p", timeinfo);

	return strdup(time);
}

request *get_request() {
	request *req;

	// if there are requests, grabs the first one
	if (num_requests > 0) {
		req = request_start->next;
		request_start->next = req->next;
		if (request_start->next == NULL) {
			last_request = NULL;
		}
		num_requests--;
		if (DEBUG)	printf("Got request...\n");
	}
	
	return req;
}

accepted_req *accept_client(int socket_fd) {
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);

	int client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &addr_size);
	if (client_fd == -1) {
		printf("ERROR while accepting client on PORT %s: %s\n", SERVER_PORT, strerror(errno));
		exit(1);
	}

	accepted_req *ret_val = malloc(sizeof(accepted_req));
	ret_val->client_fd = client_fd;

	if (client_addr.ss_family == AF_INET) {
		char buf[50], ip4[50];
		inet_ntop(AF_INET, &((struct sockaddr_in *) &client_addr)->sin_addr, buf, 50);
		strcpy(ip4, buf);
		ret_val->ip = ip4;

	} else {
		char buf[100], ip6[100];
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *) &client_addr)->sin6_addr, buf, 100);
		strcpy(ip6, buf);
		ret_val->ip = ip6;
	}

	return ret_val;
}

void send_response(int server_fd, int client_fd) {
	char request_buffer[512];
	bzero(request_buffer, 512);

	// first reads in the request
	int bytes_read = recv(client_fd, (void *) request_buffer, 512, 0);
	if (bytes_read == -1) {
		printf("ERROR while receiving client request on PORT %s: %s\n", SERVER_PORT, strerror(errno));
		exit(1);
	}

	// parses the request from the client
	req_details *req = parse_request(request_buffer);

	// obtains the data from the specified file and creates the response
	char *file_data = get_file_data(req->req_url);
	char *response = create_response(file_data);

	// sends the data to the client
	int len = strlen(response);
	int bytes_sent = 0;

	while (bytes_sent < len) {
		bytes_sent = send(client_fd, response, len, 0);
	}

	free(req);

	if (DEBUG)	printf("Response sent...\n");
}

char *get_file_data(char *file_path) {
	if (strcmp(file_path, "") == 0)
		file_path = "index.html";

	FILE *file = fopen(file_path, "r");
	
	// obtains the file's length
	fseek(file, 0L, SEEK_END);
	int size = ftell(file);
	rewind(file);

	// creates the string that will contain the data to return
	char *file_data = malloc(size + 1);

	// reads the file and terminates the string
	fread(file_data, size, 1, file);
	file_data[size] = 0;

	fclose(file);
	return file_data;
}

char *create_response(char *file_data) {
	char header[512];
	sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", (int) strlen(file_data));
	int size = strlen(header) + strlen(file_data);

	char *response = malloc(size);
	sprintf(response, "%s%s", header, file_data);

	bzero(file_data, strlen(file_data));
	free(file_data);
	return response;

}

req_details *parse_request(char *req_data) {
	req_details *req = malloc(sizeof(req_details));
	int i = 0;

	// first obtains the type of the request
	char type[4];
	memset(type, 0, 4);

	while (req_data[i] != ' ') {
		type[i] = req_data[i];
		i++;
	}
	strcpy(req->req_type, type);

	i++;

	// then obtains the url
	int j = 0;
	char url[100];
	memset(url, 0, 100);
	i++;

	while(req_data[i] != ' ' && req_data[i] != '?') {
		url[j] = req_data[i];
		i++;
		j++;
	}
	strcpy(req->req_url, url);

	// finally obtains the parameters based on which type of request it is
	j = 0;
	char params[512];
	memset(params, 0, 512);

	if (strcmp(type, "GET") == 0) {
		// only continues if there are params
		if (req_data[i] != ' ' && req_data[i] == '?') {
			i++;

			while (req_data[i] != ' ') {
				params[j] = req_data[i];
				i++;
				j++;
			}
		}

	} else {
		// must first get to the content
		int found = 0;
		while (!found) {
			i++;
			if (req_data[i-3] == '\r' && req_data[i-2] == '\n' &&
			    req_data[i-1] == '\r' && req_data[i] == '\n') {
			    found = 1;
			}
		}
		i++;

		// now i points to the first character of the parameters
		while (req_data[i] != '\r' && req_data[i] != '\n') {
			params[j] = req_data[i];
			i++;
			j++;
		}

	}
	strcpy(req->req_params, params);

	return req;
}

/**
 * Opens a new socket to run the server.
 * Returns the file descriptor for the new socket.
 */
int open_socket() {
	int status;
	int socket_fd;

	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	// gets the address information needed for the socket
	if ((status = getaddrinfo(NULL, SERVER_PORT, &hints, &server_info)) != 0) {
		printf("ERROR in getaddrinfo: %s\n", gai_strerror(status));
		exit(1);
	}

	// creates the socket using the information received from getaddrinfo()
	socket_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
	if (socket_fd == -1) {
		printf("ERROR while creating socket: %s\n", strerror(errno));
		exit(1);
	}

	// binds the socket to the port that was passed to getaddrinfo()
	int bind_status = bind(socket_fd, server_info->ai_addr, server_info->ai_addrlen);
	if (bind_status == -1) {
		printf("ERROR while binding socket to PORT %s: %s\n", SERVER_PORT, strerror(errno));
		exit(1);
	}

	int yes = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	freeaddrinfo(server_info);
	return socket_fd;
}

void close_socket(int socket_fd) {
	close(socket_fd);
}

static void sig_int_handler(int sig) {
	printf("\nClosing server...\n");
	close_socket(server_socket_fd);
	int i;
	for (i = 0; i < NUM_THREADS; i++) {
		pthread_kill(threads[i], 0);
	}
	exit(1);
}
