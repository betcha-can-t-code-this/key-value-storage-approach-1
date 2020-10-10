#define _GNU_SOURCE
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct thread_info {
	pthread_t thread_id;
	int pfd;
	int afd;
} thread_info_t;

static thread_info_t thread_info = {
	.thread_id = 0,
	.pfd = 0,
	.afd = 0
};

static void signal_handler_callback(int signum)
{
	storage_destroy();
	close(thread_info.pfd);
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

static void *read_and_process(void *targ)
{
	thread_info_t *tinfo = (thread_info_t *)targ;
	char buf[MAX_BUFLEN];
	int counter = 0;

	while (1) {
		bzero(buf, MAX_BUFLEN * sizeof(char));

		if (recv(tinfo->afd, buf, MAX_BUFLEN, 0) <= 0) {
			shutdown(tinfo->afd, SHUT_RDWR);
			close(tinfo->afd);
			pthread_exit(NULL);
			break;
		}

		buf[strlen(buf) - 1] = '\0';

		char *token = strtok(buf, " ");
		char *errmsg;
		char response[1024];

		// processing 'GET' command.
		if (token != NULL && !strcmp(token, "GET")) {
			char *key = strtok(NULL, " ");

			if (key == NULL) {
				errmsg = strdup("-ERR Wrong number of arguments required for 'GET' command.\n");
				send(tinfo->afd, errmsg, strlen(errmsg), 0);
				syslog(LOG_SYSLOG | LOG_ERR, "%s", errmsg);
				free(errmsg);
				continue;
			}

			storage_t *tmp = do_get(key);

			if (tmp == NULL) {
				send(tinfo->afd, "$-1\n", 4, 0);
				syslog(LOG_SYSLOG | LOG_ERR, "Data with key '%s' not found.\n", key);
				continue;
			}

			bzero(response, MAX_BUFLEN);
			sprintf(response, "$%ld: %s\n", strlen(tmp->value), tmp->value);
			send(tinfo->afd, response, strlen(response), 0);
			continue;
		}

		// processing 'SET' command.
		if (token != NULL && !strcmp(token, "SET")) {
			char *key = strtok(NULL, " ");
			char *value = strtok(NULL, " ");

			if (key == NULL || value == NULL) {
				errmsg = strdup("-ERR wrong number of arguments required for 'SET' command.\n");
				send(tinfo->afd, errmsg, strlen(errmsg), 0);
				syslog(LOG_SYSLOG | LOG_ERR, "%s", errmsg);
				free(errmsg);
				continue;
			}

			do_set(key, value);
			send(tinfo->afd, "+OK\n", 4, 0);
			continue;
		}

		// processing 'UPDATE' command.
		if (token != NULL && !strcmp(token, "UPDATE")) {
			char *key = strtok(NULL, " ");
			char *value = strtok(NULL, " ");

			if (key == NULL && value == NULL) {
				errmsg = strdup("-ERR wrong number of arguments required for 'UPDATE' command.\n");
				send(tinfo->afd, errmsg, strlen(errmsg), 0);
				syslog(LOG_SYSLOG | LOG_ERR, "%s", errmsg);
				free(errmsg);
				continue;
			}

			do_update(key, value);
			send(tinfo->afd, "+OK\n", 4, 0);
			continue;
		}

		// processing 'DELETE' command.
		if (token != NULL && !strcmp(token, "DELETE")) {
			char *key = strtok(NULL, " ");

			if (key == NULL) {
				errmsg = strdup("-ERR wrong number of arguments required for 'DELETE' command.\n");
				send(tinfo->afd, errmsg, strlen(errmsg), 0);
				syslog(LOG_SYSLOG | LOG_ERR, "%s", errmsg);
				free(errmsg);
				continue;
			}

			do_delete(key);
			send(tinfo->afd, "+OK\n", 4, 0);
			continue;
		}

		bzero(response, 1024 * sizeof(char));
		sprintf(response, "Unknown command '%s'\n", token);
		send(tinfo->afd, response, strlen(response), 0);
		syslog(LOG_SYSLOG | LOG_ERR, "%s", response);
	}
}

static void do_loop(void)
{
	int s;
	pthread_attr_t attr;

	while (1) {
		if (listen(thread_info.pfd, 0) < 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "listen() fail.\n");
			break;
		}

		syslog(LOG_SYSLOG | LOG_INFO, "Listening on host: localhost, port:1337.\n");

		if ((thread_info.afd = accept(thread_info.pfd, NULL, NULL)) < 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "accept() fail.\n");
			break;
		}

		syslog(LOG_SYSLOG | LOG_INFO, "Incoming connection accepted.\n");

		if ((s = pthread_attr_init(&attr)) != 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "pthread_attr_init() failed (errno: %d).\n", s);
			break;
		}

		if ((s = pthread_create(&thread_info.thread_id, &attr, &read_and_process, &thread_info)) != 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "pthread_create() failed (errno: %d).\n", s);
			break;
		}

		if ((s = pthread_attr_destroy(&attr)) != 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "pthread_attr_destroy() failed (errno: %d).\n", s);
			break;
		}

		if ((s = pthread_detach(thread_info.thread_id)) != 0) {
			syslog(LOG_SYSLOG | LOG_ERR, "pthread_detach() failed (errno: %d).\n", s);
			break;
		}
	}
}

int main(int argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);

	register_signal_handler();

	int sock_opt = 1;
	int i;
	struct sockaddr_in sd;

	if ((thread_info.pfd = socket(AF_INET, SOCK_STREAM, 0)) < 1) {
		syslog(LOG_SYSLOG | LOG_ERR, "socket() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint initialized.\n");

	if (setsockopt(thread_info.pfd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt)) < 0) {
		syslog(LOG_SYSLOG | LOG_ERR, "setsockopt() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint set to SO_REUSEADDR.\n");

	if (setsockopt(thread_info.pfd, IPPROTO_TCP, TCP_NODELAY, &sock_opt, sizeof(sock_opt)) < 0) {
		syslog(LOG_SYSLOG | LOG_ERR, "setsockopt() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint set to TCP_NODELAY.\n");

	if (setsockopt(thread_info.pfd, SOL_SOCKET, SO_KEEPALIVE, &sock_opt, sizeof(sock_opt)) < 0) {
		syslog(LOG_SYSLOG | LOG_ERR, "setsockopt() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint set to SO_KEEPALIVE.\n");

	bzero(&sd, sizeof(struct sockaddr_in));

	sd.sin_family = AF_INET;
	sd.sin_port = htons(1337);
	sd.sin_addr.s_addr = INADDR_ANY;

	if (bind(thread_info.pfd, (struct sockaddr *)&sd, (socklen_t)(sizeof(struct sockaddr))) < 0) {
		syslog(LOG_SYSLOG | LOG_ERR, "bind() fail.\n");
		return 1;
	}

	syslog(LOG_SYSLOG | LOG_INFO, "Socket endpoint binded to localhost:1337.\n");

	do_loop();

	return 0;
}
