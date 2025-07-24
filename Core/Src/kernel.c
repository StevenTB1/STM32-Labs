#include "kernel.h"
#include "main.h"

/* Kernel Global Variables */
uint32_t *stackptr;
uint32_t *next_stack_ptr;
uint32_t max_threads = MAX_THREADS;
uint32_t allocated_threads = 0;
uint32_t *msp_initial;

/* Global Thread Management */
thread threads[MAX_THREADS];
uint32_t num_created_threads = 0;
uint32_t current_thread_index = 0;
bool kernel_running = false;

// Context switch scheduler function
void osSched(void)
{
    if (threads[current_thread_index].is_active)
    {
        threads[current_thread_index].sp = (uint32_t *)(__get_PSP() - 8 * 4);
    }

    uint32_t next_thread = current_thread_index;
    uint32_t threads_checked = 0;

    do
    {
        next_thread = (next_thread + 1) % MAX_THREADS;
        threads_checked++;

        if (threads_checked > MAX_THREADS)
        {
            return; // No active threads
        }
    } while (!threads[next_thread].is_active);

    // Switch to next thread
    current_thread_index = next_thread;

    // Reset runtime to timeslice
    threads[current_thread_index].runtime = threads[current_thread_index].timeslice;

    // Set PSP to new thread's stack
    __set_PSP((uint32_t)threads[current_thread_index].sp);
}

/**
 * @brief Yield processor to next thread
 */
void osYield(void)
{
    __asm("SVC %0" : : "i"(YIELD));
}

/**
 * @brief Initialize the kernel
 */
void osKernelInitialize(void)
{
    printf("Initializing OS Kernel...\r\n");

    // Get MSP from vector table
    msp_initial = *(uint32_t **)0;
    printf("Initial MSP: %p\r\n", msp_initial);

    // Start stack allocation from a safe offset below MSP
    next_stack_ptr = (uint32_t *)((uint32_t)msp_initial - 0x800);
    allocated_threads = 0;

    // Initialize all thread structs
    for (int i = 0; i < MAX_THREADS; i++)
    {
        threads[i].sp = NULL;
        threads[i].thread_function = NULL;
        threads[i].is_active = false;
        threads[i].timeslice = 0;
        threads[i].runtime = 0;
    }

    num_created_threads = 0;
    current_thread_index = 0;
    kernel_running = false;

    printf("Kernel initialized. Stack pool at: %p\r\n", next_stack_ptr);
}

/**
 * @brief Start the kernel
 */
void osKernelStart(void)
{
    printf("Starting OS Kernel...\r\n");

    if (num_created_threads == 0)
    {
        printf("ERROR: No threads to run!\r\n");
        return;
    }

    // Find first active thread
    current_thread_index = 0;
    while (current_thread_index < MAX_THREADS && !threads[current_thread_index].is_active)
    {
        current_thread_index++;
    }

    if (current_thread_index >= MAX_THREADS)
    {
        printf("ERROR: No active threads!\r\n");
        return;
    }

    // Initialize first thread's runtime
    threads[current_thread_index].runtime = threads[current_thread_index].timeslice;

    printf("Starting thread %lu (timeslice: %lu ms)\r\n",
           current_thread_index, threads[current_thread_index].timeslice);

    // Set stackptr for compatibility
    stackptr = threads[current_thread_index].sp;

    kernel_running = true;

    // Start first thread
    __asm("SVC #3");
}

/**
 * @brief Allocate stack space for a thread
 */
uint32_t *allocate_thread_stack(void)
{
    if (allocated_threads >= max_threads)
    {
        printf("ERROR: No more stack space\r\n");
        return NULL;
    }

    uint32_t *new_stack_top = next_stack_ptr;
    next_stack_ptr = (uint32_t *)((uint32_t)next_stack_ptr - THREAD_STACK_SIZE);
    allocated_threads++;

    printf("Stack %lu allocated at: %p\r\n", allocated_threads, new_stack_top);

    return new_stack_top;
}

/**
 * @brief Create thread with default timeslice
 */
bool osCreateThread(void (*thread_func)(void *), void *args)
{
    return osCreateThreadWithDeadline(thread_func, args, DEFAULT_TIMESLICE_MS);
}

/**
 * @brief Create thread with custom timeslice
 */
bool osCreateThreadWithDeadline(void (*thread_func)(void *), void *args, uint32_t deadline_ms)
{
    printf("Creating thread (timeslice: %lu ms)...\r\n", deadline_ms);

    // Find available thread slot
    int thread_index = -1;
    for (int i = 0; i < MAX_THREADS; i++)
    {
        if (!threads[i].is_active)
        {
            thread_index = i;
            break;
        }
    }

    if (thread_index == -1)
    {
        printf("ERROR: No thread slots available\r\n");
        return false;
    }

    // Allocate stack
    uint32_t *stack_top = allocate_thread_stack();
    if (stack_top == NULL)
    {
        printf("ERROR: Stack allocation failed\r\n");
        return false;
    }

    // Set up stack frame
    uint32_t *sp = stack_top;

    // Stack frame: xPSR, PC, LR, R12, R3, R2, R1, R0, R11-R4
    *(--sp) = 1 << 24;               // xPSR (Thumb bit)
    *(--sp) = (uint32_t)thread_func; // PC
    *(--sp) = 0xA;                   // LR
    *(--sp) = 0xA;                   // R12
    *(--sp) = 0xA;                   // R3
    *(--sp) = 0xA;                   // R2
    *(--sp) = 0xA;                   // R1
    *(--sp) = (uint32_t)args;        // R0 (arguments)

    // R11 through R4
    for (int i = 0; i < 8; i++)
    {
        *(--sp) = 0xA;
    }

    // Set up thread struct
    threads[thread_index].sp = sp;
    threads[thread_index].thread_function = thread_func;
    threads[thread_index].is_active = true;
    threads[thread_index].timeslice = deadline_ms;
    threads[thread_index].runtime = deadline_ms;

    num_created_threads++;

    printf("Thread %d created successfully!\r\n", thread_index);
    printf("  SP: %p, Function: %p, Args: %p\r\n", sp, thread_func, args);

    return true;
}

void SVC_Handler_Main(unsigned int *svc_args)
{
    unsigned int svc_number = ((char *)svc_args[6])[-2];

    switch (svc_number)
    {
    case 1:
        printf("SVC 1 - SUCCESS\r\n");
        break;
    case 2:
        printf("SVC 2 - ERROR\r\n");
        break;
    case RUN_FIRST_THREAD:
        if (current_thread_index < MAX_THREADS && threads[current_thread_index].is_active)
        {
            __set_PSP((uint32_t)threads[current_thread_index].sp);
            runFirstThread();
        }
        break;
    case YIELD:
        printf("*** COOPERATIVE SWITCH: Thread %lu yielded ***\r\n", current_thread_index);

        if (current_thread_index < MAX_THREADS && threads[current_thread_index].is_active)
        {
            threads[current_thread_index].runtime = threads[current_thread_index].timeslice;
        }

        {
            _ICSR |= 1 << 28;
            __asm("isb");
        }
        break;
    default:
        break;
    }
}
