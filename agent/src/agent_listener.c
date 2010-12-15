void *
sleep_listener()
{
	DBusConnection *connection;
	DBusMessage *msg;
	DBusError error;
	int ret;
	

	dbus_error_init(&error);

	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

	if( connection == NULL )
	{
		printf("Failed to open connection to bus: %s\n",
				error.message);
		dbus_error_free(&error);
		return 1;
	}
	
	ret = dbus_bus_request_name(connection, "nitch.agent.signal.sink", DBUS_NAME_FLAG_REPLACE_EXISTING , &error);
	if( dbus_error_is_set(&error)){
		printf("Name Error %s\n", error.message);
		dbus_error_free(&error);
		return 1;
	}
	dbus_bus_add_match(connection, "type='signal',interface='agent.signal.Type'", &error);
	if(dbus_error_is_set(&error)){
		printf("Match Error %s\n", error.message);
		return 1;
	}

	printf("sent\n");

	while(1)
	{
		dbus_connection_read_write(connection, 0);
		msg = dbus_connection_pop_message(connection);

		if (NULL== msg)
		{
			sleep(0.5);
			continue;
		}
		if(dbus_message_is_signal(msg, "agent.signal.Type","Test")){
			printf("gotit\n");
		}
	}
}
