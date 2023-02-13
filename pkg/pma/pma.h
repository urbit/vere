#ifndef PMA_PMA_H
#define PMA_PMA_H

//==============================================================================
// TYPES
//==============================================================================

struct pma {
    void  *heap_start;

    void  *stack_start;

    int    heap_fd;

    int    stack_fd;

    size_t max_sz;
};
typedef struct pma pma_t;

//==============================================================================
// FUNCTIONS
//==============================================================================

pma_t *
pma_init(const char *heap_file, const char *stack_file);

int
pma_sync(pma_t *pma, size_t heap_sz, size_t stack_sz);

void
pma_deinit(pma_t *pma);

#endif /* ifndef PMA_PMA_H */
