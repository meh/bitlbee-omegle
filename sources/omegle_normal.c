/*
 *  omegle_normal.c - Omegle normal chat implementation.
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

static void omegle_normal_send(struct im_connection *ic, char *who, char *path)
{
	struct bee_user *bu = bee_user_by_handle(ic->bee, ic, who);
	struct omegle_buddy_data *bd = bu->data;

	omegle_post(bu, bd->session_id, bd->host, path, NULL, omegle_http_dummy);
}

static void omegle_normal_send_with_callback(struct im_connection *ic, char *who, char *path, http_input_function callback)
{
	struct bee_user *bu = bee_user_by_handle(ic->bee, ic, who);
	struct omegle_buddy_data *bd = bu->data;

	omegle_post(bu, bd->session_id, bd->host, path, NULL, callback);
}

static void omegle_normal_send_message(struct im_connection *ic, char *who, char *message)
{
	struct bee_user *bu = bee_user_by_handle(ic->bee, ic, who);
	struct omegle_buddy_data *bd = bu->data;

	GString *data = g_string_new("msg=");

	if (g_string_append_escaped(data, message))
		omegle_post(bu, bd->session_id, bd->host, "/send", data->str, omegle_http_dummy);

	g_string_free(data, TRUE);
}

static void omegle_normal_convo_got_id(struct http_request *req)
{
	struct bee_user *bu = req->data;
	struct omegle_buddy_data *bd = bu->data;
	
	if (req->status_code != 200)
		goto error;

	if (req->reply_body[0] != '"' || req->reply_body[strlen(req->reply_body) - 1] != '"')
		goto error;

	bd->session_id = g_strndup(req->reply_body + 1, strlen(req->reply_body) - 2);

	return;

error:
	bd->disconnected = TRUE;
}

static void omegle_normal_start_convo(struct im_connection *ic, char *who, char *host)
{
	struct bee_user *bu = bee_user_by_handle(ic->bee, ic, who);
	struct omegle_buddy_data *bd = bu->data;
	GString *url = g_string_new("/start");
	GString *likes = g_string_new("");
	GSList *l;

	bd->host = host;

	if (bd->likes) {
		g_string_append(url, "?topics=");

		g_string_append(likes, "[");

		for (l = bd->likes; l; l = l->next) {
			g_string_append_printf(likes, "\"%s\",", (char*) l->data);
		}

		g_string_truncate(likes, likes->len - 1);

		g_string_append(likes, "]");

		g_string_append_escaped(url, likes->str);
	}

	omegle_get(bu, host, url->str, omegle_normal_convo_got_id);

	g_string_free(url, TRUE);
	g_string_free(likes, TRUE);
}

static void omegle_normal_chose_server(struct http_request *req)
{
	struct bee_user *bu = req->data;
	struct omegle_buddy_data *bd = bu->data;
	struct im_connection *ic = bu->ic;
	char *who = bu->handle;
	json_error_t error;
	json_t *root = NULL;
	json_t *servers;
	int length, i;
	GRand* rand = NULL;

	if (req->status_code != 200)
		goto error;

	if (!(root = json_loads(req->reply_body, 0, &error)))
		goto error;

	if (!(servers = json_object_get(root, "servers")))
		goto error;

	length = json_array_size(servers);
	rand = g_rand_new();
	i = g_rand_int_range(rand, 0, length);

	if (!(json_string_value(json_array_get(servers, i))))
		goto error;

	omegle_normal_start_convo(ic, who, g_strdup(json_string_value(json_array_get(servers, i))));

	json_decref(root);
	g_rand_free(rand);

	return;

error:
	if (root) json_decref(root);
	if (rand) g_rand_free(rand);

	imcb_error(ic, "Could not fetch the server list, set one to use in the config");

	bd->connecting = FALSE;
}

static void omegle_normal_disconnect_happened(struct im_connection *ic, char *who)
{
	struct bee_user *bu = bee_user_by_handle(ic->bee, ic, who);
	struct omegle_buddy_data *bd = bu->data;
	account_t *acc = ic->acc;

	if (set_getbool(&acc->set, "keep_online")) {
		if (bu->flags & BEE_USER_AWAY)
			imcb_error(ic, "Lookup for a stranger failed");

		imcb_buddy_status(ic, who, BEE_USER_ONLINE | BEE_USER_AWAY, NULL, NULL);
	} else {
		if (!(bu->flags & BEE_USER_ONLINE))
			imcb_error(ic, "Lookup for a stranger failed");

		imcb_buddy_status(ic, who, 0, NULL, NULL);
	}

	if (bd->host) {
		g_free(bd->host);
		bd->host = NULL;
	}

	if (bd->session_id) {
		g_free(bd->session_id);
		bd->session_id = NULL;
	}

	if (bd->backlog) {
		g_slist_free_full(bd->backlog, g_free);
		bd->backlog = NULL;
	}

	bd->checking = FALSE;
	bd->connecting = FALSE;
	bd->disconnecting = FALSE;
	bd->disconnected = FALSE;
}

static void omegle_normal_disconnect_happened_http(struct http_request *req)
{
	struct bee_user *bu = req->data;
	struct omegle_buddy_data *bd = bu->data;

	bd->disconnected = TRUE;
	bd->disconnecting = FALSE;
}

static void omegle_normal_disconnect(struct im_connection *ic, char *who)
{
	struct bee_user *bu = bee_user_by_handle(ic->bee, ic, who);
	struct omegle_buddy_data *bd = bu->data;

	if (bd->disconnecting)
		return;

	bd->disconnecting = TRUE;

	omegle_normal_send_with_callback(ic, who, "/disconnect", omegle_normal_disconnect_happened_http);
}

static void omegle_normal_handle_events(struct http_request *req)
{
	struct bee_user *bu = req->data;
	struct omegle_buddy_data *bd = bu->data;
	struct im_connection *ic = bu->ic;
	json_error_t error;
	json_t *root, *value;
	int length, llength, i, j;
	const char *name;
	char **likes;
	GSList *l;

	if (req->status_code == 0) {
		bd->checking = FALSE;

		return;
	}

	if (req->status_code != 200) {
		imcb_error(ic, "Got an HTTP error: %d", req->status_code);

		bd->disconnected = TRUE;
		bd->connecting = FALSE;

		return;
	}

	if (!(root = json_loads(req->reply_body, JSON_DECODE_ANY, &error))) {
		imcb_error(ic, "Could not parse JSON: %s", error.text);
		imcb_error(ic, "at %d:%d in:", error.line, error.column);
		imcb_error(ic, "%s", req->reply_body);

		bd->checking = FALSE;

		return;
	}

	if (!json_is_array(root)) {
		if (json_is_null(root))
			bd->disconnected = TRUE;

		goto end;
	}

	for (i = 0, length = json_array_size(root); i < length; i++) {
		name  = json_string_value(json_array_get(json_array_get(root, i), 0));
		value = json_array_get(json_array_get(root, i), 1);

		if (!strcmp(name, "waiting")) {
			likes = g_new0(char*, 1);

			imcb_buddy_action_response(bu, "WAITING", likes, NULL);

			g_free(likes);
		} else if (!strcmp(name, "connected")) {
			imcb_buddy_status(ic, bu->handle, BEE_USER_ONLINE, NULL, NULL);

			for (l = bd->backlog; l; l = l->next) {
				omegle_normal_send_message(ic, bu->handle, l->data);
			}

			g_slist_free_full(bd->backlog, g_free);

			bd->connecting = FALSE;
			bd->backlog = NULL;
		} else if (!strcmp(name, "commonLikes")) {
			likes = g_new0(char*, json_array_size(value) + 1);

			for (j = 0, llength = json_array_size(value); j < llength; j++) {
				likes[j] = (char*) json_string_value(json_array_get(value, j));
			}

			imcb_buddy_action_response(bu, "LIKES", likes, NULL);

			g_free(likes);
		} else if (!strcmp(name, "count")) {
			likes = g_new0(char*, 2);
			likes[0] = g_strdup_printf("%" JSON_INTEGER_FORMAT, json_integer_value(value));

			imcb_buddy_action_response(bu, "COUNT", likes, NULL);

			g_free(likes[0]);
			g_free(likes);
		} else if (!strcmp(name, "typing")) {
			imcb_buddy_typing(ic, bu->handle, OPT_TYPING);
		} else if (!strcmp(name, "stoppedTyping")) {
			imcb_buddy_typing(ic, bu->handle, 0);
		} else if (!strcmp(name, "gotMessage")) {
			imcb_buddy_typing(ic, bu->handle, 0);
			imcb_buddy_msg(ic, bu->handle, (char*) json_string_value(value), 0, 0);
		} else if (!strcmp(name, "strangerDisconnected")) {
			bd->disconnected = TRUE;
		}
	}

end:
	bd->checking = FALSE;

	json_decref(root);
}
