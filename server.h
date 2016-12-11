
#define BACKLOG 15
#define DEBUG 0


typedef struct {
	char req_type[4];
	char req_url[100];
	char req_params[512];
} req_details;

typedef struct request {
	struct request *next;
	int *client_fd;
	char *ip;
} request;

typedef struct accepted_req {
	int client_fd;
	char *ip;
} accepted_req;

typedef struct server_args {
	char *port;
	Display *display;
	Window win;
	GC gc;
	int request_text_x;
	int request_text_y;
	int response_text_x;
	int response_text_y;
	int win_width;
} server_args;


void start_server(server_args *);

void print_request(char *, char *);

void print_response(char *, char *, int);

int open_socket();

void close_socket(int);

void listen_on_socket(int);

accepted_req *accept_client(int);

void send_response(int, int);

char *get_file_data(char *);

char *create_response(char *);

char *get_time();

request *get_request();

void queue_request(pthread_mutex_t);

req_details *parse_request(char *);

void thread_loop(int *);
