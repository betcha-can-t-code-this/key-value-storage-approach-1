#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
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

static void signal_handler_callback(int signum)
{
	storage_destroy();
	_exit(0);
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

static void read_and_process(int fd, pid_t cpid)
{
	char buf[MAX_BUFLEN];
	int counter = 0;

	while (1) {
		bzero(buf, MAX_BUFLEN * sizeof(char));

		if (recv(fd, buf, MAX_BUFLEN, 0) <= 0) {
			shutdown(fd, SHUT_RDWR);
			close(fd);
			kill(cpid, SIGKILL);
			break;
		}

		char *token = strtok(buf, " ");
		char response[1024];

		// processing 'GET' command.
		if (token != NULL && !strcmp(token, "GET")) {
			char *key = strtok(NULL, " ");

			if (key == NULL) {
				send(fd, "Wrong number of arguments required for 'GET' command.\n", 54, 0);
				syslog(LOG_SYSLOG | LOG_ERR, "Wrong number of arguments required for 'GET' command.\n");
				continue;
			}

			key[strlen(key) - 1] = key[strlen(key) - 1] == '\n'
				? '\0'
				: key[strlen(key) - 1];

			bzero(response, 1024 * sizeof(char));

			storage_t *tmp = do_get(key);

			if (tmp == NULL) {
				send(fd, "$-1\n", 4, 0);
				syslog(LOG_SYSLOG | LOG_ERR, "Data with key '%s' not found.\n", key);
				continue;
			}

			sprintf(response, "$%d: %s\n", counter++, tmp->value);
			send(fd, response, strlen(response), 0);
			continue;
		}

		// processing 'SET' command.
		if (token != NULL && !strcmp(token, "SET")) {
			char *key = strtok(NULL, " ");
			char *value = strtok(NULL, " ");

			if (key == NULL || value == NULL) {
				send(fd, "Wrong number of arguments required for 'SET' command.\n", 54, 0);
				syslog(LOG_SYSLOG | LOG_ERR, "Wrong number of arguments required for 'SET' command.\n");
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

		// processing 'UPDATE' command.
		if (token != NULL && !strcmp(token, "UPDATE")) {
			char *key = strtok(NULL, " ");
			char *value = strtok(NULL, " ");

			if (key == NULL && value == NULL) {
				send(fd, "Wrong number of arguments required for 'UPDATE' command.\n", 57, 0);
				syslog(LOG_SYSLOG | LOG_ERR, "Wrong number of arguments required for 'UPDATE command.\n");
				continue;
			}

			key[strlen(key) - 1] = key[strlen(key) - 1] == '\n'
				? '\0'
				: key[strlen(key) - 1];
			value[strlen(value) - 1] = value[strlen(value) - 1] == '\n'
				? '\0'
				: value[strlen(value) - 1];

			do_update(key, value);
			send(fd, "+OK\n", 4, 0);
			continue;
		}

		// processing 'DELETE' command.
		if (token != NULL && !strcmp(token, "DELETE")) {
			char *key = strtok(NULL, " ");

			if (key == NULL) {
				send(fd, "Wrong number of arguments required for 'DELETE' command.\n", 57, 0);
				syslog(LOG_SYSLOG | LOG_ERR, "Wrong number of arguments required for 'DELETE' command.\n");
				continue;
			}

			key[strlen(key) - 1] = key[strlen(key) - 1] == '\n'
				? '\0'
				: key[strlen(key) - 1];

			do_delete(key);
			send(fd, "+OK\n", 4, 0);
			continue;
		}

		bzero(response, 1024 * sizeof(char));
		sprintf(response, "Unknown command '%s'\n", token);
		send(fd, response, strlen(response), 0);
		syslog(LOG_SYSLOG | LOG_ERR, "%s", response);
	}
}

static void do_loop(int fd)
{
	int acc;
	pid_t pid;

	while (1) {
		if (listen(fd, 0) < 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "listen() fail.\n");
			break;
		}

		syslog(LOG_SYSLOG | LOG_INFO, "Listening on host: localhost, port:1337.\n");

		if ((acc = accept(fd, NULL, NULL)) < 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "accept() fail.\n");
			break;
		}

		syslog(LOG_SYSLOG | LOG_INFO, "Incoming connection accepted.\n");

		pid = fork();

		if (pid == 0) {
			read_and_process(acc, getpid());
		} else if (pid < 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "fork() failed.\n");
		} else {
			// do some awesome shit here.. :)
			signal(SIGCHLD, SIG_IGN);
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
		syslog(LOG_SYSLOG | LOG_ERR, "socket() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint initialized.\n");

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt)) < 0) {
		syslog(LOG_SYSLOG | LOG_ERR, "setsockopt() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint set to SO_REUSEADDR.\n");

	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &sock_opt, sizeof(sock_opt)) < 0) {
		syslog(LOG_SYSLOG | LOG_ERR, "setsockopt() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint set to TCP_NODELAY.\n");

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &sock_opt, sizeof(sock_opt)) < 0) {
		syslog(LOG_SYSLOG | LOG_ERR, "setsockopt() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint set to SO_KEEPALIVE.\n");

	bzero(&sd, sizeof(struct sockaddr_in));

	sd.sin_family = AF_INET;
	sd.sin_port = htons(1337);
	sd.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&sd, (socklen_t)(sizeof(struct sockaddr))) < 0) {
		syslog(LOG_SYSLOG | LOG_ERR, "bind() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint binded to localhost:1337.\n");
	do_loop(sock);
	close(sock);

	return 0;
}
