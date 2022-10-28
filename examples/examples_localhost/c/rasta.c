//
// Created by tobia on 24.02.2018.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <rasta_lib.h>
#include <rmemory.h>
#include <fifo.h>

#define CONFIG_PATH_S "rasta_server_local.cfg"
#define CONFIG_PATH_C1 "rasta_client1_local.cfg"
#define CONFIG_PATH_C2 "rasta_client2_local.cfg"

#define ID_R 0x61
#define ID_S1 0x62
#define ID_S2 0x63

void printHelpAndExit(void){
    printf("Invalid Arguments!\n use 'r' to start in receiver mode and 's1' or 's2' to start in sender mode.\n");
    exit(1);
}

void addRastaString(struct RastaMessageData * data, int pos, char * str) {
    int size =  strlen(str) + 1;

    struct RastaByteArray msg ;
    allocateRastaByteArray(&msg, size);
    rmemcpy(msg.bytes, str, size);

    data->data_array[pos] = msg;
}

int client1 = 1;
int client2 = 1;

// static rastaApplicationMessage received_packet;
fifo_t *server_fifo;

void send_pending_messages(struct rasta_handle *h) {
    while (server_fifo->size) {
        rastaApplicationMessage *oldestMessage = server_fifo->head->data;

        struct rasta_connection* con;
        int message_forwarded = 0;
        for (con = h->first_con; con; con = con->linkedlist_next) {
            if (con->remote_id != oldestMessage->id) {
                 printf("Client message from %lu is now sent to %lu\n", oldestMessage->id, (long unsigned int) con->remote_id);

                struct RastaMessageData messageData1;
                allocateRastaMessageData(&messageData1, 1);

                addRastaString(&messageData1, 0, (char*)oldestMessage->appMessage.bytes);
                sr_send(h, con->remote_id, messageData1);
                freeRastaMessageData(&messageData1);

                printf("Message forwarded\n");
                // printf("Disconnect to client %lu \n", (long unsigned int) result->connection.remote_id);
                // sr_disconnect(result->handle, &result->connection);
                message_forwarded = 1;
            }
        }

        if (message_forwarded) {
            rfree(fifo_pop(server_fifo));
        } else {
            break;
        }
    }
}

void onConnectionStateChange(struct rasta_notification_result *result) {
    printf("Connection state change (remote: %u)\n", result->connection.remote_id);

    switch (result->connection.current_state) {
        case RASTA_CONNECTION_CLOSED:
            printf("CONNECTION_CLOSED\n");
            break;
        case RASTA_CONNECTION_START:
            printf("CONNECTION_START\n");
            break;
        case RASTA_CONNECTION_DOWN:
            printf("CONNECTION_DOWN\n");
            break;
        case RASTA_CONNECTION_UP:
            printf("CONNECTION_UP\n");
            //send data to server
            if (result->connection.my_id == ID_S1) { //Client 1
                struct RastaMessageData messageData1;
                allocateRastaMessageData(&messageData1, 1);

                // messageData1.data_array[0] = msg1;
                // messageData1.data_array[1] = msg2;
                addRastaString(&messageData1, 0, "Message from Sender 1");

                //send data to server
                sr_send(result->handle,ID_R, messageData1);

                //freeRastaMessageData(&messageData1);
            } else if (result->connection.my_id == ID_S2) { //Client 2
                struct RastaMessageData messageData1;
                allocateRastaMessageData(&messageData1, 1);

                // messageData1.data_array[0] = msg1;
                // messageData1.data_array[1] = msg2;
                addRastaString(&messageData1, 0, "Message from Sender 2");

                //send data to server
                sr_send(result->handle,ID_R, messageData1);

                //freeRastaMessageData(&messageData1);
            }
            else if (result->connection.my_id == ID_R) {
                if (result->connection.remote_id == ID_S1) client1 = 0;
                else if (result->connection.remote_id == ID_S2) client2 = 0;
                send_pending_messages(result->handle);
            }

            break;
        case RASTA_CONNECTION_RETRREQ:
            printf("CONNECTION_RETRREQ\n");
            break;
        case RASTA_CONNECTION_RETRRUN:
            printf("CONNECTION_RETRRUN\n");
            break;
        default:
            break;
    }

}

void onHandshakeCompleted(struct rasta_notification_result *result){
    printf("Handshake complete, tls_state is now UP (with ID 0x%X)\n", result->connection.remote_id);
}

void onTimeout(struct rasta_notification_result *result){
    printf("Entity 0x%X had a heartbeat timeout!\n", result->connection.remote_id);
}

void onReceive(struct rasta_notification_result *result) {
    rastaApplicationMessage p;

    switch (result->connection.my_id) {
        case ID_R:
            // Server
            printf("Received data from Client %u\n", result->connection.remote_id);

            p = sr_get_received_data(result->handle, &result->connection);

            printf("Packet is from %lu\n", p.id);
            printf("Msg: %s\n", p.appMessage.bytes);

            void* relayed_message = rmalloc(sizeof(rastaApplicationMessage));
            memcpy(relayed_message, &p, sizeof(rastaApplicationMessage));
            if (!fifo_push(server_fifo, relayed_message)) {
                rfree(relayed_message);
            }

            send_pending_messages(result->handle);

            break;
        case ID_S1: case ID_S2:
            printf("Received data from Server %u\n", result->connection.remote_id);

            p = sr_get_received_data(result->handle,&result->connection);

            printf("Packet is from %lu\n", p.id);
            printf("Msg: %s\n", p.appMessage.bytes);
            break;
        default:
            break;
    }
}

