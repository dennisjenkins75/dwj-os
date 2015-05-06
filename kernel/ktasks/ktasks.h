/*	kernel/ktasks/ktasks.h	*/

// demo.c
extern void	create_demo_threads(int count);

// hud.c
extern int	ktask_hud_entry(void *arg);

// reaper.c
extern int	ktask_reaper_entry(void *arg);

// startup.c
extern int	ktask_startup_entry(void *arg);
