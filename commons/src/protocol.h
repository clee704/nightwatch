#ifndef PROTOCOL_H
#define PROTOCOL_H

// including the null caracter
#define MAX_URI_LEN 64
#define MAX_MESSAGE_LEN 64
#define MAX_DATA_LEN 512

// The maximum lengths of requests and responses in the serialized form,
// including the null character.
#define MAX_REQUEST_LEN (MAX_URI_LEN + MAX_DATA_LEN + 8)
#define MAX_RESPONSE_LEN (MAX_MESSAGE_LEN + MAX_DATA_LEN + 7)

/**
 * A request consists of a method, a URI, and data. The serialized form is
 * "<method>[ <uri>]\n[\n<data>\n\n]".
 *
 * The following list describes implemented methods in the protocol. Note that
 * 200 (OK) and 400 (Bad Request) can be a response status code to any
 * request.
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
    int has_uri:1;
    int has_data:1;
    char uri[MAX_URI_LEN];
    char data[MAX_DATA_LEN];
};

/**
 * A response consists of a status code, a message, and data. The serialized
 * form is "<status code> <message>\n[\n<data>\n\n]".
 *
 * The following is a list of status codes and corresponding messages.
 *
 * - 200 OK:
 *   Standard response for successful requests.
 * - 400 Bad Request:
 *   The request could not be understood by the server due to malformed
 *   syntax.
 * - 404 Not Found:
 *   The requested URI does not represent a resource, such as a device.
 * - 409 Conflict:
 *   If this is a response to RSUM, it means the device is already up or
 *   resuming. To SUSP, it means the device is already suspended.
 */
struct response {
    int status;
    char message[MAX_MESSAGE_LEN];
    int has_data:1;
    char data[MAX_DATA_LEN];
};

/**
 * Parse the serialized request into the request structure.
 *
 * Return 0 on success, -1 on error.
 */
int
parse_request(const char *, struct request *);

/**
 * Parse the serialized response into the response structure.
 *
 * Return 0 on success, -1 on error.
 */
int
parse_response(const char *, struct response *);

/**
 * Serialize the request into the character buffer.
 *
 * Return the number of written characters (not including the null character)
 * on success, -1 on error.
 */
int
serialize_request(const struct request *, char *);

/**
 * Serialize the response into the character buffer.
 *
 * Return the number of written characters (not including the null character)
 * on success, -1 on error.
 */
int
serialize_response(const struct response *, char *);

#endif /* PROTOCOL_H */