struct connect_event_data {
    struct rasta_handle * h;
    struct RastaIPData * ip_data_arr;
    timed_event * connect_event;
    timed_event * schwarzenegger;
};

int connect_timed(void* carry_data) {
    printf("->   Connection request sent to 0x%lX\n", (unsigned long)ID_R);
    struct connect_event_data* data = carry_data;
    sr_connect(data->h, ID_R, data->ip_data_arr);
    enable_timed_event(data->schwarzenegger);
    disable_timed_event(data->connect_event);
    return 0;
}

static bool test_success = false;

int terminator(void* h) {
    struct rasta_handle *handle = (struct rasta_handle*) h;
    printf("terminating\n");
    if (!handle->last_con) {
            printf("Test failure - no last connection!\n");
    } else if (handle->last_con->role == RASTA_ROLE_SERVER) {
        // server checks for success and tears down, client relies on server
        if (handle->last_con->current_state != RASTA_CONNECTION_UP) {
            printf("Test failure - last connection state was not UP: %u\n", handle->last_con->current_state);
        } else {
            test_success = true;
            printf("Test success!\n");
        }
    }
    else{
        printf("Check server log for test status!\n");
        test_success = true;
    }
    sr_cleanup(h);
    return 1;
}

void* on_con_start(rasta_lib_connection_t connection) {
    (void) connection;
    return malloc(sizeof(rasta_lib_connection_t));
}

void on_con_end(rasta_lib_connection_t connection, void* memory) {
    (void) connection;
    free(memory);
}

int main(int argc, char *argv[]){

    if (argc != 2) printHelpAndExit();

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
            .connect_event = &connect_on_timeout_event
    };

    termination_event.callback = terminator;
    termination_event.carry_data = &rc->h;
    termination_event.interval = 30000000000ul;

    connect_on_timeout_event.callback = connect_timed;
    connect_on_timeout_event.carry_data = &connect_on_stdin_event_data;
    connect_on_timeout_event.interval = 3000000000ul;

    if (strcmp(argv[1], "r") == 0) {
        printf("->   R (ID = 0x%lX)\n", (unsigned long)ID_R);
        rasta_lib_init_configuration(rc, CONFIG_PATH_S);
        rc->h.user_handles->on_connection_start = on_con_start;
        rc->h.user_handles->on_disconnect = on_con_end;


        server_fifo = fifo_init(128);

        rc->h.notifications.on_connection_state_change = onConnectionStateChange;
        rc->h.notifications.on_receive = onReceive;
        rc->h.notifications.on_handshake_complete = onHandshakeCompleted;
        rc->h.notifications.on_heartbeat_timeout = onTimeout;

        enable_timed_event(&termination_event);
        disable_timed_event(&connect_on_timeout_event);
        add_timed_event(&rc->rasta_lib_event_system, &termination_event);
        rasta_lib_start(rc, 0);

        fifo_destroy(server_fifo);
    }
    else if (strcmp(argv[1], "s1") == 0) {
        printf("->   S1 (ID = 0x%lX)\n", (unsigned long)ID_S1);
        rasta_lib_init_configuration(rc, CONFIG_PATH_C1);
        rc->h.user_handles->on_connection_start = on_con_start;
        rc->h.user_handles->on_disconnect = on_con_end;

        rc->h.notifications.on_connection_state_change = onConnectionStateChange;
        rc->h.notifications.on_receive = onReceive;
        rc->h.notifications.on_handshake_complete = onHandshakeCompleted;
        enable_timed_event(&termination_event);
        enable_timed_event(&connect_on_timeout_event);
        add_timed_event(&rc->rasta_lib_event_system, &termination_event);
        add_timed_event(&rc->rasta_lib_event_system, &connect_on_timeout_event);
        rasta_lib_start(rc, 0);
    }
    else if (strcmp(argv[1], "s2") == 0) {
        printf("->   S2 (ID = 0x%lX)\n", (unsigned long)ID_S2);
        rasta_lib_init_configuration(rc, CONFIG_PATH_C2);
        rc->h.user_handles->on_connection_start = on_con_start;
        rc->h.user_handles->on_disconnect = on_con_end;

        rc->h.notifications.on_connection_state_change = onConnectionStateChange;
        rc->h.notifications.on_receive = onReceive;
        rc->h.notifications.on_handshake_complete = onHandshakeCompleted;
        enable_timed_event(&termination_event);
        enable_timed_event(&connect_on_timeout_event);
        add_timed_event(&rc->rasta_lib_event_system, &termination_event);
        add_timed_event(&rc->rasta_lib_event_system, &connect_on_timeout_event);
        rasta_lib_start(rc, 0);
    }
    return test_success != true;
}

