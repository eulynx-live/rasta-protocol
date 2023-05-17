#include "transport.h"

void transport_init_base(struct rasta_handle *h, rasta_transport_channel* channel, unsigned id, const char *host, uint16_t port, const rasta_config_tls *tls_config) {
    channel->id = id;
    channel->remote_port = port;
    strncpy(channel->remote_ip_address, host, INET_ADDRSTRLEN-1);
    channel->send_callback = send_callback;
    channel->tls_config = tls_config;

    memset(&channel->receive_event, 0, sizeof(fd_event));
    channel->receive_event.carry_data = &channel->receive_event_data;
    channel->receive_event.callback = channel_receive_event;

    memset(&channel->receive_event_data, 0, sizeof(channel->receive_event_data));
    channel->receive_event_data.h = h;
    channel->receive_event_data.channel = channel;
    channel->receive_event_data.connection = NULL;

    add_fd_event(h->ev_sys, &channel->receive_event, EV_READABLE);
}
