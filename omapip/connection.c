/* connection.c

   Subroutines for dealing with connections. */

/*
 * Copyright (c) 1996-1999 Internet Software Consortium.
 * Use is subject to license terms which appear in the file named
 * ISC-LICENSE that should have accompanied this file when you
 * received it.   If a file named ISC-LICENSE did not accompany this
 * file, or you are not sure the one you have is correct, you may
 * obtain an applicable copy of the license at:
 *
 *             http://www.isc.org/isc-license-1.0.html. 
 *
 * This file is part of the ISC DHCP distribution.   The documentation
 * associated with this file is listed in the file DOCUMENTATION,
 * included in the top-level directory of this release.
 *
 * Support and other services are available for ISC products - see
 * http://www.isc.org for more information.
 */

#include <omapip/omapip.h>

isc_result_t omapi_connect (omapi_object_t *c,
			    char *server_name,
			    int port)
{
	struct hostent *he;
	int hix;
	isc_result_t status;
	omapi_connection_object_t *obj;

	obj = (omapi_connection_object_t *)malloc (sizeof *obj);
	if (!obj)
		return ISC_R_NOMEMORY;
	memset (obj, 0, sizeof *obj);
	obj -> refcnt = 1;
	obj -> type = omapi_type_connection;

	status = omapi_object_reference (&c -> outer, (omapi_object_t *)obj,
					 "omapi_protocol_connect");
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference ((omapi_object_t **)&obj,
					  "omapi_protocol_connect");
		return status;
	}
	status = omapi_object_reference (&obj -> inner, c,
					 "omapi_protocol_connect");
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference ((omapi_object_t **)&obj,
					  "omapi_protocol_connect");
		return status;
	}

	/* Set up all the constants in the address... */
	obj -> remote_addr.sin_port = htons (port);

	/* First try for a numeric address, since that's easier to check. */
	if (!inet_aton (server_name, &obj -> remote_addr.sin_addr)) {
		/* If we didn't get a numeric address, try for a domain
		   name.  It's okay for this call to block. */
		he = gethostbyname (server_name);
		if (!he) {
			omapi_object_dereference ((omapi_object_t **)&obj,
						  "omapi_connect");
			return ISC_R_HOSTUNKNOWN;
		}
		hix = 1;
		memcpy (&obj -> remote_addr.sin_addr,
			he -> h_addr_list [0],
			sizeof obj -> remote_addr.sin_addr);
	} else
		he = (struct hostent *)0;

	obj -> remote_addr.sin_len =
		sizeof (struct sockaddr_in);
	obj -> remote_addr.sin_family = AF_INET;
	memset (&(obj -> remote_addr.sin_zero), 0,
		sizeof obj -> remote_addr.sin_zero);

	/* Create a socket on which to communicate. */
	obj -> socket =
		socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (obj -> socket < 0) {
		omapi_object_dereference ((omapi_object_t **)&obj,
					  "omapi_connect");
		if (errno == EMFILE || errno == ENFILE || errno == ENOBUFS)
			return ISC_R_NORESOURCES;
		return ISC_R_UNEXPECTED;
	}
	
	/* Try to connect to the one IP address we were given, or any of
	   the IP addresses listed in the host's A RR. */
	while (connect (obj -> socket,
			((struct sockaddr *)
			 &obj -> remote_addr),
			sizeof obj -> remote_addr)) {
		if (!he || !he -> h_addr_list [hix]) {
			omapi_object_dereference ((omapi_object_t **)&obj,
						  "omapi_connect");
			if (errno == ECONNREFUSED)
				return ISC_R_CONNREFUSED;
			if (errno == ENETUNREACH)
				return ISC_R_NETUNREACH;
			return ISC_R_UNEXPECTED;
		}
		memcpy (&obj -> remote_addr.sin_addr,
			he -> h_addr_list [hix++],
			sizeof obj -> remote_addr.sin_addr);
	}

	obj -> state = omapi_connection_connected;

	/* I don't know why this would fail, so I'm tempted not to test
	   the return value. */
	hix = sizeof (obj -> local_addr);
	if (getsockname (obj -> socket,
			 ((struct sockaddr *)
			  &obj -> local_addr), &hix) < 0) {
	}

	if (fcntl (obj -> socket, F_SETFL, O_NONBLOCK) < 0) {
		omapi_object_dereference ((omapi_object_t **)&obj,
					  "omapi_connect");
		return ISC_R_UNEXPECTED;
	}

	status = omapi_register_io_object ((omapi_object_t *)obj,
					   omapi_connection_readfd,
					   omapi_connection_writefd,
					   omapi_connection_reader,
					   omapi_connection_writer,
					   omapi_connection_reaper);
	if (status != ISC_R_SUCCESS) {
		omapi_object_dereference ((omapi_object_t **)&obj,
					  "omapi_connect");
		return status;
	}

	return ISC_R_SUCCESS;
}

