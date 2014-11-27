/*
    Copyright (c) 2013 Martin Sustrik  All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef NN_USOCK_INCLUDED
#define NN_USOCK_INCLUDED

/*  Import the definition of nn_iovec. */
#include "../nn.h"
#include "fsm.h"
#include "worker.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

/*  OS-level sockets. */

/*  Event types generated by nn_usock. */
#define NN_USOCK_CONNECTED 1
#define NN_USOCK_ACCEPTED 2
#define NN_USOCK_SENT 3
#define NN_USOCK_RECEIVED 4
#define NN_USOCK_ERROR 5
#define NN_USOCK_ACCEPT_ERROR 6
#define NN_USOCK_STOPPED 7
#define NN_USOCK_SHUTDOWN 8

/*  Maximum number of iovecs that can be passed to nn_usock_send function. */
#define NN_USOCK_MAX_IOVCNT 3

/*  Size of the buffer used for batch-reads of inbound data. To keep the
    performance optimal make sure that this value is larger than network MTU. */
#define NN_USOCK_BATCH_SIZE 2048

struct nn_usock {

    /*  State machine base class. */
    struct nn_fsm fsm;
    int state;

    /*  The worker thread the usock is associated with. */
    struct nn_worker *worker;

    /*  The underlying OS socket and handle that represents it in the poller. */
    int s;
    struct nn_worker_fd wfd;

    /*  Members related to receiving data. */
    struct {

        /*  The buffer being filled in at the moment. */
        uint8_t *buf;
        size_t len;

        /*  Buffer for batch-reading inbound data. */
        uint8_t *batch;

        /*  Size of the batch buffer. */
        size_t batch_len;

        /*  Current position in the batch buffer. The data preceding this
            position were already received by the user. The data that follow
            will be received in the future. */
        size_t batch_pos;
    } in;

    /*  Members related to sending data. */
    struct {

        /*  msghdr being sent at the moment. */
        struct msghdr hdr;

        /*  List of buffers being sent at the moment. Referenced from 'hdr'. */
        struct iovec iov [NN_USOCK_MAX_IOVCNT];
    } out;

    /*  Asynchronous tasks for the worker. */
    struct nn_worker_task task_connecting;
    struct nn_worker_task task_connected;
    struct nn_worker_task task_accept;
    struct nn_worker_task task_send;
    struct nn_worker_task task_recv;
    struct nn_worker_task task_stop;

    /*  Events raised by the usock. */
    struct nn_fsm_event event_established;
    struct nn_fsm_event event_sent;
    struct nn_fsm_event event_received;
    struct nn_fsm_event event_error;

    /*  In ACCEPTING state points to the socket being accepted.
        In BEING_ACCEPTED state points to the listener socket. */
    struct nn_usock *asock;

    /*  Errno remembered in NN_USOCK_ERROR state  */
    int errnum;
};

void nn_usock_init (struct nn_usock *self, int src, struct nn_fsm *owner);
void nn_usock_term (struct nn_usock *self);

int nn_usock_isidle (struct nn_usock *self);
int nn_usock_start (struct nn_usock *self, int domain, int type, int protocol);
void nn_usock_stop (struct nn_usock *self);

void nn_usock_swap_owner (struct nn_usock *self, struct nn_fsm_owner *owner);

int nn_usock_setsockopt (struct nn_usock *self, int level, int optname,
    const void *optval, size_t optlen);

int nn_usock_bind (struct nn_usock *self, const struct sockaddr *addr,
    size_t addrlen);
int nn_usock_listen (struct nn_usock *self, int backlog);

/*  Accept a new connection from a listener. When done, NN_USOCK_ACCEPTED
    event will be delivered to the accepted socket. To cancel the operation,
    stop the socket being accepted. Listening socket should not be stopped
    while accepting a new socket is underway. */
void nn_usock_accept (struct nn_usock *self, struct nn_usock *listener);

/*  When all the tuning is done on the accepted socket, call this function
    to activate standard data transfer phase. */
void nn_usock_activate (struct nn_usock *self);

/*  Start connecting. Prior to this call the socket has to be bound to a local
    address. When connecting is done NN_USOCK_CONNECTED event will be reaised.
    If connecting fails NN_USOCK_ERROR event will be raised. */
void nn_usock_connect (struct nn_usock *self, const struct sockaddr *addr,
    size_t addrlen);

void nn_usock_send (struct nn_usock *self, const struct nn_iovec *iov,
    int iovcnt);
void nn_usock_recv (struct nn_usock *self, void *buf, size_t len);

int nn_usock_geterrno (struct nn_usock *self);

#endif
