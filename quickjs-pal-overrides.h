typedef JSPalThread pthread_t;
typedef JSPalMutex pthread_mutex_t;
typedef JSPalCond pthread_cond_t;
typedef size_t pthread_attr_t;
typedef JSPalTime timespec;
#define pthread_attr_init(attr_stack_size) (*(pthread_attr_t*)(attr_stack_size) = 0)
#define pthread_attr_destroy(attr)
#define pthread_attr_setstacksize(attr, size) (*(attr) = (size))
#define pthread_create(thread, attr_stack_size, start, arg) jspal_thread_create(pal, (thread), (start), (arg), attr_stack_size ? *(pthread_attr_t*)(attr_stack_size) : 0)
#define pthread_join(thread, unused_ret) jspal_thread_join(pal, &(thread))
#define pthread_detach(thread) jspal_thread_detach(pal, (thread))
#define pthread_mutex_init(mutex, unused_attr) jspal_mutex_init(pal, (mutex))
#define pthread_mutex_destroy(mutex) jspal_mutex_destroy(pal, (mutex))
#define pthread_mutex_lock(mutex) jspal_mutex_lock(pal, (mutex))
#define pthread_mutex_unlock(mutex) jspal_mutex_unlock(pal, (mutex))
#define pthread_cond_init(cond, attr) jspal_cond_init(pal, (cond))
#define pthread_cond_destroy(cond) jspal_cond_destroy(pal, (cond))
#define pthread_cond_wait(cond, mutex) jspal_cond_wait(pal, (cond), (mutex))
#define pthread_cond_timedwait(cond, mutex, time) jspal_cond_timedwait(pal, (cond), (mutex), (time))
#define pthread_cond_signal(cond) jspal_cond_signal(pal, (cond))
#define pthread_cond_broadcast(cond) jspal_cond_broadcast(pal, (cond))
#define _Atomic