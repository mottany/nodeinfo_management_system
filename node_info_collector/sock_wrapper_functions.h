#ifndef _SOCK_WRAPPER_FUNCTIONS_H_
#define _SOCK_WRAPPER_FUNCTIONS_H_

/* wrapper functions */
int wrapped_socket(int, int, int);
int wrapped_bind(int, struct sockaddr *, socklen_t);
int wrapped_listen(int, int);
int wrapped_connect(int, struct sockaddr *, socklen_t);
int wrapped_accept(int, struct sockaddr *, socklen_t *);
int wrapped_send(int, void *, size_t, int);
int wrapped_sendto(int, void *, size_t, int, struct sockaddr *, socklen_t);
int wrapped_recv(int, void *, size_t, int);
int wrapped_recvfrom(int, void *, size_t, int,
		     struct sockaddr *, socklen_t *);
int wrapped_set_recv_timeout(int sock, int sec, int usec);
int wrapped_read(int, void *, size_t);
int wrapped_write(int, void *, size_t);

int wrapped_epoll_create(int);
int wrapped_epoll_ctl(int, int, int, struct epoll_event *);
int wrapped_epoll_wait(int, struct epoll_event *, int, int);

#endif /* _SOCK_WRAPPER_FUNCTIONS_H_ */
