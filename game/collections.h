/*
** base_enhanced collections: general guide by Avygeil
**
** Before using any of these collections, you must have a struct for the data you want to store.
** All elements of a specific collection will be tied to a single type.
**
** Such types require "extending" the common generic type, `node_t`, by declaring it as the
** FIRST MEMBER of the struct. Example struct to store player names + IDs:
**
** ```
** typedef struct playerData_s {
**     node_t node;
**
**     // user fields below:
**     char name[32];
**     int id;
** } playerData_t;
** ```
**
** The above structure is valid and usable in any collection, as long as node is the FIRST member.
** Functions that accept `genericNode_t` will use that type, and any type with the same skeleton.
** Similarly, you are supposed to cast `genericNode_t` return values to your type to work with them.
**
** Think of such functions as generic functions that accept a generic type <T>, as seen in OOP.
** `node_t` would be the abstract parent type. Technically, you can use that type directly, but
** you can't store anything in it anyway.
**
** Adding a new element to any collection requires you to provide the size of that new structure.
** The function will dynamically allocate the memory. Most of the time, that memory is automatically
** freed when the element is removed or when the collection is cleared. In some cases, which are
** documented, you will need to free() the data yourself.
**
** The top level structures for these collections rely on having NULL pointers for their members.
** This means you should ALWAYS initialize them to 0 (as in: `list_t list = { 0 };`)
**
** These collections are not thread safe on their own. You must explicitly make use of locks in
** multithreaded environments. If speed is important, consider using lockless concurrent collections.
**
** Technically, you can mix data types in the same collection. It is however discouraged unless you
** store a way to tell the type of data before casting.
*/

#pragma once

#include "g_local.h"

typedef void genericNode_t;

typedef struct node_s {
	genericNode_t *next;
	genericNode_t *prev;
} node_t;

typedef struct queue_s {
	genericNode_t *head;
	genericNode_t *tail;
	int size;
} queue_t;

typedef struct list_s {
	genericNode_t *head;
	genericNode_t *tail;
	int size;
} list_t;

typedef struct iterator_s {
	genericNode_t *current;
	qboolean reverse;

	// add future structs here
	union {
		list_t *parentList;
	} parent;
} iterator_t;

typedef qboolean ( *IterateCallback )( genericNode_t*, void* );
typedef qboolean ( *ComparatorCallback )( genericNode_t*, void* );

/*
==================
LISTS

A list is a linear and extensible group of elements.

This implementation does not support indexed elements (on purpose). As such, it is just a list and
not an array list. If using numerical indexes is necessary, just use an actual array. Here, functions
that need a position in the list use a pointer to said element.
==================
*/

// Allocates a new element of size `newElementSize` and adds it to the end of the specified list.
// This element will effectively become the new "tail" of the list.
// Returns a pointer to the newly allocated element, or NULL if allocation failed.
// You are supposed to use the returned pointer to initialize the data.
genericNode_t*	ListAdd( list_t *list, size_t newElementSize );

// Allocates a new element of size `newElementSize` and adds it right after `previousElement`.
// If `previousElement` is NULL, it will be inserted at the end of the list.
// Returns a pointer to the newly allocated element, or NULL if allocation failed.
// You are supposed to use the returned pointer to initialize the data.
genericNode_t*	ListInsert( list_t *list, genericNode_t *previousElement, size_t newElementSize );

// Initializes the specified iterator to loop over the list. Iteration will be done from the start
// to the end unless `reverse` is true. See the ITERATORS section for more information.
void			ListIterate( list_t *list, iterator_t *iter, qboolean reverse );

// Iterates over all elements of the list, starting at the head (included) or at `startElement` (excluded) if it is not NULL.
// Each element is submitted to the callback. The 1st parameter is the element itself, and the 2nd parameter is
// the `userData` parameter of this function (generally used to store a state for the callback to use, but it
// can be NULL). Your comparator callback must return true if the element matches your query. In which case, this
// function will return said element. If the end of the list was reached and no elements were found, it returns NULL.
// If you expect to find more than one element, you can re-use the returned pointer as the `startElement` again,
// in a loop, until NULL is returned.
// This function is not an optimized searching algorithm. It is exactly the same as looping over a normal array
// linearly, and for that purpose, this function is very convenient. If you need a fast searching algorithm, like
// binary searching, use an array instead.
genericNode_t*	ListFind( list_t *list, ComparatorCallback callback, void *userData, genericNode_t *startElement );

