#pragma once

/**
 * Representation of an element in the FIFO
 */
struct fifo_element {
    /**
     * The data
     */
    void *data;
    /**
     * The pointer to the next FIFO element.
     */
    struct fifo_element *next;
};

/**
 * Representation of a simple FIFO data structure.
 */
typedef struct {
    /**
     * The amount of elements in the FIFO
     */
    unsigned int size;
    /**
     * The maximum amount of elements in the FIFO
     */
    unsigned int max_size;
    /**
     * The first (oldest) element of the FIFO
     */
    struct fifo_element *head;
    /**
     * The last (newest) element in the FIFO
     */
    struct fifo_element *tail;
} fifo_t;

/**
 * Initializes an empty FIFO with given maximum amount of elements.
 * @param max_size maximum amount of elements in the queue
 * @return an initialized FIFO
 */
fifo_t *fifo_init(unsigned int max_size);

/**
 * Destroys the given FIFO and all elements that are still inside.
 * Note: the data of the elements is NOT freed
 * @param fifo the FIFO to free
 */
void fifo_destroy(fifo_t **fifo);

/**
 * Retrieves the first (oldest) element from the FIFO and removes it from the FIFO.
 * @param fifo the FIFO to use
 * @return the data of the first element or NULL if the FIFO is empty
 */
void *fifo_pop(fifo_t *fifo);

/**
 * Adds an element to the end of the FIFO. If the FIFO is full, nothing is done.
 * @param fifo the FIFO to use
 * @param element the data to insert
 */
int fifo_push(fifo_t *fifo, void *element);

/**
 * Gets the amount of elements in the FIFO.
 * @param fifo the FIFO to use
 * @return the amount of elements in the FIFO
 */
unsigned int fifo_get_size(fifo_t *fifo);

/**
 * Checks if the FIFO has reached its maximum capacity.
 * @param fifo the FIFO to use
 * @return 1 if the capacity has been reached, 0 otherwise
 */
int fifo_full(fifo_t *fifo);
