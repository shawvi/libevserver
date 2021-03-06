#include <stdio.h>
#include <stdlib.h>

#include "evsrv.h"


// We define a custom connection with some extra data.
// Just for an example there will be a message, which will be send to client.
typedef struct {
    evsrv_conn conn;
    char* message;
} tcpserver_conn;

void on_started(evsrv* srv);
evsrv_conn* on_conn_create(evsrv* srv, struct evsrv_conn_info* info);
void on_conn_destroy(evsrv_conn* conn, int err);
void on_read(evsrv_conn* conn, ssize_t nread);
bool on_conn_graceful(tcpserver_conn* c);
void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents);
void on_gracefully_stopped(evsrv* srv);


int main() {
    struct ev_loop* loop = EV_DEFAULT;

    evsrv srv;
    evsrv_init(loop, &srv, "127.0.0.1", "9090");
    evsrv_set_on_started(&srv, on_started);                                     // will be called on server start
    evsrv_set_on_conn(&srv, on_conn_create, on_conn_destroy);                   // to enable custom connections creation we need to
                                                                                // define on_conn_create and on_conn_destroy functions

    if (evsrv_bind(&srv) == -1) {                                               // binds to host:port
        return EXIT_FAILURE;
    }
    if (evsrv_listen(&srv) == -1) {                                             // starts listening on host:port
        return EXIT_FAILURE;
    }

    ev_signal sig;
    ev_signal_init(&sig, sigint_cb, SIGINT);
    sig.data = (void*) &srv;
    ev_signal_start(srv.loop, &sig);

    evsrv_accept(&srv);                                                         // beginning to accept connections
    ev_run(loop, 0);

    evsrv_destroy(&srv);                                                          // cleaning srv
    ev_loop_destroy(srv.loop);
}

void on_started(evsrv* srv) {
    printf("Started demo server at %s:%s\n", srv->host, srv->port);
}

evsrv_conn* on_conn_create(evsrv* srv, struct evsrv_conn_info* info) {
    tcpserver_conn* c = (tcpserver_conn*) malloc(sizeof(tcpserver_conn));       // allocating memory for our connection
    evsrv_conn_init(&c->conn, srv, info);                                       // initializing evsrv_conn

    evsrv_conn_set_rbuf(&c->conn, (char*) malloc(1024), 1024);                  // allocating buffer with size 1024
    evsrv_conn_set_on_read(&c->conn, on_read);                                  // setting on_read callback for this connection
    evsrv_conn_set_on_graceful_close(&c->conn, on_conn_graceful);               // setting on_graceful_close callback - it will be called when graceful shutdown requested

    c->message = "Hello from TCP server!";                                      // here goes our custom message

    return (evsrv_conn*) c;                                                     // returning newly created connection
}

void on_conn_destroy(evsrv_conn* conn, int err) {
    tcpserver_conn* c = (tcpserver_conn*) conn;                                 // casting evsrv_conn* to our connection type
    evsrv_conn_destroy(&c->conn);                                                 // cleaning evsrv_conn

    free(c->conn.rbuf);                                                         // cleaning previously allocated buffer
    c->conn.rbuf = NULL;
    free(c);                                                                    // cleaning an entire structure
}

void on_read(evsrv_conn* conn, ssize_t nread) {
    tcpserver_conn* c = (tcpserver_conn*) conn;

    evsrv_conn_write(conn, c->message, 0);                                      // just replying with out custom message
    conn->ruse = 0;                                                             // setting ruse to 0, in order to not exceed read buffer size in future
}

bool on_conn_graceful(tcpserver_conn* c) {
    evsrv_conn_write(&c->conn, "Server is shutting down!", 0);                  // sending to each client that is server is shutting down
    evsrv_conn_shutdown(&c->conn, EVSRV_SHUT_RD);                               // shutdown connection for READ
    evsrv_conn_close(&c->conn, 0);                                              // closing connection (i.e. destroying it)
    return true;                                                                // returning true as we closed connection (if we didn't close it - false has to be returned)
}

void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    evsrv* srv = (evsrv*) w->data;
    evsrv_graceful_stop(srv, on_gracefully_stopped);                            // gracefully shutting down server with callback that has to be called in the end
}

void on_gracefully_stopped(evsrv* srv) {
    printf("Gracefully stopped %s:%s", srv->host, srv->port);
    ev_break(srv->loop, EVBREAK_ALL);
}
