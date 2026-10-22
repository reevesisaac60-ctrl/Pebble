#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "pebble_manifest/resources.h"
#include "pebble_manifest/build_graph.h"

static void check(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void mkdirp(const char *p) { mkdir(p, 0777); }

int main(void) {
    char dir[] = "/tmp/pebble_res_test_XXXXXX";
    if (mkdtemp(dir) == NULL) return 1;
    char path[256];
    snprintf(path, sizeof(path), "%s/a.peb", dir);
    FILE *f = fopen(path, "w"); fputs("x", f); fclose(f);
    mkdirp(strcat(strcpy(path, dir), "/.pebble"));

    PebbleResourceList list;
    memset(&list, 0, sizeof(list));
    check(pebble_resource_scan(dir, &list) == 0,
          "the scan of the directory should succeed");
    int found = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (strcmp(list.items[i].relative_path, "a.peb") == 0) found = 1;
    }
    check(found == 1, "the scanned file should be found");
    for (size_t i = 0; i < list.count; i++) {
        check(strncmp(list.items[i].relative_path, ".pebble", 7) != 0,
              "the hidden directory should be ignored");
    }
    unsigned long long h1 = list.items[0].hash;
    pebble_resource_list_destroy(&list);

    snprintf(path, sizeof(path), "%s/a.peb", dir);
    f = fopen(path, "w");
    fputs("y", f); fclose(f);
    memset(&list, 0, sizeof(list));
    pebble_resource_scan(dir, &list);
    check(list.items[0].hash != h1,
          "the hash should change with the contents");
    pebble_resource_list_destroy(&list);

    PebbleBuildGraph g;
    pebble_build_graph_init(&g);
    int a = pebble_build_graph_add_node(&g, "a");
    int b = pebble_build_graph_add_node(&g, "b");
    pebble_build_graph_add_edge(&g, a, b);
    pebble_build_graph_add_edge(&g, b, a);
    check(pebble_build_graph_has_cycle(&g) == 1,
          "the graph should report the cycle");
    pebble_build_graph_destroy(&g);

    PebbleBuildGraph g2;
    pebble_build_graph_init(&g2);
    int x = pebble_build_graph_add_node(&g2, "x");
    int y = pebble_build_graph_add_node(&g2, "y");
    pebble_build_graph_add_edge(&g2, y, x);
    pebble_build_graph_mark_dirty(&g2, x);
    check(g2.nodes[y].dirty == 1,
          "the change should mark the dependent node dirty");
    pebble_build_graph_destroy(&g2);

    snprintf(path, sizeof(path), "rm -rf %s", dir);
    system(path);
    printf("resources tests passed\n");
    return 0;
}
