#include "unity.h"
#include "osal.h"
#include "osal_task.h"
#include "osal_types.h"
#include <unistd.h>     /* for usleep */
#include <string.h>

/* 
 * NOTE:
 *  - In your current osal_task_linux.c, OSAL_MAX_TASKS is 8 (static).
 *  - We mirror that here for the "exhaust slots" test.
 */
#define TEST_OSAL_MAX_TASKS   8

/* a simple task that just marks a flag and exits */
static void simple_task_entry(void *arg)
{
    int *flag = (int *)arg;
    if (flag) {
        *flag = 1;
    }
    /* return → thread ends → OSAL_TaskDelete can join */
}

/* a long-running task that cooperates with suspend/resume */
static void loop_task_entry(void *arg)
{
    volatile int *running = (volatile int *)arg;
    while (*running) {
        /* cooperative point, will also check suspend/stop */
        OSAL_TaskDelayMs(10);
    }
    /* exit gracefully */
}

void setUp(void)
{
    /* init OSAL core first because task code uses OSAL_LOG */
    OSAL_Config cfg = {
        .backend = OSAL_BACKEND_LINUX,
        .log     = NULL,
        .platform_ctx = NULL
    };
    OSAL_Status st = OSAL_Init(&cfg);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
}

void tearDown(void)
{
    OSAL_Deinit();
}

/* ===== Tests ===== */

void test_TaskCreate_null_out_handle_should_fail(void)
{
    OSAL_Status st = OSAL_TaskCreate(NULL, simple_task_entry, NULL, NULL);
    TEST_ASSERT_EQUAL(OSAL_EINVAL, st);
}

