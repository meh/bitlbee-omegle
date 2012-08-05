/*
 *  omegle_http.c - HTTP helpers for Omegle
 *
 *  Copyright (c) 2012 by meh. <meh@paranoici.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

static GString *g_string_append_escaped(GString *string, char *data)
{
	int i, length = strlen(data);

	for (i = 0; i < length; i++) {
		if (isalpha(data[i]) || isdigit(data[i]) ||
		    data[i] == '-' || data[i] == '_' || data[i] == '.' || data[i] == '!' ||
		    data[i] == '~' || data[i] == '*' || data[i] == '\'' || data[i] == '(' || data[i] == ')') {
			g_string_append_c(string, data[i]);
		} else if (data[i] == ' ') {
			g_string_append_c(string, '+');
		} else {
			g_string_append_printf(string, "%%%x", data[i]);
		}
	}

	return string;
}

static void omegle_http_dummy(struct http_request *req)
{
}

static void omegle_get(void *to_pass, char *host, char *path, http_input_function callback)
{
	GString *request;

	request = g_string_new("");

	g_string_append_printf(request, "GET %s HTTP/1.0\r\n", path);
	g_string_append_printf(request, "Host: %s\r\n", host);
	g_string_append_printf(request, "User-Agent: BitlBee " BITLBEE_VERSION " " ARCH "/" CPU "\r\n");
	g_string_append_printf(request, "\r\n");

	http_dorequest(host, 80, 0, request->str, callback, to_pass);

	g_string_free(request, TRUE);
}

static void omegle_post(void *to_pass, char *session_id, char *host, char *path, char *data, http_input_function callback)
{
	GString *request;
	GString *form;

	request = g_string_new("");
	form    = g_string_new("");

	g_string_append_printf(form, "id=%s&%s", session_id, data);

	g_string_append_printf(request, "POST %s HTTP/1.0\r\n", path);
	g_string_append_printf(request, "Host: %s\r\n", host);
	g_string_append_printf(request, "User-Agent: BitlBee " BITLBEE_VERSION " " ARCH "/" CPU "\r\n");
	g_string_append_printf(request, "Content-Type: application/x-www-form-urlencoded\r\n");
	g_string_append_printf(request, "Content-Length: %zd\r\n\r\n", form->len);
	g_string_append_printf(request, "%s", form->str);

	http_dorequest(host, 80, 0, request->str, callback, to_pass);

	g_string_free(request, TRUE);
	g_string_free(form, TRUE);
}