// This function is a wrapper over an iterator, which simply calls the specified callback for each element.
// The 1st parameter is the element itself, and the 2nd is `userData` (generally used to store a state for the
// callback to use, but it can be NULL). The list will be iterated until the callback returns false.
// It is generally more convenient to use a raw iterator in a loop with access to local variables in its context.
void			ListForEach( list_t *list, IterateCallback callback, void *userData );

// Removes the specified element from the list and automatically frees its memory.
void			ListRemove( list_t *list, genericNode_t *element );

// Removes all elements from the list and frees all memory.
void			ListClear( list_t *list );

// Copies an entire list to new elements in another list.
// The other list does not need to be empty.
void			ListCopy(list_t *old, list_t *new, size_t elementSize);

// Returns -1, 0, or 1 based on whether the first element is less than, equal to, or greater than the second.
typedef int (*SortCallback)(genericNode_t *, genericNode_t *, void *userData);

// Sorts a given list using a merge sort algorithm.
void ListSort(list_t *list, SortCallback compare, void *userData);

// Trims the list to the specified number of elements, trimming from the tail if reverse is false, or from the head if reverse is true.
// If the list contains fewer elements than the specified number, the list remains unchanged.
// Any elements beyond the specified number are removed and their memory is freed.
void ListTrim(list_t *list, int numElements, qboolean reverse);

/*
==================
QUEUES

A queue is a data type where elements are always added at the end.
You can only retrieve elements in the same order as they were added (first come, first served).
This is the opposite of a stack.

Internally, it is just a specialized list with limited functionality to encourage good
programming. Consider using a list if you need more.
==================
*/

// Allocates a new element of size `newElementSize` and adds it to the end of the specified queue.
// This element will effectively become the new "tail" of the queue.
// Returns a pointer to the newly allocated element, or NULL if allocation failed.
// You are supposed to use the returned pointer to initialize the data.
genericNode_t*	QueueAdd( queue_t *queue, size_t newElementSize );

// Returns the first element in the specified queue, without removing it.
// Returns NULL if there are no more elements.
genericNode_t*	QueuePeek( queue_t *queue );

// Returns the first element in the specified queue and removes it, allowing the next call to return
// the next element in the queue.
// Returns NULL if there are no more elements.
// After you are done with the data, you must free() the pointer yourself!
genericNode_t*	QueuePoll( queue_t *queue );

// Removes the first element from the queue, freeing memory automatically. This is the same as
// calling `free( QueuePoll() );`
void			QueueDiscard( queue_t *queue );

// Removes all elements from the queue and frees all memory.
void			QueueClear( queue_t *queue );

/*
==================
ITERATORS

An iterator is a convenient way to iterate over elements of a collection, close to the way it would be done
using a for loop and an array.

Example code (assuming `myDataType_t` is a valid type that extends `node_t`) to loop over a list:

```
list_t list = { 0 };

// add stuff to the list

iterator_t iter;
ListIterate( &list, &iter, qfalse );

while ( IteratorHasNext( &iter ) ) {
    myDataType_t *d = IteratorNext( &iter );
	
	// do stuff
}
```

Not all collections support iterators. Those that do have a dedicated function to initialize it for their type.

While you are iterating a list, NEVER change its content! Do not add/clear/delete a collection while iterating!
The only safe operation is deleting THROUGH THE ITERATOR, using `IteratorRemove()`, which will correctly
update the iterator while you are still iterating.

As a side effect, iterating is not a thread safe operation. Do not iterate over collections that are accessed
by multiple threads.
==================
*/

// Returns true if IteratorNext() would return a valid element (non NULL)
qboolean		IteratorHasNext( iterator_t *iter );

// Returns the next element in the iteration. The iterator must have been initialized for a compatible collection
// type using one of the dedicated functions prior to it. Returns NULL if no more elements are available.
// Internally, the cursor works so that when an iterator is first initialized, calling this function for the first
// time will return the first element.
genericNode_t*	IteratorNext( iterator_t *iter );

// Removes the current element pointed by the iterator from the collection. Calling this has the same effect on the
// internal cursor than calling `IteratorNext()`, which means it is shifted. This function is the only way to
// alter a collection safely while it is being iterated. Its behavior allows you to selectively remove elements
// while iterating in the same fashion as `IteratorNext()`.
void			IteratorRemove( iterator_t *iter );
