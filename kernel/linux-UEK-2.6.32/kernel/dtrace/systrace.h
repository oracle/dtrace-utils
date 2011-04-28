#ifndef _SYSTRACE_H_
#define _SYSTRACE_H_

extern dtrace_provider_id_t	systrace_id;

extern void systrace_provide(void *, const dtrace_probedesc_t *);
extern int systrace_enable(void *arg, dtrace_id_t, void *);
extern void systrace_disable(void *arg, dtrace_id_t, void *);
extern void systrace_destroy(void *, dtrace_id_t, void *);

extern int systrace_dev_init(void);
extern void systrace_dev_exit(void);

#endif /* _SYSTRACE_H_ */
