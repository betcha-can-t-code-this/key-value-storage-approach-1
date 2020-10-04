#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "./lib/command.h"
#include "./lib/storage.h"

#define MAX_BUFLEN	1024
#define UNUSED(x)	((void)(x))
#define MAX_PROC	5

static void signal_handler_callback(int signum)
{
	storage_destroy();
}

static void register_signal_handler(void)
{
	struct sigaction sa;

	sa.sa_flags = SA_RESTART;
	sa.sa_handler = signal_handler_callback;

	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);

	return;
}

static void read_and_process(int fd)
{
	char buf[MAX_BUFLEN];
	int counter = 0;

	while (1) {
		bzero(buf, MAX_BUFLEN * sizeof(char));

		if (recv(fd, buf, MAX_BUFLEN, 0) < 0) {
			fprintf(stderr, "read() fail.\n");
			break;
		}

		char *token = strtok(buf, " ");
		char response[1024];

		if (token != NULL && !strcmp(token, "GET")) {
			char *key = strtok(NULL, " ");

			if (key == NULL) {
				send(fd, "Wrong number of arguments required for 'GET' command.\n", 54, 0);
				continue;
			}

			key[strlen(key) - 1] = key[strlen(key) - 1] == '\n'
				? '\0'
				: key[strlen(key) - 1];

			bzero(response, 1024 * sizeof(char));

			storage_t *tmp = do_get(key);

			if (tmp == NULL) {
				send(fd, "$-1\n", 4, 0);
				continue;
			}

			sprintf(response, "$%d: %s\n", counter++, tmp->value);
			send(fd, response, strlen(response), 0);
			continue;
		}

		if (token != NULL && !strcmp(token, "SET")) {
			char *key = strtok(NULL, " ");
			char *value = strtok(NULL, " ");

			if (key == NULL || value == NULL) {
				send(fd, "Wrong number of arguments required for 'SET' command.\n", 54, 0);
				continue;
			}

			key[strlen(key) - 1] = key[strlen(key) - 1] == '\n'
				? '\0'
				: key[strlen(key) - 1];
			value[strlen(value) - 1] = value[strlen(value) - 1] == '\n'
				? '\0'
				: value[strlen(value) - 1];

			do_set(key, value);
			send(fd, "+OK\n", 4, 0);
			continue;
		}

		bzero(response, 1024 * sizeof(char));
		sprintf(response, "Unknown command '%s'\n", token);
		send(fd, response, strlen(response), 0);
	}
}

static void do_loop(int fd)
{
	int acc;
	pid_t pid;

	while (1) {
		if (listen(fd, 0) < 0) {
			fprintf(stderr, "listen() fail.\n");
			break;
		}

		if ((acc = accept(fd, NULL, NULL)) < 0) {
			fprintf(stderr, "accept() fail.\n");
			break;
		}

		pid = fork();

		if (pid == 0) {
			read_and_process(acc);
		} else if (pid < 0) {
			fprintf(stderr, "fork() fail.\n");
			break;
		} else {
			// ...
		}
	}
}

int main(int argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);

	register_signal_handler();

	int sock;
	int sock_opt = 1;
	int i;
	struct sockaddr_in sd;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 1) {
		fprintf(stderr, "socket() fail.\n");
		return 1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt)) < 0) {
		fprintf(stderr, "setsockopt() fail.\n");
		return 1;
	}

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &sock_opt, sizeof(sock_opt)) < 0) {
		fprintf(stderr, "setsockopt() fail.\n");
		return 1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &sock_opt, sizeof(sock_opt)) < 0) {
		fprintf(stderr, "setsockopt() fail.\n");
		return 1;
	}

	bzero(&sd, sizeof(struct sockaddr_in));

	sd.sin_family = AF_INET;
	sd.sin_port = htons(1337);
	sd.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&sd, (socklen_t)(sizeof(struct sockaddr))) < 0) {
		fprintf(stderr, "bind() fail.\n");
		return 1;
	}

	do_loop(sock);

	close(sock);
	return 0;
}
