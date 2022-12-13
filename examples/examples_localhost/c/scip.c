#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <rmemory.h>
#include <rasta_new.h>
#include <scip.h>
#include <scils.h>
#include <rasta_lib.h>

#define CONFIG_PATH_S "../../../rasta_server_local.cfg"
#define CONFIG_PATH_C "../../../rasta_client1_local.cfg"

#define ID_S 0x61
#define ID_C 0x62

#define SCI_NAME_S "S"
#define SCI_NAME_C "C"

scip_t *scip;

void printHelpAndExit(void)
{
    printf("Invalid Arguments!\n use 's' to start in server mode and 'c' client mode.\n");
    exit(1);
}

void onReceive(struct rasta_notification_result *result)
{
    rastaApplicationMessage message = sr_get_received_data(result->handle, &result->connection);
    scip_on_rasta_receive(scip, message);
}

void onHandshakeComplete(struct rasta_notification_result *result)
{
    if (result->connection.my_id == ID_C)
    {

        printf("Sending change location command...\n");
        sci_return_code code = scip_send_change_location(scip, SCI_NAME_S, POINT_LOCATION_CHANGE_TO_RIGHT);
        if (code == SUCCESS)
        {
            printf("Sent change location command to server\n");
        }
        else
        {
            printf("Something went wrong, error code 0x%02X was returned!\n", code);
        }
    }
}

void onChangeLocation(scip_t *p, char *sender, scip_point_target_location location)
{
    printf("Received location change to 0x%02X from %s\n", location, sci_get_name_string(sender));

    printf("Sending back location status...\n");
    sci_return_code code = scip_send_location_status(p, sender, POINT_LOCATION_RIGHT);
    if (code == SUCCESS)
    {
        printf("Sent location status\n");
    }
    else
    {
        printf("Something went wrong, error code 0x%02X was returned!\n", code);
    }
}

void onLocationStatus(scip_t *p, char *sender, scip_point_location location)
{
    (void)p;
    printf("Received location status from %s. Point is at position 0x%02X.\n", sci_get_name_string(sender), location);
}

void *on_con_start(rasta_lib_connection_t connection)
{
    (void)connection;
    return malloc(sizeof(rasta_lib_connection_t));
}

void on_con_end(rasta_lib_connection_t connection, void *memory)
{
    (void)connection;
    free(memory);
}

struct connect_event_data
{
    struct rasta_handle *h;
    struct RastaIPData *ip_data_arr;
    timed_event *connect_event;
    timed_event *schwarzenegger;
};

int connect_timed(void *carry_data)
{
    printf("->   Connection request sent to 0x%lX\n", (unsigned long)ID_S);
    struct connect_event_data *data = carry_data;
    sr_connect(data->h, ID_S, data->ip_data_arr);
    enable_timed_event(data->schwarzenegger);
    disable_timed_event(data->connect_event);
    return 0;
}

int terminator(void *h)
{
    struct rasta_handle *handle = (struct rasta_handle *)h;
    printf("terminating\n");
    sr_cleanup(h);
    return 1;
}

int main(int argc, char *argv[])
{

    if (argc != 2)
        printHelpAndExit();

    rasta_lib_configuration_t rc;

    struct RastaIPData toServer[2];

    strcpy(toServer[0].ip, "127.0.0.1");
    strcpy(toServer[1].ip, "127.0.0.1");
    toServer[0].port = 8888;
    toServer[1].port = 8889;

    timed_event termination_event, connect_on_timeout_event;
    struct connect_event_data connect_on_stdin_event_data = {
        .h = &rc->h,
        .ip_data_arr = toServer,
        .schwarzenegger = &termination_event,
        .connect_event = &connect_on_timeout_event};

    termination_event.callback = terminator;
    termination_event.carry_data = &rc->h;
    termination_event.interval = 30000000000ul;

    connect_on_timeout_event.callback = connect_timed;
    connect_on_timeout_event.carry_data = &connect_on_stdin_event_data;
    connect_on_timeout_event.interval = 3000000000ul;

    if (strcmp(argv[1], "s") == 0)
    {
        printf("->   S (ID = 0x%lX)\n", (unsigned long)ID_S);

        getchar();
        // sr_init_handle(&h, CONFIG_PATH_S);

        rasta_lib_init_configuration(rc, CONFIG_PATH_S);
        rc->h.notifications.on_receive = onReceive;
        rc->h.notifications.on_handshake_complete = onHandshakeComplete;
        scip = scip_init(&rc->h, SCI_NAME_S);
        scip->notifications.on_change_location_received = onChangeLocation;

        rc->h.user_handles->on_connection_start = on_con_start;
        rc->h.user_handles->on_disconnect = on_con_end;

        enable_timed_event(&termination_event);
        disable_timed_event(&connect_on_timeout_event);
        add_timed_event(&rc->rasta_lib_event_system, &termination_event);
        rasta_lib_start(rc, 0);
    }
    else if (strcmp(argv[1], "c") == 0)
    {
        printf("->   C (ID = 0x%lX)\n", (unsigned long)ID_C);

        rasta_lib_init_configuration(rc, CONFIG_PATH_C);
        rc->h.notifications.on_receive = onReceive;
        rc->h.notifications.on_handshake_complete = onHandshakeComplete;

        rc->h.user_handles->on_connection_start = on_con_start;
        rc->h.user_handles->on_disconnect = on_con_end;

        printf("->   Press Enter to connect\n");
        getchar();

        enable_timed_event(&termination_event);
        enable_timed_event(&connect_on_timeout_event);
        add_timed_event(&rc->rasta_lib_event_system, &termination_event);
        add_timed_event(&rc->rasta_lib_event_system, &connect_on_timeout_event);

        // sr_connect(&h, ID_S, toServer);

        scip = scip_init(&rc->h, SCI_NAME_C);
        scip->notifications.on_location_status_received = onLocationStatus;

        scip_register_sci_name(scip, SCI_NAME_S, ID_S);
        rasta_lib_start(rc, 0);
    }

    getchar();

    scip_cleanup(scip);
    sr_cleanup(&rc->h);
}
