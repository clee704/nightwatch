#ifndef PROTOCOL_H
#define PROTOCOL_H

// The maximum lengths of requests and responses in the serialized form.
#define MAX_REQUEST_LEN 586
#define MAX_RESPONSE_LEN 585

/**
 * A request consists of a method, a URI, and data. The serialized form is
 * "<method> <uri>\n" for short methods, and "<method> <uri>\n\n<data>\n\n"
 * for long methods.
 *
 * The following list describes implemented methods in the protocol. Unless
 * otherwise noted, the only possible response status code is 200 (OK).
 *
 * - GETA:
 *   Get a list of all devices.
 * - RSUM:
 *   Resume a device. The device is identified by the MAC address (in the
 *   standard hex-digits-and-colons notation) in the URI field. The possible
 *   response status codes are 200, 404, and 409.
 * - SUSP:
 *   Suspend a device. If the request is from the proxy to an agent, then
 *   the agent suspends the device on which it is running. If the request
 *   is from the web UI to the proxy, then the device is identified by the
 *   MAC address in the URI field, in the same manner as RSUM. For requests
 *   from the web UI to the proxy, the possible response status codes are 200,
 *   404 and 409.
 * - PING:
 *   Do nothing. This is used by the proxy to check whether a device is live
 *   or not.
 * - INFO:
 *   Report the device's hostname and MAC address. An agent must send this
 *   request to the proxy before sending any other requests. The hostname
 *   and MAC address is placed in the data field, separated by a newline
 *   character.
 * - NTFY:
 *   Notify the proxy that the device is going to suspend.
 */
struct request {
    enum METHOD { GETA, RSUM, SUSP, PING, INFO, NTFY } method;
    char uri[64];
    char data[512];
};

/**
 * A response consists of a status code, a message, and data. The serialized
 * form is "<status code> <message>\n" for short responses, and
 * "<status code> <message>\n\n<data>\n\n" for long responses.
 *
 * The following is a list of status codes and corresponding messages.
 *
 * - 200 OK:
 *   Standard response for successful requests.
 * - 404 Not Found:
 *   The requested URI does not represent a resource, such as a device.
 * - 409 Conflict:
 *   If this is a response to RSUM, it means the device is already up or
 *   resuming. To SUSP, it means the device is already suspended.
 */
struct response {
    int status;
    char message[64];
    char data[512];
}

#endif /* PROTOCOL_H */
