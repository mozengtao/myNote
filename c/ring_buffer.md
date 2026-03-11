## ring buffer
[Ring-Buffer](https://github.com/AndersKaloer/Ring-Buffer/tree/master)  
[c-ringbuf](https://github.com/dhess/c-ringbuf/tree/master)  
[]()  
[]()  
[]()  
[]()  
```c
# 1
#include <stdio.h>

#define BUFFER_SIZE 5

typedef struct {
    int data[BUFFER_SIZE];
    int count; // Number of elements in the buffer (0 to BUFFER_SIZE)
    int tail;  // Index where the next element will be inserted
} RingBuffer;

// Add a new integer to the ring buffer
void add(RingBuffer *buf, int value) {
    buf->data[buf->tail] = value;
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;

    if (buf->count < BUFFER_SIZE) {
        buf->count++;
    }
}

// Print the buffer contents from oldest to newest
void print_buffer(const RingBuffer *buf) {
    int start = (buf->tail - buf->count + BUFFER_SIZE) % BUFFER_SIZE;
    for (int i = 0; i < buf->count; i++) {
        int idx = (start + i) % BUFFER_SIZE;
        printf("%d ", buf->data[idx]);
    }
    printf("\n");
}

// Main: test the ring buffer
int main() {
    RingBuffer buf = { .count = 0, .tail = 0 };

    for (int i = 1; i <= 8; ++i) {
        add(&buf, i);
        printf("After adding %d: ", i);
        print_buffer(&buf);
    }

    return 0;
}

# 2
#include <stdio.h>

#define BUFFER_SIZE 5

typedef struct {
    int data[BUFFER_SIZE];
    int count; // Number of elements in the buffer (0 to BUFFER_SIZE)
    int head;  // Index of the oldest element
} RingBuffer;

// Add a new integer to the ring buffer
void add(RingBuffer *buf, int value) {
    int insert_index = (buf->head + buf->count) % BUFFER_SIZE;

    buf->data[insert_index] = value;

    if (buf->count < BUFFER_SIZE) {
        buf->count++;
    } else {
        // Buffer full, move head to next oldest
        buf->head = (buf->head + 1) % BUFFER_SIZE;
    }
}

// Print the buffer contents from oldest to newest
void print_buffer(const RingBuffer *buf) {
    for (int i = 0; i < buf->count; i++) {
        int idx = (buf->head + i) % BUFFER_SIZE;
        printf("%d ", buf->data[idx]);
    }
    printf("\n");
}

// Main: test the ring buffer
int main() {
    RingBuffer buf = { .count = 0, .head = 0 };

    for (int i = 1; i <= 8; ++i) {
        add(&buf, i);
        printf("After adding %d: ", i);
        print_buffer(&buf);
    }

    return 0;
}

# 3
#include<stdio.h>
#include<malloc.h>
#include<memory.h>
#include<unistd.h>
#include<stdlib.h>

typedef struct cbuff_{
    int * buff;
    int start;
    int end;
    int size;
    int count;
} cbuff_t;

cbuff_t* cbuff_new(int size)
{
  cbuff_t *cb = (cbuff_t*)malloc(sizeof(cbuff_t));
  memset(cb, 0, sizeof(cbuff_t));
  cb->size = size;
    cb->buff = (int*)malloc(sizeof(int)*size);
  
  return cb;
}

void cbuff_add(cbuff_t *cb, int elem)
{
  int end = cb->end;
  if(cb->count && (end % cb->size) == cb->start) {
    printf("Overflow Elem[%d] %d lost\n", cb->start, cb->buff[cb->start]);
    cb->start = (cb->start + 1 ) %cb->size;
    cb->count --;
  }

  printf("Added Elem[%d] = %d\n",cb->end, elem);
  cb->buff[cb->end] = elem;
  cb->end = (cb->end+1) % cb->size;
  cb->count ++;
}

int cbuff_remove(cbuff_t *cb)
{
  int start = cb->start ;
  int ret = -1;
  if(cb->count <= 0) {
    printf("Buffer is empty\n");
    return ret;
  }

  if(cb->count || (start % cb->size) != cb->end) {
    printf("Removed Elem[%d] = %d\n",cb->start, cb->buff[cb->start]);
    ret = cb->buff[cb->start];
    cb->start = (cb->start + 1 ) % cb->size;
    cb->count--;
  } else {
    printf("Buffer is empty\n");
  }

  return ret;
}

void cbuff_print(cbuff_t *cb)
{
  int start = cb->start ;
  int end = cb->end ;
  int i, count = 0;
  for(i = start; count < cb->count; i = (i + 1) % cb->size) {
    printf("Elem[%d] = %d\n", i, cb->buff[i]);
    count++;
    if(i == (end - 1)) {
      break;
    }
  }
}

void cbuff_delete(cbuff_t *cb)
{
  free(cb->buff);
  free(cb);
}

int main(int argc, char *argv[])
{
  char key;
  int elem;
  cbuff_t *cb = cbuff_new(5);

  while(1) {
    printf("circular buffer add[a], remove[r], print[p] : ");
    fflush(stdin);

    key = getchar();
    switch(key) {
    case 'a':
      printf("Element to add : ");
      scanf("%d", &elem);
      cbuff_add(cb, elem);
      break;
    case 'r':
      cbuff_remove(cb);
      break;
    case 'p':
      cbuff_print(cb);
      break;
    case 'q':
      cbuff_delete(cb);
      exit(0);
    }

    continue;
  }

  return 0;
}

# 4
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 5

typedef struct RingBuffer	RingBuffer;
struct RingBuffer
{
	int	count;	/* occupied size of data[]*/
	int	tail;	  /* index of last entry inserted + 1 */
	int data[BUFFER_SIZE];
};

void ringbuffer_add(RingBuffer *rb, int value)
{
    rb->data[rb->tail] = value;
    rb->tail = (rb->tail + 1) % BUFFER_SIZE;
    if (rb->count < BUFFER_SIZE) {
        rb->count++;
    }
}

void ringbuffer_print(RingBuffer *rb)
{
    if(rb->count == 0) {
        printf("RingBuffer is empty.\n");
        return;
    }
  
  int start = (rb->tail - rb->count + BUFFER_SIZE) % BUFFER_SIZE;
    for(int i = 0; i < rb->count; i++) {
        int idx = (start + i) % BUFFER_SIZE;
        printf("%d ", rb->data[idx]);
    }

    printf("\n");
}

int main(void)
{
    int i;
    RingBuffer rb;

    memset(&rb, 0, sizeof(rb));
    ringbuffer_print(&rb);

    for (i = 0; i < 10; i++) {
        ringbuffer_add(&rb, i);
        ringbuffer_print(&rb);
    }

    return 0;
}
```