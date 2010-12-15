#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>

void sendsignal(char* sigvalue)
{
	DBusMessage* msg;
	DBusMessageIter args;
	DBusConnection* conn;
	DBusError err;
	int ret;
	dbus_uint32_t serial = 0;

	dbus_error_init(&err);

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (dbus_error_is_set(&err)) { 
		fprintf(stderr, "Connection Error (%s)\n", err.message); 
		dbus_error_free(&err); 
	}
	if (NULL == conn) { 
		exit(1); 
	}

	ret = dbus_bus_request_name(conn, "nitch.agent.signal.source",DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
	if (dbus_error_is_set(&err)) { 
		fprintf(stderr, "Name Error (%s)\n", err.message); 
		dbus_error_free(&err); 
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
		exit(1);
	}
	msg = dbus_message_new_signal("/ntest/signal/Object",
			"agent.signal.Type",
			"Test");
	if (NULL == msg) 
	{ 
		fprintf(stderr, "Message Null\n"); 
		exit(1); 
	}
	dbus_message_iter_init_append(msg, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &sigvalue)) {
		fprintf(stderr, "Out Of Memory!\n"); 
		exit(1);
	}
	if (!dbus_connection_send(conn, msg, &serial)) {
		fprintf(stderr, "Out Of Memory!\n"); 
		exit(1);
	}
	dbus_connection_flush(conn);
	printf("sig sent\n");
	dbus_message_unref(msg);
}
int main()
{
	sendsignal("sleep");
}
