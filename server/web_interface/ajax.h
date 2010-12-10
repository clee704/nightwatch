#ifndef AJAX_H
#define AJAX_H

/**
 * Process the URI /ajax/wake_up.
 *
 * The device_id field in the query string defines the device to be woken up.
 * Currently it is the MAC address of the device. The web server makes a call
 * to the sleep-proxy server to wake up the device. Then the server returns a
 * result, which is returned to the web client as a JSON object.
 *
 * The returned object has two properties: "success" and "message". "success"
 * can be either true or false. "message" is "ok" for success, and
 * "no such device" or "not sleeping" for failure.
 */
void ajax_wake_up(struct mg_connection *, const struct mg_request_info *);

/**
 * Process the URI /ajax/get_device_list.
 *
 * There is no field in the query string. The web server makes a call to the
 * sleep-proxy server to get the list. The list is returned to the web client
 * as a JSON object.
 *
 * The following example shows the structure of the returned object. Note that
 * a device may have many network interfaces but only the interface that
 * communicates with the sleep-proxy server is relevant.
 *
 * [
 *     {"mac": "00:11:22:33:44:55", "ip": "10.0.0.1", "status": "on"},
 *     {"mac": "00:11:22:33:44:56", "ip": "10.0.0.2", "status": "waking up"},
 *     {"mac": "00:11:22:33:44:57", "ip": "10.0.0.3", "status": "sleeping"}
 * ]
 *
 * Currently there are three status for a device: "on", "waking up", and
 * "sleeping".
 */
void ajax_get_device_list(struct mg_connection *,
                          const struct mg_request_info *);

#endif /* AJAX_H */
