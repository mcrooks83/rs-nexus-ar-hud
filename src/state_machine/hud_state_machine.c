#include "hud_state_machine.h"

#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>

#include "display_manager.h"

LOG_MODULE_REGISTER(hud_state_machine, LOG_LEVEL_INF);

/*
    the state machine must have a means to
    1. set the state
    2. initialise the state machine
    3. handle events
    4. transition states via button presses
    5. get the current state
    6. render the states on the display
*/

// starting point
static enum hud_state current_state = HUD_STATE_BOOTING;

static hud_send_command_fn send_command_callback;

static struct hud_session current_session;

/*
 * Render the current state.
 *
 * This function only decides what content appears on the OLED.
 * It must not send commands, reconnect, or change state.
 */
static void render_current_state(void)
{
    switch (current_state) {
    case HUD_STATE_BOOTING:
        display_manager_show_lines(
            "Nexus AR HUD",
            "Booting...",
            NULL,
            NULL
        );
        break;

    case HUD_STATE_CONNECTING:
        display_manager_show_lines(
            "Nexus AR HUD",
            "Connecting...",
            NULL,
            NULL
        );
        break;

    case HUD_STATE_CONNECTED:
        display_manager_show_lines(
            "Server connected",
            NULL,
            NULL,
            NULL
        );
        break;

    case HUD_STATE_CHECKING_SERVER_READY:
        display_manager_show_lines(
            "Server connected",
            "Checking ready...",
            NULL,
            NULL
        );
        break;

    case HUD_STATE_WAITING_FOR_SYSTEM_INITIALIZED:
        display_manager_show_lines(
            "Server ready",
            "Waiting for",
            "session...",
            NULL
        );
        break;

    case HUD_STATE_READY:
        display_manager_show_lines(
            current_session.name,
            current_session.algorithm,
            current_session.sensor_name,
            "Ready"
        );
        break;

    case HUD_STATE_START_REQUESTED:
        display_manager_show_lines(
            "Starting stream",
            "Request sent",
            NULL,
            NULL
        );
        break;

    case HUD_STATE_WARMING_UP:
        display_manager_show_lines(
            "Starting stream",
            "Warming up...",
            NULL,
            NULL
        );
        break;

    case HUD_STATE_STREAMING:
        display_manager_show_lines(
            current_session.name,
            "Streaming",
            "Waiting for data",
            NULL
        );
        break;

    case HUD_STATE_STOP_REQUESTED:
        display_manager_show_lines(
            "Stopping stream",
            "Request sent",
            NULL,
            NULL
        );
        break;

    case HUD_STATE_DRAINING:
        display_manager_show_lines(
            "Stream stopped",
            "Finalizing data",
            NULL,
            NULL
        );
        break;

    case HUD_STATE_ERROR:
        display_manager_show_lines(
            "Nexus AR HUD",
            "System error",
            NULL,
            NULL
        );
        break;

    default:
        break;
    }
}


// sets the new state
static void set_state(enum hud_state state){
    current_state = state;
    LOG_INF("HUD state changed: %d", state);

    render_current_state();
}

// initialise the state machine by passing in a send command callback 
int hud_state_machine_init(
    hud_send_command_fn send_command
)
{
    if (send_command == NULL) {
        return -EINVAL;
    }

    send_command_callback = send_command;

    // creates a current session in memory
    memset(
        &current_session,
        0,
        sizeof(current_session)
    );

    // set the starting point state (again to be sure)
    set_state(HUD_STATE_BOOTING);

    LOG_INF("HUD state machine initialized");

    return 0;
}

// handle an event
int hud_state_machine_handle_event(
    const struct hud_event *event
)
{
    if (event == NULL) {
        return -EINVAL;
    }

    switch (event->type) {
    case HUD_EVENT_CONNECTION_STARTED:
        set_state(HUD_STATE_CONNECTING);
        break;

    case HUD_EVENT_SERVER_CONNECTED:{
        set_state(HUD_STATE_CONNECTED);
        set_state(HUD_STATE_CHECKING_SERVER_READY);

        //when the sever is connected we should send the is_server_ready command
        int error = send_command_callback(
            "is_server_ready"
        );
        // if the error is non zero it failed
        if( error !=0 ){
            // log the error
            LOG_ERR(
                "Failed to request server readiness: %d",
                error
            );
            //return to the previous state to essentially try again
            set_state(HUD_STATE_CONNECTING);
            return error;
        }
        
        break;
    }
    case HUD_EVENT_SERVER_DISCONNECTED:
        set_state(HUD_STATE_CONNECTING);
        break;

    case HUD_EVENT_SERVER_READY:
        set_state(
            HUD_STATE_WAITING_FOR_SYSTEM_INITIALIZED
        );
        break;

    case HUD_EVENT_SYSTEM_INITIALIZED:
        if (event->session == NULL) {
            return -EINVAL;
        }

        memcpy(
            &current_session,
            event->session,
            sizeof(current_session)
        );

        set_state(HUD_STATE_READY);
        break;

    case HUD_EVENT_STREAM_STARTED:
    case HUD_EVENT_STREAM_WARMUP_STARTED:
        set_state(HUD_STATE_WARMING_UP);
        break;

    case HUD_EVENT_STREAM_OFFICIAL_STARTED:
        set_state(HUD_STATE_STREAMING);
        break;

    case HUD_EVENT_STREAM_STOPPED:
        set_state(HUD_STATE_DRAINING);
        break;

    case HUD_EVENT_STREAM_DRAINED:
        set_state(HUD_STATE_READY);
        break;

    case HUD_EVENT_STREAM_STARTUP_RETRY:
        LOG_WRN("Stream startup retry");
        break;

    case HUD_EVENT_STREAM_STARTUP_FAILED:
    case HUD_EVENT_ERROR:
        set_state(HUD_STATE_ERROR);
        break;

    case HUD_EVENT_COMPUTE_RESULT:
        /*
         * Compute-result storage will be added next.
         */
        break;

    default:
        return -ENOTSUP;
    }

    return 0;
}

// handle button presses
void hud_state_machine_handle_single_press(void)
{
    /*
     * Start and stop command handling will be added once
     * the dummy command sender exists.
     */
}

void hud_state_machine_handle_double_press(void)
{
    /*
     * Metric selection will be added after compute-result
     * storage is implemented.
     */
}

// get the current state
enum hud_state hud_state_machine_get_state(void)
{
    return current_state;
}