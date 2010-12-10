#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <unistd.h>

#include "mongoose.h"

#define PROGNAME "nitch_server_httpd"

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info);

const char *mg_options[] = {
    // TODO we need to read it from the config file
    "listening_ports", "8080"
};

int main(void) {
    struct mg_context *ctx;

    // TODO the program should be a daemon
    chdir(".");

    ctx = mg_start(&event_handler, mg_options);
    if (ctx == NULL)
        error(2, errno, PROGNAME ": mg_start");

    printf("Press enter to quit.\n");
    getchar();
    mg_stop(ctx);
    return 0;
}

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info)
{
    // TODO 
    return NULL;
}
