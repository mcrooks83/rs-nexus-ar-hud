// taken from nexus-n3-core

#ifndef NEXUS_MESSAGES_H
#define NEXUS_MESSAGES_H

/* Future wire mapping; readiness is observed, never requested. */
#define NEXUS_CMD_START_STREAM_FOR_ALL "start_stream_for_all"
#define NEXUS_CMD_STOP_STREAM_FOR_ALL "stop_stream_for_all"

#define NEXUS_EVT_SERVER_READY \
    "server_ready"

#define NEXUS_EVT_SYSTEM_INITIALIZED \
    "system_initialized"

#define NEXUS_EVT_STREAM_STARTED \
    "stream_started"

#define NEXUS_EVT_STREAM_WARMUP_STARTED \
    "stream_warmup_started"

#define NEXUS_EVT_STREAM_OFFICIAL_STARTED \
    "stream_official_started"

#define NEXUS_EVT_STREAM_STOPPED \
    "stream_stopped"

#define NEXUS_EVT_STREAM_DRAINED \
    "stream_drained"

#define NEXUS_EVT_COMPUTE_RESULT \
    "compute_result"

#define NEXUS_EVT_ERROR \
    "error"

#endif