#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mongoose.h"
#include "ajax.h"

#define PROGNAME "nitch_server_httpd"

//
// TODO We need to get some options from the config file
//
static const char *mg_options[] = {
    "listening_ports", "8080",
    "document_root", "./var",
    "num_threads", "3",
    NULL
};

static void *event_handler(enum mg_event event,
                           struct mg_connection *conn,
                           const struct mg_request_info *request_info);

int main(void) {
    struct mg_context *ctx;

    //
    // TODO The program should be a daemon
    //

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
    const char *method = request_info->request_method;
    const char *uri = request_info->uri;

    if (event != MG_NEW_REQUEST)
        return NULL;
    else if (strcmp(method, "GET") != 0)
        return NULL;
    else if (strcmp(uri, "/ajax/resume") == 0)
        ajax_resume(conn, request_info);
    else if (strcmp(uri, "/ajax/devicelist") == 0)
        ajax_device_list(conn, request_info);
    else
        return NULL;
    return "processed";
}
