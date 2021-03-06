#include <stdio.h>

#include "evsrv_manager.h"
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

typedef struct {
    evsrv srv;
} my1_srv;

typedef struct {
    evsrv srv;
} my2_srv;

typedef struct {
    evsrv_conn conn;
} my1_conn;




static void on_read(evsrv_conn* conn, ssize_t nread);
static bool on_graceful_conn_close(evsrv_conn* conn);

static evsrv_conn* on_conn_create(evsrv* srv, struct evsrv_conn_info* info) {
    my1_conn* c = (my1_conn*) malloc(sizeof(my1_conn));
    evsrv_conn_init(&c->conn, srv, info);

    evsrv_conn_set_rbuf(&c->conn, (char*) malloc(EVSRV_DEFAULT_BUF_LEN), EVSRV_DEFAULT_BUF_LEN);
    evsrv_conn_set_on_read(&c->conn, on_read);
    evsrv_conn_set_on_graceful_close(&c->conn, on_graceful_conn_close);
    return (evsrv_conn*) c;
}

static void on_conn_destroy(evsrv_conn* conn, int err) {
    my1_conn* c = (my1_conn*) conn;
    evsrv_conn_destroy(&c->conn);
    free(c->conn.rbuf);
    c->conn.rbuf = NULL;
    free(c);
}

void on_read(evsrv_conn* conn, ssize_t nread) {
    char* rbuf = conn->rbuf;
    char* end = rbuf + conn->ruse;

    size_t size = 10;

    while (rbuf < end) {
        ptrdiff_t buf_len = end - rbuf;
        if (buf_len < size) {
            break;
        }

        evsrv_conn_write(conn, rbuf, size);
        rbuf += size;

        if (rbuf == end) {
            conn->ruse = 0;
            break;
        }
    }

    // printf("%.*s", (int) nread, buf);
    conn->ruse = (uint32_t) (end - rbuf);
    if (conn->ruse > 0) {
        memmove(conn->rbuf, rbuf, conn->ruse);
    }
}

static bool on_graceful_conn_close(evsrv_conn* conn) {
//    int sock = conn->info->sock;
    evsrv_conn_close(conn, 0);
    return true;
}

void on_started_my1(evsrv* srv) {
    printf("Started demo my1 server at %s:%s\n", srv->host, srv->port);
}

void on_started_my2(evsrv* srv) {
    printf("Started demo my2 server at %s:%s\n", srv->host, srv->port);
}



static evsrv* on_my1_create(evsrv_manager* self, size_t id, evsrv_info* info) {
    my1_srv* s = (my1_srv*) malloc(sizeof(my1_srv));
    evsrv_init(self->loop, &s->srv, info->host, info->port);
    s->srv.backlog = 500;
    s->srv.write_timeout = 0.0;
    evsrv_set_on_started(&s->srv, on_started_my1);
    evsrv_set_on_conn(&s->srv, on_conn_create, on_conn_destroy);
    return (evsrv*) s;
}

static void on_my1_destroy(evsrv* self) {
    my1_srv* s = (my1_srv*) self;
    evsrv_destroy(&s->srv);
    free(s);
}


static evsrv* on_my2_create(evsrv_manager* self, size_t id, evsrv_info* info) {
    my2_srv* s = (my2_srv*) malloc(sizeof(my1_srv));
    evsrv_init(self->loop, &s->srv, info->host, info->port);
    s->srv.backlog = SOMAXCONN;
    s->srv.write_timeout = 0.0;
    evsrv_set_on_started(&s->srv, on_started_my2);
    evsrv_set_on_conn(&s->srv, on_conn_create, on_conn_destroy);
    return (evsrv*) s;
}

static void on_my2_destroy(evsrv* self) {
    my2_srv* s = (my2_srv*) self;
    evsrv_destroy(&s->srv);
    free(s);
}

static void on_gracefully_stopped(evsrv_manager* server) {
    cwarn("Gracefully stopped evserver");
    ev_break(server->loop, EVUNLOOP_ALL);
}


static void signal_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    evsrv_manager* server = (evsrv_manager*) w->data;
    evsrv_manager_graceful_stop(server, on_gracefully_stopped);
}



int main() {

    struct ev_loop* loop = EV_DEFAULT;

    evsrv_info hosts[] = {
            { "127.0.0.1", "9090", on_my1_create, on_my1_destroy },
            { "127.0.0.1", "7070", on_my2_create, on_my2_destroy },
    };
    size_t hosts_len = sizeof(hosts) / sizeof(hosts[0]);

    evsrv_manager mgr;
    evsrv_manager_init(loop, &mgr, hosts, hosts_len);

    ev_signal sig;
    ev_signal_init(&sig, signal_cb, SIGINT);
    sig.data = (void*) &mgr;
    ev_signal_start(loop, &sig);


    evsrv_manager_bind(&mgr);
    evsrv_manager_listen(&mgr);
    evsrv_manager_accept(&mgr);
    ev_run(mgr.loop, 0);


//    int max_childs = 8;
//    for (int i = 0; i < max_childs; ++i) {
//        pid_t pid = fork();
//        if (pid > 0) {
//            // master
//
//        } else if (pid == 0) {
//            // child
//            ev_loop_fork(loop);
//            evsrv_manager_accept(&mgr);
//            ev_run(loop, 0);
//            break;
//        } else {
//            // error
//            perror("fork failed");
//            break;
//        }
//    }

    evsrv_manager_destroy(&mgr);
    ev_loop_destroy(loop);

}
