/*
 *  omegle.c - Omegle plugin for BitlBee
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

#include <bitlbee/config.h>
#include <bitlbee/bitlbee.h>
#include <bitlbee/http_client.h>
#include <jansson.h>

GSList *omegle_connections = NULL;

struct omegle_data {
	struct im_connection *ic;

	gint main_loop_id;
};

struct omegle_buddy_data {
	char* host;
	char* session_id;
	gboolean checking;
	gboolean connecting;
	gboolean disconnecting;
	gboolean disconnected;
	GSList *backlog;
	GSList *likes;
};

struct omegle_groupchat_data {
	char *host;
	char *session_id;
};

#include "omegle_http.c"
#include "omegle_normal.c"
#include "omegle_spy.c"

static int omegle_send_typing(struct im_connection *ic, char *who, int typing)
{
	if (typing & OPT_TYPING) {
		omegle_normal_send(ic, who, "/typing");
	} else {
		omegle_normal_send(ic, who, "/stoppedTyping");
	}

	return 1;
}

static void omegle_add_deny(struct im_connection *ic, char *who)
{
}

static void omegle_rem_deny(struct im_connection *ic, char *who)
{
}

static void omegle_add_permit(struct im_connection *ic, char *who)
{
	struct bee_user *bu = bee_user_by_handle(ic->bee, ic, who);
	struct omegle_buddy_data *bd = bu->data;
	account_t *acc = ic->acc;
	char *host = set_getstr(&acc->set, "host");

	if (bd->connecting || bd->session_id)
		return;

	bd->connecting = TRUE;

	if (host) {
		omegle_normal_start_convo(ic, who, g_strdup(host));
	}
	else {
		omegle_get(bu, "omegle.com", "/status", omegle_normal_chose_server);
	}
}

static void omegle_rem_permit(struct im_connection *ic, char *who)
{
	omegle_normal_disconnect(ic, who);
}

static void omegle_buddy_data_add(bee_user_t *bu)
{
	bu->data = g_new0(struct omegle_buddy_data, 1);
}

static void omegle_buddy_data_free(bee_user_t *bu)
{
	struct omegle_buddy_data *bd = bu->data;

	if (bd->host)
		g_free(bd->host);

	if (bd->session_id)
		g_free(bd->session_id);

	if (bd->backlog)
		g_slist_free_full(bd->backlog, g_free);

	g_free(bd);
}

GList *omegle_buddy_action_list(bee_user_t *bu)
{
	static GList *ret = NULL;
	
	if (ret == NULL) {
		static const struct buddy_action ba[] = {
			{ "CONNECT", "Connect to a Stranger" },
			{ "DISCONNECT", "Disconnect from the Stranger" },
			{ "LIKES", "Specify what the common topics to look for" },
			{ "RANDOM", "Stop looking for common interests" },
			{ NULL, NULL }
		};
		
		ret = g_list_prepend(ret, (void*) ba + 0);
	}
	
	return ret;
}

static void *omegle_buddy_action(struct bee_user *bu, const char *action, char * const args[], void *data)
{
	struct omegle_buddy_data *bd = bu->data;
	char **arg = (char**) args;
	char **likes;
	GSList *l;
	int i;

	if (!strcmp(action, "CONNECT")) {
		omegle_add_permit(bu->ic, bu->handle);
	} else if (!strcmp(action, "DISCONNECT")) {
		omegle_rem_permit(bu->ic, bu->handle);
	} else if (!strcmp(action, "LIKES")) {
		if (*arg && !strcmp(*arg, "?")) {
			if (!bd->likes)
				return NULL;

			likes = g_new0(char*, g_slist_length(bd->likes) + 1);

			for (i = 0, l = bd->likes; l; l = l->next, i++)
				likes[i] = l->data;

			imcb_buddy_action_response(bu, "LIKES", likes, NULL);

			g_free(likes);
		} else {
			if (bd->likes) {
				g_slist_free_full(bd->likes, g_free);

				bd->likes = NULL;
			}

			if (*arg) {
				do {
					bd->likes = g_slist_append(bd->likes, g_strdup(*arg));
				} while (*++arg);
			}
		}
	} else if (!strcmp(action, "RANDOM")) {
		if (bd->session_id)
			omegle_normal_send(bu->ic, bu->handle, "/stoplookingforcommonlikes");
	}

	return NULL;
}

static void omegle_remove_buddy(struct im_connection *ic, char *who, char *group)
{
	imcb_remove_buddy(ic, who, NULL);
}

static void omegle_add_buddy(struct im_connection *ic, char *who, char *group)
{
	account_t *acc = ic->acc;

	imcb_add_buddy(ic, who, NULL);

	if (set_getbool(&acc->set, "keep_online"))
		imcb_buddy_status(ic, who, BEE_USER_ONLINE | BEE_USER_AWAY, NULL, NULL);
}

static int omegle_buddy_msg(struct im_connection *ic, char *who, char *message, int flags)
{
	struct bee_user *bu = bee_user_by_handle(ic->bee, ic, who);
	struct omegle_buddy_data *bd = bu->data;

	if (!bd->session_id) {
		bd->backlog = g_slist_append(bd->backlog, g_strdup(message));

		omegle_add_permit(ic, who);
	} else {
		omegle_normal_send_message(ic, who, message);
	}

	return 1;
}

static void omegle_logout(struct im_connection *ic)
{
	struct omegle_data *od = ic->proto_data;

	if (od)
		g_free(od);
	
	ic->proto_data = NULL;

	omegle_connections = g_slist_remove(omegle_connections, ic);
}

static void omegle_init(account_t *acc)
{
	set_t *s;

	s = set_add(&acc->set, "host", NULL, set_eval_account, acc);

	s = set_add(&acc->set, "keep_online", "false", set_eval_bool, acc);

	s = set_add(&acc->set, "auto_add_strangers", "1", set_eval_int, acc);
	s->flags |= ACC_SET_OFFLINE_ONLY;

	s = set_add(&acc->set, "stranger_prefix", "Stranger", set_eval_account, acc);
	s->flags |= ACC_SET_OFFLINE_ONLY;
}

gboolean omegle_main_loop(gpointer data, gint fd, b_input_condition cond)
{
	struct im_connection *ic = data;
	account_t *acc = ic->acc;
	struct bee_user *bu;
	struct omegle_buddy_data *bd;
	struct groupchat *gc;
	struct omegle_groupchat_data *gd;
	int number, i;
	char *name, *prefix;
	GSList *to_disconnect = NULL, *l;

	// Check if we are still logged in...
	if (!g_slist_find(omegle_connections, ic))
		return FALSE;

	if (!(ic->flags & OPT_LOGGED_IN)) {
		imcb_connected(ic);

		prefix = set_getstr(&acc->set, "stranger_prefix");
		number = set_getint(&acc->set, "auto_add_strangers");

		for (i = 0; i < number; i++) {
			if (i == 0)
				name = g_strdup(prefix);
			else
				name = g_strdup_printf("%s%d", prefix, i);

			imcb_add_buddy(ic, name, NULL);

			if (set_getbool(&acc->set, "keep_online"))
				imcb_buddy_status(ic, name, BEE_USER_ONLINE | BEE_USER_AWAY, NULL, NULL);

			g_free(name);
		}
	}

	for (l = ic->bee->users; l; l = l->next) {
		bu = l->data;

		if (!bu || bu->ic != ic)
			continue;

		bd = bu->data;

		if (!bd)
			continue;

		if (bd->disconnected) {
			to_disconnect = g_slist_append(to_disconnect, bu);

			continue;
		}

		if (bd->checking || !bd->session_id || bd->disconnecting)
			continue;

		bd->checking = TRUE;

		omegle_normal_send_with_callback(ic, bu->handle, "/events", omegle_normal_handle_events);
	}

	for (l = to_disconnect; l; l = l->next) {
		bu = l->data;
		bd = bu->data;

		omegle_normal_disconnect_happened(bu->ic, bu->handle);
	}

	g_slist_free(to_disconnect);

	for (l = ic->groupchats; l; l = l->next) {
		gc = l->data;
		gd = gc->data;

//		omegle_send_with_callback(ic, gc, "/events", omegle_handle_spy_events);
	}

	return ic->flags & OPT_LOGGED_IN;
}

static void omegle_login(account_t *acc)
{
	struct im_connection *ic = imcb_new(acc);
	struct omegle_data *od = g_new0(struct omegle_data, 1);

	ic->proto_data = od;
	od->ic = ic;

	imcb_log(ic, "Connecting");
	omegle_connections = g_slist_append(omegle_connections, ic);

	// this is not an option because events are long polled on the omegle's side
	// so we don't really send a request every second
	od->main_loop_id = b_timeout_add(1000, omegle_main_loop, ic);
}

void init_plugin(void)
{
	struct prpl *ret = g_new0(struct prpl, 1);

	ret->name = "omegle";
	ret->login = omegle_login;
	ret->init = omegle_init;
	ret->logout = omegle_logout;
	ret->buddy_msg = omegle_buddy_msg;
	ret->handle_cmp = g_strcasecmp;
	ret->add_buddy = omegle_add_buddy;
	ret->remove_buddy = omegle_remove_buddy;
	ret->buddy_action = omegle_buddy_action;
	ret->buddy_action_list = omegle_buddy_action_list;
	ret->buddy_data_add = omegle_buddy_data_add;
	ret->buddy_data_free = omegle_buddy_data_free;
	ret->add_permit = omegle_add_permit;
	ret->rem_permit = omegle_rem_permit;
	ret->add_deny = omegle_add_deny;
	ret->rem_deny = omegle_rem_deny;
	ret->send_typing = omegle_send_typing;

	register_protocol(ret);
}
