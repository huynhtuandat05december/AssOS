#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
	return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
	/* TODO: put a new process to queue [q] */
	if (q->size == MAX_QUEUE_SIZE)
		return;
	q->proc[q->size++] = proc;
}

struct pcb_t *dequeue(struct queue_t *q)
{
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	if (q->size <= 0)
		return NULL;
	int maxPriority = 0, index = -1;
	int i;
	for (i = 0; i < q->size; i++)
		if (maxPriority < q->proc[i]->priority)
		{
			maxPriority = q->proc[i]->priority;
			index = i;
		}
	if (index == -1)
		return NULL;
	struct pcb_t *temp = q->proc[index];
	q->proc[index] = q->proc[q->size - 1];
	q->size--;
	return temp;
}
