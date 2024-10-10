// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;

void process_graph(void *arg);
static void process_node(unsigned int idx);

/* DONE: Define graph synchronization mechanisms. */
pthread_mutex_t mutex;

/* DONE: Define graph task argument. */
void process_graph(void *arg)
{
	unsigned int idx = *(unsigned int *)arg;

	/* locking the mutex to make sure that only one node is processed a time */
	pthread_mutex_lock(&mutex);

	/* if the node has already been visited, then there's
	 * no need to check the neighbours of this node
	 */
	if (graph->visited[idx] != NOT_VISITED) {
		pthread_mutex_unlock(&mutex);
		return;
	}

	/* mark the node as DONE so that other threads won't process it again */
	graph->visited[idx] = DONE;

	os_node_t *current = graph->nodes[idx];

	/* adding to the sum the value of the current node */
	sum += current->info;

	/* proccessing the neighbours */
	for (unsigned int i = 0; i < current->num_neighbours; i++)
		process_node(current->neighbours[i]);

	pthread_mutex_unlock(&mutex);
}

static void process_node(unsigned int idx)
{
	/* DONE: Implement thread-pool based processing of graph. */

	/* allocating memory for the argument of the create_task function */
	unsigned int *pointer_idx = malloc(sizeof(unsigned int));

	DIE(!pointer_idx, "malloc\n");
	*pointer_idx = idx;

	os_task_t *task = create_task(process_graph, pointer_idx, NULL);

	DIE(!task, "create_task failed\n");

	enqueue_task(tp, task);
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* DONE: Initialize graph synchronization mechanisms. */
	pthread_mutex_init(&mutex, NULL);

	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);
	pthread_mutex_destroy(&mutex);
	printf("%d", sum);

	return 0;
}
