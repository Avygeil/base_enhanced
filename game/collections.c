#include "g_local.h"

genericNode_t* ListAdd( list_t *list, size_t newElementSize ) {
	return ListInsert( list, NULL, newElementSize );
}

genericNode_t* ListInsert( list_t *list, genericNode_t *previousElement, size_t newElementSize ) {
	node_t *newNode = calloc( 1, newElementSize );

	if ( !newNode ) {
		return NULL;
	}

	if ( list->head == NULL ) {
		list->head = newNode;
		list->tail = newNode;
	} else {
		node_t *previousNode = ( node_t* )previousElement;
		node_t *nextNode;

		if ( previousNode == NULL ) {
			// add it to the end
			previousNode = ( node_t* )list->tail;
			nextNode = NULL;
		} else {
			nextNode = previousNode->next;
		}

		previousNode->next = newNode;

		newNode->next = nextNode;
		newNode->prev = previousNode;

		if ( nextNode != NULL ) {
			nextNode->prev = newNode;
		} else {
			// this is the new tail
			list->tail = newNode;
		}
	}

	list->size++;

	return newNode;
}

void ListIterate( list_t *list, iterator_t *iter, qboolean reverse ) {
	iter->current = !reverse ? list->head : list->tail;
	iter->reverse = reverse;
	iter->parent.parentList = list;
}

genericNode_t* ListFind( list_t *list, ComparatorCallback callback, void *userData, genericNode_t *startElement ) {
	iterator_t iter;
	qboolean foundStart = qfalse;

	if ( startElement == NULL ) {
		foundStart = qtrue;
	}

	ListIterate( list, &iter, qfalse );

	while ( IteratorHasNext( &iter ) ) {
		genericNode_t *currentNode = IteratorNext( &iter );

		if ( foundStart && callback( currentNode, userData ) ) {
			return currentNode;
		}

		if ( currentNode == startElement ) {
			foundStart = qtrue;
		}
	}

	return NULL;
}

void ListForEach( list_t *list, IterateCallback callback, void *userData ) {
	iterator_t iter;

	ListIterate( list, &iter, qfalse );

	while ( IteratorHasNext( &iter ) ) {
		if ( !callback( IteratorNext( &iter ), userData ) ) {
			return;
		}
	}
}

void ListRemove( list_t *list, genericNode_t *element ) {
	node_t *node = ( node_t* )element;
	node_t *nextNode = ( node_t* )node->next, *prevNode = ( node_t* )node->prev;

	free( node );

	if ( prevNode != NULL ) {
		prevNode->next = nextNode;
	} else {
		// that was the head
		list->head = nextNode;
	}

	if ( nextNode != NULL ) {
		nextNode->prev = prevNode;
	} else {
		// that was the tail
		list->tail = prevNode;
	}

	list->size--;
}

void ListClear( list_t *list ) {
	iterator_t iter;

	ListIterate( list, &iter, qfalse );

	while ( IteratorHasNext( &iter ) ) {
		IteratorRemove( &iter );
	}
}

void ListCopy(list_t *old, list_t *new, size_t elementSize) {
	iterator_t iter;

	ListIterate(old, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		genericNode_t *oldElement = IteratorNext(&iter);
		node_t *newElement = (node_t *)ListAdd(new, elementSize);

		// backup prev/next before memcpy overwrites it
		genericNode_t *prev = newElement->prev;
		genericNode_t *next = newElement->next;

		// copy
		memcpy(newElement, oldElement, elementSize);

		// restore prev/next
		newElement->prev = prev;
		newElement->next = next;
	}
}

genericNode_t* QueueAdd( queue_t *queue, size_t newElementSize ) {
	// since we are adding things at the end of the queue, this is exactly the same operation
	return ListAdd( ( list_t* )queue, newElementSize );
}

genericNode_t* QueuePeek( queue_t *queue ) {
	return queue->head;
}

genericNode_t* QueuePoll( queue_t *queue ) {
	node_t *oldHead = ( node_t* )queue->head;

	if ( oldHead == NULL ) {
		return NULL;
	}

	node_t *newHead = oldHead->next;

	if ( newHead != NULL ) {
		newHead->prev = NULL;
	} else {
		queue->tail = NULL;
	}

	queue->head = newHead;
	queue->size--;

	oldHead->next = NULL;
	oldHead->prev = NULL;

	return oldHead;
}

void QueueDiscard( queue_t *queue ) {
	free( QueuePoll( queue ) );
}

void QueueClear( queue_t *queue ) {
	while ( queue->size > 0 ) {
		QueueDiscard( queue );
	}
}

qboolean IteratorHasNext( iterator_t *iter ) {
	return iter->current != NULL;
}

genericNode_t* IteratorNext( iterator_t *iter ) {
	if ( !IteratorHasNext( iter ) ) {
		return NULL;
	}

	node_t *node = ( node_t* )iter->current;
	iter->current = !iter->reverse ? node->next : node->prev;
	return node;
}

void IteratorRemove( iterator_t *iter ) {
	if ( iter->current == NULL ) {
		return;
	}

	node_t *node = ( node_t* )iter->current;
	iter->current = !iter->reverse ? node->next : node->prev;

	// currently, you can only iterate lists. makes it more simple
	ListRemove( iter->parent.parentList, node );
}