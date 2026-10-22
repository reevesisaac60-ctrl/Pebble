#include "pebble_manifest/build_graph.h"

#include <stdlib.h>
#include <string.h>

int pebble_build_graph_init(PebbleBuildGraph *g) {
    if (g == NULL) return -1;
    memset(g, 0, sizeof(*g));
    return 0;
}

static int grow_nodes(PebbleBuildGraph *g) {
    if (g->node_count == g->node_cap) {
        size_t nc = g->node_cap == 0 ? 16 : g->node_cap * 2;
        PebbleBuildNode *nn = (PebbleBuildNode *)realloc(g->nodes, nc * sizeof(PebbleBuildNode));
        size_t **ne = (size_t **)realloc(g->edges, nc * sizeof(size_t *));
        size_t *nec = (size_t *)realloc(g->edge_counts, nc * sizeof(size_t));
        size_t *neca = (size_t *)realloc(g->edge_caps, nc * sizeof(size_t));
        if (!nn || !ne || !nec || !neca) return -1;
        g->nodes = nn; g->edges = ne; g->edge_counts = nec; g->edge_caps = neca;
        for (size_t i = g->node_cap; i < nc; i++) {
            g->edges[i] = NULL; g->edge_counts[i] = 0; g->edge_caps[i] = 0;
        }
        g->node_cap = nc;
    }
    return 0;
}

int pebble_build_graph_add_node(PebbleBuildGraph *g, const char *id) {
    if (g == NULL || id == NULL) return -1;
    if (grow_nodes(g) != 0) return -1;
    g->nodes[g->node_count].id = (char *)malloc(strlen(id)+1);
    if (g->nodes[g->node_count].id == NULL) return -1;
    strcpy(g->nodes[g->node_count].id, id);
    g->nodes[g->node_count].dirty = 0;
    g->node_count++;
    return (int)(g->node_count - 1);
}

static int grow_edges(PebbleBuildGraph *g, int from) {
    if (g->edge_counts[from] == g->edge_caps[from]) {
        size_t nc = g->edge_caps[from] == 0 ? 8 : g->edge_caps[from] * 2;
        size_t *tmp = (size_t *)realloc(g->edges[from], nc * sizeof(size_t));
        if (tmp == NULL) return -1;
        g->edges[from] = tmp;
        g->edge_caps[from] = nc;
    }
    return 0;
}

int pebble_build_graph_add_edge(PebbleBuildGraph *g, int from, int to) {
    if (g == NULL || from < 0 || to < 0 ||
        (size_t)from >= g->node_count || (size_t)to >= g->node_count) return -1;
    if (grow_edges(g, from) != 0) return -1;
    g->edges[from][g->edge_counts[from]++] = (size_t)to;
    return 0;
}

void pebble_build_graph_mark_dirty(PebbleBuildGraph *g, int node) {
    if (g == NULL || node < 0 || (size_t)node >= g->node_count) return;
    for (size_t i = 0; i < g->node_count; i++) {
        for (size_t j = 0; j < g->edge_counts[i]; j++) {
            if (g->edges[i][j] == (size_t)node) {
                if (!g->nodes[i].dirty) {
                    g->nodes[i].dirty = 1;
                    pebble_build_graph_mark_dirty(g, (int)i);
                }
            }
        }
    }
    g->nodes[node].dirty = 1;
}

static int dfs_cycle(const PebbleBuildGraph *g, int node, char *state) {
    if (state[node] == 1) return 1;
    if (state[node] == 2) return 0;
    state[node] = 1;
    for (size_t i = 0; i < g->edge_counts[node]; i++) {
        if (dfs_cycle(g, (int)g->edges[node][i], state)) return 1;
    }
    state[node] = 2;
    return 0;
}

int pebble_build_graph_has_cycle(const PebbleBuildGraph *g) {
    if (g == NULL) return 0;
    char *state = (char *)calloc(g->node_count, 1);
    if (state == NULL && g->node_count > 0) return 0;
    for (size_t i = 0; i < g->node_count; i++) {
        if (state[i] == 0 && dfs_cycle(g, (int)i, state)) {
            free(state);
            return 1;
        }
    }
    free(state);
    return 0;
}

size_t pebble_build_graph_dirty_count(const PebbleBuildGraph *g) {
    if (g == NULL) return 0;
    size_t n = 0;
    for (size_t i = 0; i < g->node_count; i++) if (g->nodes[i].dirty) n++;
    return n;
}

void pebble_build_graph_destroy(PebbleBuildGraph *g) {
    if (g == NULL) return;
    for (size_t i = 0; i < g->node_count; i++) free(g->nodes[i].id);
    free(g->nodes);
    for (size_t i = 0; i < g->node_cap; i++) free(g->edges[i]);
    free(g->edges);
    free(g->edge_counts);
    free(g->edge_caps);
    memset(g, 0, sizeof(*g));
}
