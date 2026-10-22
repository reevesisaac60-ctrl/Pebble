#ifndef PEBBLE_MANIFEST_BUILD_GRAPH_H
#define PEBBLE_MANIFEST_BUILD_GRAPH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *id;
    int dirty;
} PebbleBuildNode;

typedef struct {
    PebbleBuildNode *nodes;
    size_t node_count;
    size_t node_cap;
    size_t **edges;
    size_t *edge_counts;
    size_t *edge_caps;
} PebbleBuildGraph;

int pebble_build_graph_init(PebbleBuildGraph *g);
void pebble_build_graph_destroy(PebbleBuildGraph *g);

int pebble_build_graph_add_node(PebbleBuildGraph *g, const char *id);
int pebble_build_graph_add_edge(PebbleBuildGraph *g, int from, int to);

void pebble_build_graph_mark_dirty(PebbleBuildGraph *g, int node);

int pebble_build_graph_has_cycle(const PebbleBuildGraph *g);

size_t pebble_build_graph_dirty_count(const PebbleBuildGraph *g);

#ifdef __cplusplus
}
#endif

#endif
