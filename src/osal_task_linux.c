// osal_task_linux.c
#include "osal_task.h"
#include "osal.h"
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>

#ifndef OSAL_MAX_TASKS
#define OSAL_MAX_TASKS 16
#endif

typedef struct {
    uint8_t      used;
    pthread_t    th;
    OSAL_TaskEntry entry;
    void*        arg;
    char         name[32];
    uint8_t      prio;

    // suspend/resume (cooperative)
    pthread_mutex_t mtx;
    pthread_cond_t  cv;
    int             suspended;  // 1=suspended
    int             should_exit;
} _TaskSlot;

static _TaskSlot s_tasks[OSAL_MAX_TASKS];

static int _find_idx(OSAL_TaskHandle h) {
    for (int i=0;i<OSAL_MAX_TASKS;i++)
        if (s_tasks[i].used && (OSAL_TaskHandle)&s_tasks[i] == h) return i;
    return -1;
}
static int _alloc_idx(void){
    for (int i=0;i<OSAL_MAX_TASKS;i++)
        if (!s_tasks[i].used) return i;
    return -1;
}

// Thread wrapper: chặn tại “điểm an toàn”
static void* _thread_main(void* p) {
    _TaskSlot* s = (_TaskSlot*)p;

    // Đặt tên (nếu hệ hỗ trợ)
#if defined(__linux__)
    if (s->name[0]) pthread_setname_np(pthread_self(), s->name);
#endif

    // Vòng chạy nhiệm vụ
    s->entry(s->arg);
    return NULL;
}

// Điểm “an toàn” để treo (được gọi từ OSAL_TaskDelayMs / Yield)
static void _maybe_wait_if_suspended(_TaskSlot* s) {
    pthread_mutex_lock(&s->mtx);
    while (s->suspended && !s->should_exit) {
        pthread_cond_wait(&s->cv, &s->mtx);
    }
    pthread_mutex_unlock(&s->mtx);
}

/* ===== API ===== */

OSAL_Status OSAL_TaskCreate(OSAL_TaskHandle* h, OSAL_TaskEntry entry, void* arg, const OSAL_TaskAttr* a) {
    if (!g_osal.initialized || !h || !entry || !a) return OSAL_EINVAL;
    int i = _alloc_idx(); if (i<0) return OSAL_EOS;

    _TaskSlot* s = &s_tasks[i];
    memset(s,0,sizeof(*s));
    s->used = 1;
    s->entry = entry;
    s->arg   = arg;
    s->prio  = a->prio;
    pthread_mutex_init(&s->mtx, NULL);
    pthread_cond_init(&s->cv, NULL);
    if (a->name) strncpy(s->name, a->name, sizeof(s->name)-1);

    pthread_attr_t attr; pthread_attr_init(&attr);

    // Thử set lịch SCHED_FIFO tương ứng “prio” (cần CAP_SYS_NICE/root)
    struct sched_param sp = {0};
    sp.sched_priority = (int)(99 - a->prio); // map đơn giản
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) == 0 &&
        pthread_attr_setschedparam(&attr, &sp) == 0) {
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    } // nếu thất bại → rơi về SCHED_OTHER

    int rc = pthread_create(&s->th, &attr, _thread_main, s);
    pthread_attr_destroy(&attr);
    if (rc != 0) { s->used=0; return OSAL_EOS; }

    *h = (OSAL_TaskHandle)s;
    return OSAL_OK;
}

OSAL_Status OSAL_TaskDelete(OSAL_TaskHandle h) {
    int i = _find_idx(h); if (i<0) return OSAL_EINVAL;
    _TaskSlot* s = &s_tasks[i];
    pthread_mutex_lock(&s->mtx);
    s->should_exit = 1;
    s->suspended = 0;
    pthread_cond_broadcast(&s->cv);
    pthread_mutex_unlock(&s->mtx);

    // best-effort cancel (nếu task không tự thoát)
    pthread_cancel(s->th);
    pthread_join(s->th, NULL);

    pthread_mutex_destroy(&s->mtx);
    pthread_cond_destroy(&s->cv);
    memset(s,0,sizeof(*s));
    return OSAL_OK;
}

OSAL_Status OSAL_TaskSuspend(OSAL_TaskHandle h) {
    int i = _find_idx(h); if (i<0) return OSAL_EINVAL;
    _TaskSlot* s = &s_tasks[i];
    pthread_mutex_lock(&s->mtx);
    s->suspended = 1;
    pthread_mutex_unlock(&s->mtx);
    return OSAL_OK;
}

OSAL_Status OSAL_TaskResume(OSAL_TaskHandle h) {
    int i = _find_idx(h); if (i<0) return OSAL_EINVAL;
    _TaskSlot* s = &s_tasks[i];
    pthread_mutex_lock(&s->mtx);
    s->suspended = 0;
    pthread_cond_broadcast(&s->cv);
    pthread_mutex_unlock(&s->mtx);
    return OSAL_OK;
}

OSAL_Status OSAL_TaskChangePrio(OSAL_TaskHandle h, uint8_t new_prio){
    int i = _find_idx(h); if (i<0) return OSAL_EINVAL;
    _TaskSlot* s = &s_tasks[i];
    struct sched_param sp = { .sched_priority = (int)(99 - new_prio) };
    if (pthread_setschedparam(s->th, SCHED_FIFO, &sp) != 0) return OSAL_EOS;
    s->prio = new_prio;
    return OSAL_OK;
}

void OSAL_TaskYield(void){ sched_yield(); }

void OSAL_TaskDelayMs(uint32_t ms) {
    // chặn nếu đang suspend
    // Tìm slot hiện tại (pthread_self) — tối giản: duyệt
    pthread_t self = pthread_self();
    for (int i=0;i<OSAL_MAX_TASKS;i++){
        if (s_tasks[i].used && pthread_equal(s_tasks[i].th,self)) {
            _maybe_wait_if_suspended(&s_tasks[i]);
            break;
        }
    }
    struct timespec ts = { .tv_sec=(time_t)(ms/1000u), .tv_nsec=(long)((ms%1000u)*1000000u) };
    nanosleep(&ts, NULL);
}

uint32_t OSAL_TaskCount(void){
    uint32_t c=0; for (int i=0;i<OSAL_MAX_TASKS;i++) if (s_tasks[i].used) c++; return c;
}

OSAL_Status OSAL_TaskForEach(void (*cb)(OSAL_TaskHandle, void*), void* arg){
    if (!cb) return OSAL_EINVAL;
    for (int i=0;i<OSAL_MAX_TASKS;i++) if (s_tasks[i].used) cb((OSAL_TaskHandle)&s_tasks[i], arg);
    return OSAL_OK;
}

// (tuỳ chọn) GetState/GetName nếu bạn cần — có thể trả về SUSPENDED khi s->suspended=1.
