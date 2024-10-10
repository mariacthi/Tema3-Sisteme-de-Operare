// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

/* Create a task that would be executed by a thread. */
os_task_t *create_task(void (*action)(void *), void *arg, void (*destroy_arg)(void *))
{
	os_task_t *t;

	t = malloc(sizeof(*t));
	DIE(t == NULL, "malloc");

	t->action = action;		// the function
	t->argument = arg;		// arguments for the function
	t->destroy_arg = destroy_arg;	// destroy argument function

	return t;
}

/* Destroy task. */
void destroy_task(os_task_t *t)
{
	if (t->destroy_arg != NULL)
		t->destroy_arg(t->argument);
	free(t);
}

/*
 * Check if queue is empty.
 * This function should be called in a synchronized manner.
 */
static int queue_is_empty(os_threadpool_t *tp)
{
	return list_empty(&tp->head);
}

/* Put a new task to threadpool task queue. */
void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
	assert(tp != NULL);
	assert(t != NULL);

	/* DONE: Enqueue task to the shared task queue. Use synchronization. */

	/* locking the mutex to make sure that only one thread adds one
	 * task in the queue at a time
	 */
	pthread_mutex_lock(&(tp->mutex));

	/* activating the condition variable associated with the queue
	 * of tasks
	 */
	if (queue_is_empty(tp))
		pthread_cond_signal(&(tp->cond_queue));

	/* adding the task in the queue */
	list_add(&(tp->head), &(t->list));

	pthread_mutex_unlock(&(tp->mutex));
}

/*
 * Get a task from threadpool task queue.
 * Block if no task is available.
 * Return NULL if work is complete, i.e. no task will become available,
 * i.e. all threads are going to block.
 */

os_task_t *dequeue_task(os_threadpool_t *tp)
{
	/* initializing with NULL */
	os_task_t *t = NULL;

	/* DONE: Dequeue task from the shared task queue. Use synchronization. */

	/* locking the mutex so that only one thread has access
	 * to the queue at a time
	 */
	pthread_mutex_lock(&(tp->mutex));

	/* if the number of  waiting threads == N - 1, then the work
	 * is done and this is the last thread, so the stop_work flag
	 * is activated and the signal is broadcasted for the queue variable
	 */
	if (tp->waiting_threads == tp->num_threads - 1) {
		tp->stop_work = 1;
		pthread_cond_broadcast(&(tp->cond_queue));
	}

	tp->waiting_threads++;

	while (queue_is_empty(tp) && !tp->stop_work) {
		/* waiting for the signal until the queue is not empty anymore
		 * or the stop_work flag is activated
		 */
		pthread_cond_wait(&(tp->cond_queue), &(tp->mutex));
	}

	if (!queue_is_empty(tp)) {
		/* dequeueing task */
		t = list_entry(tp->head.prev, os_task_t, list);
		list_del(tp->head.prev);

		/* signaling that the work of this thread is done */
		pthread_cond_signal(&(tp->work_done));
	}

	/* decrementing the number of waiting threads after it get out of
	 * the loop and dequeues a task
	 */
	tp->waiting_threads--;

	pthread_mutex_unlock(&(tp->mutex));

	return t;
}

/* Loop function for threads */
static void *thread_loop_function(void *arg)
{
	os_threadpool_t *tp = (os_threadpool_t *) arg;

	while (1) {
		os_task_t *t;

		t = dequeue_task(tp);
		if (t == NULL)
			break;
		t->action(t->argument);
		destroy_task(t);
	}

	return NULL;
}

/* Wait completion of all threads. This is to be called by the main thread. */
void wait_for_completion(os_threadpool_t *tp)
{
	/* DONE: Wait for all worker threads. Use synchronization. */

	pthread_mutex_lock(&(tp->mutex));

	while (!(queue_is_empty(tp)) && !tp->stop_work) {
		/* waiting for all of the threads to be signaled the
		 * work_done condition
		 */
		pthread_cond_wait(&(tp->work_done), &(tp->mutex));
	}

	pthread_mutex_unlock(&(tp->mutex));

	/* Join all worker threads. */
	for (unsigned int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
}

/* Create a new threadpool. */
os_threadpool_t *create_threadpool(unsigned int num_threads)
{
	os_threadpool_t *tp = NULL;
	int rc;

	tp = malloc(sizeof(*tp));
	DIE(tp == NULL, "malloc");

	list_init(&tp->head);

	/* DONE: Initialize synchronization data. */

	/* initializing the mutex and the condition variables using specific
	 * functions
	 */
	pthread_mutex_init(&(tp->mutex), NULL);
	pthread_cond_init(&(tp->cond_queue), NULL);
	pthread_cond_init(&(tp->work_done), NULL);

	/* also initializing the other variables */
	tp->waiting_threads = 0;
	tp->stop_work = 0;

	tp->num_threads = num_threads;
	tp->threads = malloc(num_threads * sizeof(*tp->threads));
	DIE(tp->threads == NULL, "malloc");
	for (unsigned int i = 0; i < num_threads; ++i) {
		rc = pthread_create(&tp->threads[i], NULL, &thread_loop_function, (void *) tp);
		DIE(rc < 0, "pthread_create");
	}

	return tp;
}

/* Destroy a threadpool. Assume all threads have been joined. */
void destroy_threadpool(os_threadpool_t *tp)
{
	os_list_node_t *n, *p;

	/* DONE: Cleanup synchronization data. */

	/* using specific functions to destroy the mutex and the
	 * condition variables
	 */
	pthread_mutex_destroy(&(tp->mutex));
	pthread_cond_destroy(&(tp->cond_queue));
	pthread_cond_destroy(&(tp->work_done));

	list_for_each_safe(n, p, &tp->head) {
		list_del(n);
		destroy_task(list_entry(n, os_task_t, list));
	}

	free(tp->threads);
	free(tp);
}
