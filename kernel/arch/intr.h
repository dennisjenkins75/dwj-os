/*	kernel/intr.h	*/

void	irq_set_handler(int irq, void (*handler)(struct regs *r));
void	intr_install(void);
