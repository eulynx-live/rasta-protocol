#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include <stdint.h>

typedef struct event_system event_system;
typedef struct rasta rasta;

// event callback pointer, return 0 to keep the loop running, everything else stops the loop
typedef int (*event_ptr)(void *h, int fd);

#define EV_READABLE (1 << 0)
#define EV_WRITABLE (1 << 1)
#define EV_EXCEPTIONAL (1 << 2)

/**
 * contains a function pointer to a callback function and interval in microseconds
 */
typedef struct timed_event {
    event_ptr callback;
    void *carry_data;
    struct timed_event *prev;
    struct timed_event *next;
    uint64_t interval;
    uint64_t last_call;
    char enabled;
} timed_event;

/**
 * contains a function pointer to a callback function and a file descriptor
 */
typedef struct fd_event {
    event_ptr callback;
    void *carry_data;
    struct fd_event *prev;
    struct fd_event *next;
    int fd;
    int options;
    char enabled;
} fd_event;

/**
 * enables a timed event, it will fire in event::interval nanoseconds
 * @param event the event to enable
 */
void enable_timed_event(timed_event *event);

/**
 * enables a fd event
 * @param event the event to enable
 */
void enable_fd_event(fd_event *event);

/**
 * disables a timed event
 * @param event the event to disable
 */
void disable_timed_event(timed_event *event);

/**
 * enables a fd event
 * @param event the event to enable
 */
void disable_fd_event(fd_event *event);

/**
 * Add a timed event to an event system.
 * A event can only be in one event system at a time.
 * (not thread safe)
 * @param ev_sys the event will be added to this event system
 * @param event the event to add
 */
void add_timed_event(event_system *ev_sys, timed_event *event);

/**
 * Removes a timed event from its event system.
 * (not thread safe)
 * @param ev_sys the event will be added to this event system
 * @param event the event to add
 */
void remove_timed_event(event_system *ev_sys, timed_event *event);

/**
 * Add a fd event to an event system.
 * A event can only be in one event system at a time.
 * (not thread safe)
 * @param ev_sys the event will be added to this event system
 * @param event the event to add
 * @param options set how the event should be triggered. (EV_READABLE | EV_WRITABLE | EV_CHANGE)
 */
void add_fd_event(event_system *ev_sys, fd_event *event, int options);

void rasta_add_fd_event(rasta *h, fd_event *event, int options);

/**
 * Removes a fd event from its event system.
 * (not thread safe)
 * @param ev_sys the event will be added to this event system
 * @param event the event to add
 */
void remove_fd_event(event_system *ev_sys, fd_event *event);

void rasta_remove_fd_event(rasta *h, fd_event *event);

#ifdef __cplusplus
}
#endif