/* Disconnect a connection object from the remote end.   If force is nonzero,
   close the connection immediately.   Otherwise, shut down the receiving end
   but allow any unsent data to be sent before actually closing the socket. */

isc_result_t omapi_disconnect (omapi_object_t *h,
			       int force)
{
	omapi_connection_object_t *c;

	c = (omapi_connection_object_t *)h;
	if (c -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;

	if (!force) {
		/* If we're already disconnecting, we don't have to do
		   anything. */
		if (c -> state == omapi_connection_disconnecting)
			return ISC_R_SUCCESS;

		/* Try to shut down the socket - this sends a FIN to the
		   remote end, so that it won't send us any more data.   If
		   the shutdown succeeds, and we still have bytes left to
		   write, defer closing the socket until that's done. */
		if (!shutdown (c -> socket, SHUT_RD)) {
			if (c -> out_bytes > 0) {
				c -> state = omapi_connection_disconnecting;
				return ISC_R_SUCCESS;
			}
		}
	}
	close (c -> socket);
	c -> state = omapi_connection_closed;

	/* Disconnect from I/O object, if any. */
	if (h -> outer)
		omapi_object_dereference (&h -> outer, "omapi_disconnect");

	/* If whatever created us registered a signal handler, send it
	   a disconnect signal. */
	omapi_signal (h, "disconnect", h);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_require (omapi_object_t *h, int bytes)
{
	omapi_connection_object_t *c;

	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	c = (omapi_connection_object_t *)h;

	c -> bytes_needed = bytes;
	if (c -> bytes_needed <= c -> in_bytes) {
		return ISC_R_SUCCESS;
	}
	return ISC_R_NOTYET;
}

/* Return the socket on which the dispatcher should wait for readiness
   to read, for a connection object.   If we already have more bytes than
   we need to do the next thing, and we have at least a single full input
   buffer, then don't indicate that we're ready to read. */
int omapi_connection_readfd (omapi_object_t *h)
{
	omapi_connection_object_t *c;
	if (h -> type != omapi_type_connection)
		return -1;
	c = (omapi_connection_object_t *)h;
	if (c -> state != omapi_connection_connected)
		return -1;
	if (c -> in_bytes >= OMAPI_BUF_SIZE - 1 &&
	    c -> in_bytes > c -> bytes_needed)
		return -1;
	return c -> socket;
}

/* Return the socket on which the dispatcher should wait for readiness
   to write, for a connection object.   If there are no bytes buffered
   for writing, then don't indicate that we're ready to write. */
int omapi_connection_writefd (omapi_object_t *h)
{
	omapi_connection_object_t *c;
	if (h -> type != omapi_type_connection)
		return -1;
	if (c -> out_bytes)
		return c -> socket;
	else
		return -1;
}

/* Reaper function for connection - if the connection is completely closed,
   reap it.   If it's in the disconnecting state, there were bytes left
   to write when the user closed it, so if there are now no bytes left to
   write, we can close it. */
isc_result_t omapi_connection_reaper (omapi_object_t *h)
{
	omapi_connection_object_t *c;

	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;

	c = (omapi_connection_object_t *)h;
	if (c -> state == omapi_connection_disconnecting &&
	    c -> out_bytes == 0)
		omapi_disconnect (h, 1);
	if (c -> state == omapi_connection_closed)
		return ISC_R_NOTCONNECTED;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_set_value (omapi_object_t *h,
					 omapi_object_t *id,
					 omapi_data_string_t *name,
					 omapi_typed_data_t *value)
{
	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_connection_get_value (omapi_object_t *h,
					 omapi_object_t *id,
					 omapi_data_string_t *name,
					 omapi_value_t **value)
{
	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_connection_destroy (omapi_object_t *h, char *name)
{
	omapi_connection_object_t *c;

	if (h -> type != omapi_type_connection)
		return ISC_R_UNEXPECTED;
	c = (omapi_connection_object_t *)(h);
	if (c -> state == omapi_connection_connected)
		omapi_disconnect (h, 1);
	if (c -> listener)
		omapi_object_dereference (&c -> listener, name);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_connection_signal_handler (omapi_object_t *h,
					      char *name, va_list ap)
{
	if (h -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t omapi_connection_stuff_values (omapi_object_t *c,
					    omapi_object_t *id,
					    omapi_object_t *m)
{
	int i;

	if (m -> type != omapi_type_connection)
		return ISC_R_INVALIDARG;

	if (m -> inner && m -> inner -> type -> stuff_values)
		return (*(m -> inner -> type -> stuff_values)) (c, id,
								m -> inner);
	return ISC_R_SUCCESS;
}