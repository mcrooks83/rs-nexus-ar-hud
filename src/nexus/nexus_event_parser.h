#ifndef NEXUS_EVENT_PARSER_H
#define NEXUS_EVENT_PARSER_H
#include "hud_state_machine.h"
#include <stddef.h>
#define NEXUS_MESSAGE_MAX 8192
#define NEXUS_TOKEN_MAX 768
struct nexus_token { unsigned short start, end, next, count; char kind; };
struct nexus_parser {
    struct nexus_token tokens[NEXUS_TOKEN_MAX];
    const char *json;
    size_t length, pos;
    unsigned used;
};
/* 1 = owned event, 0 = ignored event, negative errno = rejected message.
 * Workspace is owned by the receive thread and never stored in queued data. */
int nexus_event_parse(struct nexus_parser *parser, const char *json, size_t length,
                      struct hud_event *event);
#endif
