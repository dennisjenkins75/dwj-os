/*	kernel/test/t-printf.c

	Routines to test the behavior of 'snprintf()'.
*/

#include "kernel/kernel.h"

void	test_snprintf_1 (void)
{
	char	initial[17] = "0123456789abcdef";
	char	control[17] = "9999\056789abcdef";
	size_t	count = snprintf (initial, 5, "%d", 99999999);

	int count_ok = (count == 4);
	int mem_ok = (0 == memcmp (initial, control, 17));

	if (!count_ok || !mem_ok) {
		kdebug (DEBUG_ERROR, FAC_GENERAL, "%s: failed\n", __FUNCTION__);
		kdebug (DEBUG_ERROR, FAC_GENERAL, "%s: 'count' = %d\n", __FUNCTION__, count);
		kdebug_mem_dump (DEBUG_ERROR, FAC_GENERAL, initial, sizeof(initial));
		kdebug_mem_dump (DEBUG_ERROR, FAC_GENERAL, control, sizeof(control));
		PANIC1 ("FAILED");
	}

	ASSERT (count == 4);
	ASSERT (memcmp (initial, control, 17) == 0);
}

void	test_snprintf (void)
{
	test_snprintf_1();
}
