/* cmd.c */
#include "cmd.h"
#include "conf.h"
#include "client.h"
#include "worker.h"
#include "http.h"
#include "request.h"
#include "server.h"
#include "slog.h"
#include "embed.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct cmd *cmd_new() {
	struct cmd *c = calloc(1, sizeof(struct cmd));

	return c;
}


void cmd_free(struct cmd *c) {
	if(!c) return;

	if(c->mime_free) free(c->mime);

	free(c);
}

/* taken from libevent */
static char *decode_uri(const char *uri, size_t length, size_t *out_len, int always_decode_plus) {
	char c;
	size_t i, j;
	int in_query = always_decode_plus;

	char *ret = malloc(length);

	for (i = j = 0; i < length; i++) {
		c = uri[i];
		if (c == '?') {
			in_query = 1;
		} else if (c == '+' && in_query) {
			c = ' ';
		} else if (c == '%' && isxdigit((unsigned char)uri[i+1]) &&
		    isxdigit((unsigned char)uri[i+2])) {
			char tmp[] = { uri[i+1], uri[i+2], '\0' };
			c = (char)strtol(tmp, NULL, 16);
			i += 2;
		}
		ret[j++] = c;
	}
	*out_len = (size_t)j;

	return ret;
}

/* setup headers */
void cmd_setup(struct cmd *cmd, struct http_client *client) {
	int i;
	cmd->keep_alive = client->keep_alive;
	cmd->w = client->w; /* keep track of the worker */

	for(i = 0; i < client->header_count; ++i) {
		if(strcasecmp(client->headers[i].key, "Connection") == 0 &&
				strcasecmp(client->headers[i].val, "Keep-Alive") == 0) {
			cmd->keep_alive = 1;
		}
	}

	cmd->fd = client->fd;
	cmd->http_version = client->http_version;
}

cmd_response_t cmd_run(struct worker *w, struct http_client *client,
		const char *uri, size_t uri_len,
		const char *body, size_t body_len) {

    (void) w;
    (void) uri;
    (void) uri_len;
    (void) body;
    (void) body_len;

    struct cmd *cmd;
	struct http_response *resp;

	size_t fpath_len;
	char *fpath;

	fpath_len = uri_len + strlen(client->sc->root);
	fpath = malloc((fpath_len + 1) * sizeof(char));
	memcpy(fpath, client->sc->root, strlen(client->sc->root));
	memcpy(fpath + strlen(client->sc->root), uri, uri_len);
	fpath[fpath_len] = 0;

	slog(w->s, LOG_DEBUG, fpath, fpath_len);

    cmd = cmd_new();
	cmd->fd = client->fd;

	/* add HTTP info */
	cmd_setup(cmd, client);

	resp = http_response_init(cmd->w, 200, "OK");
	resp->http_version = cmd->http_version;

	struct server_request *request;
	request = malloc(sizeof(request));
	request->client = client;
	request->resp = resp;

	int ret = embed_execute(fpath, request);
	(void) ret;

	/* http_response_set_keep_alive(resp, cmd->keep_alive); */
	http_response_write(resp, client->fd);

	free(request);
	free(fpath);
    cmd_free(cmd);

    return CMD_SENT;
}

void cmd_send(struct cmd *cmd) {
    (void) cmd;
}
