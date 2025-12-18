#ifndef __WORKQUEUE_H__
#define __WORKQUEUE_H__

typedef void (*work_t)(void *arg);

void workqueue_init(void);
void workqueue_run(work_t work, void *arg);

#endif /* __WORKQUEUE_H__ */
