#ifndef _DISPATCH_DISPATCH_H_
#define _DISPATCH_DISPATCH_H_

#include <stdint.h>
#include <stddef.h>
#include <Block.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef void *dispatch_object_t;
typedef void *dispatch_queue_t;
typedef void *dispatch_queue_attr_t;
typedef void (^dispatch_block_t)(void);

#define DISPATCH_QUEUE_PRIORITY_HIGH        2
#define DISPATCH_QUEUE_PRIORITY_DEFAULT     0
#define DISPATCH_QUEUE_PRIORITY_LOW         (-2)
#define DISPATCH_QUEUE_PRIORITY_BACKGROUND  (-32768)

dispatch_queue_t dispatch_queue_create(const char *label, dispatch_queue_attr_t attr);
dispatch_queue_t dispatch_get_global_queue(long identifier, unsigned long flags);
void dispatch_async(dispatch_queue_t queue, dispatch_block_t block);
void dispatch_release(dispatch_object_t object);

#if defined(__cplusplus)
}
#endif

#endif /* _DISPATCH_DISPATCH_H_ */
