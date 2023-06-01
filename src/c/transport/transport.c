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

// finds the transport channel corresponding to the sender (identified by IP address and port)
// and returns the indexes of this transport channel and the redundancy channel it belongs to
void find_channel_by_ip_address(struct rasta_handle *h, struct sockaddr_in sender, int *red_channel_idx, int *transport_channel_idx){
    *red_channel_idx = -1;
    *transport_channel_idx = -1;

    char ip_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender.sin_addr, ip_addr, INET_ADDRSTRLEN);

    for (unsigned i = 0; i < h->mux.redundancy_channels_count; i++) {
        for (unsigned j = 0; j < h->mux.redundancy_channels[i].transport_channel_count; j++) {
            rasta_transport_channel *current_channel = &h->mux.redundancy_channels[i].transport_channels[j];
            if (strncmp(current_channel->remote_ip_address, ip_addr, INET_ADDRSTRLEN) == 0
                && current_channel->remote_port == ntohs(sender.sin_port)) {
                *red_channel_idx = i;
                *transport_channel_idx = j;
                break;
            }
        }
    }
}
