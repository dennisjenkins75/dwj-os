/*	kernel/assert.h	*/

#ifndef __ASSERT_H__
#define __ASSERT_H__

extern void panic(const char *file, const char *func, int line, const char *fmt, ...)
	__attribute__ ((__noreturn__));

#define ASSERT(x) ((x) ? (void)0 : panic (__FILE__, __FUNCTION__, __LINE__, "%s", #x))
#define ASSERT_HEAP(x) (is_heap_ptr(x) ? (void)0 : panic (__FILE__, __FUNCTION__, __LINE__, "not in heap: %p == %s", (x), #x))

#define PANIC1(x) panic(__FILE__, __FUNCTION__, __LINE__, (x))
#define PANIC2(x,a) panic(__FILE__, __FUNCTION__, __LINE__, (x), (a))
#define PANIC3(x,a,b) panic(__FILE__, __FUNCTION__, __LINE__, (x), (a), (b))
#define PANIC4(x,a,b,c) panic(__FILE__, __FUNCTION__, __LINE__, (x), (a), (b), (c))


#endif	// __ASSERT_H__