void test_TaskCreate_null_entry_should_fail(void)
{
    OSAL_TaskHandle h = NULL;
    OSAL_Status st = OSAL_TaskCreate(&h, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(OSAL_EINVAL, st);
    TEST_ASSERT_NULL(h);
}

void test_TaskCreate_and_delete_should_succeed(void)
{
    OSAL_TaskHandle h = NULL;
    int done = 0;

    OSAL_Status st = OSAL_TaskCreate(&h, simple_task_entry, &done, NULL);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
    TEST_ASSERT_NOT_NULL(h);

    /* give the task some time to run */
    OSAL_TaskDelayMs(20);
    TEST_ASSERT_EQUAL(1, done);

    /* delete should succeed even if task already returned */
    st = OSAL_TaskDelete(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
}

void test_Task_suspend_resume_and_state(void)
{
    OSAL_TaskHandle h = NULL;
    volatile int running = 1;
    OSAL_TaskAttr attr = {
        .name = "loop_task",
        .stack_size = 0,
        .prio = 10
    };

    OSAL_Status st = OSAL_TaskCreate(&h, loop_task_entry, (void*)&running, &attr);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
    TEST_ASSERT_NOT_NULL(h);

    /* let task start */
    OSAL_TaskDelayMs(30);

    /* suspend it */
    st = OSAL_TaskSuspend(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);

    /* allow suspend to take effect */
    OSAL_TaskDelayMs(20);

    OSAL_TaskState state = OSAL_TASK_STATE_INVALID;
    st = OSAL_TaskGetState(h, &state);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
    /* when suspended, backend reports WAITING */
    TEST_ASSERT_EQUAL(OSAL_TASK_STATE_WAITING, state);

    /* resume and check again */
    st = OSAL_TaskResume(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);

    OSAL_TaskDelayMs(20);
    st = OSAL_TaskGetState(h, &state);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
    /* after resume it should be running again */
    TEST_ASSERT_EQUAL(OSAL_TASK_STATE_RUNNING, state);

    /* stop loop and delete */
    running = 0;
    OSAL_TaskDelayMs(20);
    st = OSAL_TaskDelete(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
}

void test_Task_change_priority_and_get_name(void)
{
    OSAL_TaskHandle h = NULL;
    volatile int running = 1;
    OSAL_TaskAttr attr = {
        .name = "prio_task",
        .stack_size = 0,
        .prio = 5
    };

    OSAL_Status st = OSAL_TaskCreate(&h, loop_task_entry, (void*)&running, &attr);
    TEST_ASSERT_EQUAL(OSAL_OK, st);

    /* change priority – on Linux may fallback but should return OK/EINIT */
    st = OSAL_TaskChangePrio(h, 20);
    /* depending on your build you may get OSAL_OK or OSAL_EINIT, so just check it's not EINVAL */
    TEST_ASSERT(st != OSAL_EINVAL);

    /* get name */
    const char *name = NULL;
    st = OSAL_TaskGetName(h, &name);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("prio_task", name);

    /* stop */
    running = 0;
    OSAL_TaskDelayMs(20);
    st = OSAL_TaskDelete(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
}

void test_Task_exhaust_slots_should_fail_on_extra_task(void)
{
    OSAL_TaskHandle hs[TEST_OSAL_MAX_TASKS];
    volatile int running[TEST_OSAL_MAX_TASKS];
    memset(hs, 0, sizeof(hs));

    /* fill all slots */
    for (int i = 0; i < TEST_OSAL_MAX_TASKS; ++i) {
        running[i] = 1;
        OSAL_Status st = OSAL_TaskCreate(&hs[i], loop_task_entry, (void*)&running[i], NULL);
        TEST_ASSERT_EQUAL_MESSAGE(OSAL_OK, st, "should be able to create task up to max");
    }

    /* one more should fail */
    OSAL_TaskHandle extra = NULL;
    OSAL_Status st = OSAL_TaskCreate(&extra, loop_task_entry, (void*)&running[0], NULL);
    TEST_ASSERT_EQUAL(OSAL_EINIT, st);

    /* cleanup all */
    for (int i = 0; i < TEST_OSAL_MAX_TASKS; ++i) {
        running[i] = 0;
    }
    OSAL_TaskDelayMs(50);
    for (int i = 0; i < TEST_OSAL_MAX_TASKS; ++i) {
        TEST_ASSERT_EQUAL(OSAL_OK, OSAL_TaskDelete(hs[i]));
    }
}

void test_Task_api_with_null_handle_should_fail(void)
{
    OSAL_Status st;

    st = OSAL_TaskDelete(NULL);
    TEST_ASSERT_EQUAL(OSAL_EINVAL, st);

    st = OSAL_TaskSuspend(NULL);
    TEST_ASSERT_EQUAL(OSAL_EINVAL, st);

    st = OSAL_TaskResume(NULL);
    TEST_ASSERT_EQUAL(OSAL_EINVAL, st);

    OSAL_TaskState state;
    st = OSAL_TaskGetState(NULL, &state);
    TEST_ASSERT_EQUAL(OSAL_EINVAL, st);

    st = OSAL_TaskGetName(NULL, NULL);
    TEST_ASSERT_EQUAL(OSAL_EINVAL, st);
}

/* ===== extra tasks for yield/count tests ===== */

/* task that yields a few times and then exits */
static void yield_task_entry(void *arg)
{
    int *counter = (int *)arg;
    for (int i = 0; i < 5; ++i) {
        if (counter) {
            (*counter)++;
        }
        /* must be safe to call from a running task */
        OSAL_TaskYield();
    }
}

/* task that only yields in a loop, so suspend/resume will go through yield path */
static void yield_loop_task_entry(void *arg)
{
    volatile int *running = (volatile int *)arg;
    while (*running) {
        OSAL_TaskYield();
    }
}

/* --- TESTS --- */

void test_TaskYield_should_run_without_error(void)
{
    OSAL_TaskHandle h = NULL;
    int counter = 0;

    OSAL_Status st = OSAL_TaskCreate(&h, yield_task_entry, &counter, NULL);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
    TEST_ASSERT_NOT_NULL(h);

    /* give the task time to run all 5 yields */
    OSAL_TaskDelayMs(50);

    /* task should have incremented counter 5 times */
    TEST_ASSERT_EQUAL(5, counter);

    /* delete after finished */
    st = OSAL_TaskDelete(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
}

void test_TaskYield_should_block_when_suspended(void)
{
    OSAL_TaskHandle h = NULL;
    volatile int running = 1;

    OSAL_Status st = OSAL_TaskCreate(&h, yield_loop_task_entry, (void*)&running, NULL);
    TEST_ASSERT_EQUAL(OSAL_OK, st);

    /* let task start and call yield a few times */
    OSAL_TaskDelayMs(30);

    /* suspend the task – OSAL_TaskYield has logic to wait on condvar if suspended */
    st = OSAL_TaskSuspend(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);

    /* give it a bit so that the task really enters the suspended wait */
    OSAL_TaskDelayMs(20);

    OSAL_TaskState state = OSAL_TASK_STATE_INVALID;
    st = OSAL_TaskGetState(h, &state);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
    /* when a yielding task is suspended, it should become WAITING */
    TEST_ASSERT_EQUAL(OSAL_TASK_STATE_WAITING, state);

    /* resume back */
    st = OSAL_TaskResume(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);

    /* stop the loop and delete */
    running = 0;
    OSAL_TaskDelayMs(20);
    st = OSAL_TaskDelete(h);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
}

void test_TaskCount_should_reflect_number_of_tasks(void)
{
    /* at start of each test we called OSAL_Init() in setUp() and created no task yet */
    uint32_t cnt = OSAL_TaskCount();
    TEST_ASSERT_EQUAL_UINT32(0, cnt);

    OSAL_TaskHandle h1 = NULL, h2 = NULL;
    volatile int r1 = 1, r2 = 1;

    /* create 2 long-running tasks */
    TEST_ASSERT_EQUAL(OSAL_OK, OSAL_TaskCreate(&h1, yield_loop_task_entry, (void*)&r1, NULL));
    TEST_ASSERT_EQUAL(OSAL_OK, OSAL_TaskCreate(&h2, yield_loop_task_entry, (void*)&r2, NULL));

    /* now count must be 2 */
    cnt = OSAL_TaskCount();
    TEST_ASSERT_EQUAL_UINT32(2, cnt);

    /* stop and delete them */
    r1 = 0; r2 = 0;
    OSAL_TaskDelayMs(30);

    TEST_ASSERT_EQUAL(OSAL_OK, OSAL_TaskDelete(h1));
    TEST_ASSERT_EQUAL(OSAL_OK, OSAL_TaskDelete(h2));

    /* after delete, count should go back to 0 */
    cnt = OSAL_TaskCount();
    TEST_ASSERT_EQUAL_UINT32(0, cnt);
}
/* Unity main */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_TaskCreate_null_out_handle_should_fail);
    RUN_TEST(test_TaskCreate_null_entry_should_fail);
    RUN_TEST(test_TaskCreate_and_delete_should_succeed);
    RUN_TEST(test_Task_suspend_resume_and_state);
    RUN_TEST(test_Task_change_priority_and_get_name);
    RUN_TEST(test_Task_exhaust_slots_should_fail_on_extra_task);
    RUN_TEST(test_Task_api_with_null_handle_should_fail);
    RUN_TEST(test_TaskYield_should_run_without_error);
    RUN_TEST(test_TaskYield_should_block_when_suspended);
    RUN_TEST(test_TaskCount_should_reflect_number_of_tasks);
    
    return UNITY_END();
}
