#ifndef PROTOCOL_H
#define PROTOCOL_H

// Lengths includes the null caracter
#define MAX_URI_LEN 64
#define MAX_MESSAGE_LEN 64
#define MAX_DATA_LEN 512

// Maximum lengths of requests and responses in the serialized form
// Lengths includes the null caracter
#define MAX_REQUEST_STRLEN (MAX_URI_LEN + MAX_DATA_LEN + 8)
#define MAX_RESPONSE_STRLEN (MAX_MESSAGE_LEN + MAX_DATA_LEN + 7)

/**
 * A request consists of a method, a URI, and data. The serialized form is
 * "<method>[ <uri>]\n[\n<data>\n]".
 *
 * - GETA:
 *     Request a list of all devices.
 * - RSUM:
 *     Resume a device. The device is identified by the MAC address (in the
 *     standard hex-digits-and-colons notation) in the URI field.
 * - SUSP:
 *     Suspend a device. This method has two versions. If the request is to an
 *     agent, then the agent suspends the device on which it is running. If the
 *     request is to the sleep proxy, then it requests SUSP to an agent that
 *     is running on the device that is identified by the MAC address in the
 *     URI field, in the same manner as RSUM.
 * - PING:
 *     Request nothing but the 200 OK response. This is used to check whether
 *     a device is live or not.
 * - INFO:
 *     Report the device's hostname and MAC address. An agent must send this
 *     request to the sleep proxy before sending any other requests. The
 *     hostname and MAC address is placed in the data field, separated by a
 *     newline character.
 * - NTFY:
 *     Notify that the device is going to suspend. An agent must send this
 *     request to the sleep proxy when it detects that the device on which
 *     it is running is goint to suspend, even if that is the result of a SUSP
 *     request to the agent.
 */
struct request {
    enum METHOD { GETA, RSUM, SUSP, PING, INFO, NTFY } method;
    char uri[MAX_URI_LEN];
    char data[MAX_DATA_LEN];
    int has_uri:1;
    int has_data:1;
};

/**
 * A response consists of a status code, a message, and data. The serialized
 * form is "<status code> <message>\n[\n<data>\n]".
 *
 * The following is a list of status codes and corresponding messages. Some
 * are borrowed from the HTTP status code definitions.
 *
 * - 200 OK:
 *     Standard response for successful requests.
 * - 400 Bad Request:
 *     The request could not be understood by the server due to malformed
 *     syntax.
 * - 402 Payment Required:
 *     The request must have data.
 * - 404 Not Found:
 *     The server has not found anything matching the Request-URI.
 * - 409 Conflict:
 *     The request could not be completed due to a conflict with the current
 *     state of the resource. If this is a response to RSUM, it means the
 *     device is already up or resuming. To SUSP, it means the device is
 *     already suspended.
 * - 418 Unexpected Method:
 *     The request method is not allowed in the current state of the server.
 * - 500 Internal Server Error:
 *     The server encountered an unexpected condition which prevented it from
 *     fulfilling the request.
 * - 501 Not Implemented:
 *     The server does not support the functionality required to fulfill the
 *     request. 
 */
struct response {
    int status;
    char message[MAX_MESSAGE_LEN];
    char data[MAX_DATA_LEN];
    int has_data:1;
};

/**
 * Parse the serialized request into the request structure.
 *
 * Return 0 on success, -1 on error.
 */
int parse_request(const char *, struct request *);

/**
 * Parse the serialized response into the response structure.
 *
 * Return 0 on success, -1 on error.
 */
int parse_response(const char *, struct response *);

/**
 * Serialize the request into the character buffer.
 *
 * Return the number of written characters (not including the null character)
 * on success, -1 on error.
 */
int serialize_request(const struct request *, char *);

/**
 * Serialize the response into the character buffer.
 *
 * Return the number of written characters (not including the null character)
 * on success, -1 on error.
 */
int serialize_response(const struct response *, char *);

#endif /* PROTOCOL_H */
