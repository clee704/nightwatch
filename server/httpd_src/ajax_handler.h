#ifndef AJAX_HANDLER_H
#define AJAX_HANDLER_H

void ajax_set_socket_file(const char *sock_file);

struct mg_connection;
struct mg_request_info;

/**
 * Resume the specified device. The request URI is /ajax/resume?deviceId=<id>,
 * where <id> is replaced by the actual value.
 *
 * The deviceId field in the query string defines the device to be resumed.
 * Currently it is the MAC address of the device. The web server makes a call
 * to the sleep proxy server to resume the device. Then the server returns a
 * result, which is returned to the web client as a JSON object.
 *
 * The returned object has two properties: "success" and "message". "success"
 * can be either true or false. "message" is "ok" for success, and
 * "no such device" or "already up or resuming" for failure.
 */
void ajax_resume(struct mg_connection *, const struct mg_request_info *);

/**
 * Suspend to memory the specified device. The request URI is
 * /ajax/suspend?deviceId=<id>, where <id> is replaced by the actual value.
 *
 * The detail is similar to ajax_resume(). The messages for failure are
 * "no such device" and "already suspended".
 */
void ajax_suspend(struct mg_connection *, const struct mg_request_info *);

/**
 * Return the list of the devices that are connected to the sleep proxy server.
 * The request URI is /ajax/devicelist.
 *
 * There is no field in the query string. The web server makes a call to the
 * sleep proxy server to get the list. The list is returned to the web client
 * as a JSON object.
 *
 * The returned object is an array of objects, each for one device. The
 * following is an example of the result.
 *
 * [
 *     {
 *         "hostname": "mimosa",
 *         "ip": "10.0.0.2",
 *         "mac": "00:11:22:33:44:55",
 *         "state": "up",
 *         "monitoredSince": 1267369283000,
 *         "totalUptime": 3517003,
 *         "sleepTime": 2300910,
 *         "totalDowntime": 36713
 *     }
 * ]
 *
 * Hostname can be an empty string. Total uptime includes sleep time. Times
 * are represented in milliseconds. Currently there are 6 states for a device:
 * "up", "resuming", "suspended", and "down". "down" means the device is not
 * responding, not it is actually down.
 *
 * Note that a device may have many network interfaces but only the interface
 * that communicates with the sleep proxy server is relevant.
 */
void ajax_device_list(struct mg_connection *, const struct mg_request_info *);

#endif /* AJAX_HANDLER_H */
