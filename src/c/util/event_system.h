#pragma once

#include <inttypes.h>
#include <time.h>
#include <unistd.h>

#include <rasta/events.h>

struct timed_event_linked_list_s {
    timed_event *first;
    timed_event *last;
};

struct fd_event_linked_list_s {
    fd_event *first;
    fd_event *last;
};

/**
 * an event system contains timed events (firing in a given interval) and fd events (firing when a fd becomes readable/writable/exceptional)
 */
typedef struct event_system {
    struct timed_event_linked_list_s timed_events;
    struct fd_event_linked_list_s fd_events;
} event_system;

/**
 * starts an event loop with the given events
 * the events may not be removed while the loop is running, but can be modified
 * @param ev_sys contains all the events the loop should handle.
 * Can be modified from the calling thread while running.
 */
void event_system_start(event_system *ev_sys);

/**
 * reschedules the event to the current time + the event interval
 * resulting in a delay of the event
 * @param event the event to delay
 */
void reschedule_event(timed_event *event);
