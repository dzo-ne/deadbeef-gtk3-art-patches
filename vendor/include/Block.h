#ifndef _BLOCK_H_
#define _BLOCK_H_

#if defined(__cplusplus)
extern "C" {
#endif

void *_Block_copy(const void *aBlock);
void _Block_release(const void *aBlock);

#define Block_copy(x) ((__typeof(x))_Block_copy((const void *)(x)))
#define Block_release(x) _Block_release((const void *)(x))

#if defined(__cplusplus)
}
#endif

#endif /* _BLOCK_H_ */
