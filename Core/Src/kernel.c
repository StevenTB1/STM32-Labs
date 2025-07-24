#include "kernel.h"
#include "main.h"

/* Kernel Global Variables */
uint32_t *stackptr;
uint32_t *next_stack_ptr;           // Points to the next available stack location
uint32_t max_threads = MAX_THREADS; // Maximum number of threads (from header)
uint32_t allocated_threads = 0;     // Number of threads currently allocated
uint32_t *msp_initial;              // Store initial MSP value

/* Global Threads */
thread threads[MAX_THREADS];       // Array of thread instances
uint32_t num_created_threads = 0;  // Number of threads created
uint32_t current_thread_index = 0; // Index of currently running thread

/**
 * @brief Context switch scheduler function - implements round-robin scheduling
 * This function saves the current thread's context and switches to the next active thread
 */
void osSched(void)
{
    // Step 1: Save the stack pointer of the current thread
    if (threads[current_thread_index].is_active)
    {
        threads[current_thread_index].sp = (uint32_t *)(__get_PSP() - 8 * 4);
        printf("Saved thread %lu SP: %p\r\n", current_thread_index, threads[current_thread_index].sp);
    }

    // Step 2: Find the next active thread (Round-Robin scheduling)
    uint32_t next_thread = current_thread_index;
    uint32_t threads_checked = 0;

    do
    {
        next_thread = (next_thread + 1) % MAX_THREADS;
        threads_checked++;

        // Prevent infinite loop if no active threads
        if (threads_checked > MAX_THREADS)
        {
            printf("ERROR: No active threads found in scheduler!\r\n");
            return;
        }
    } while (!threads[next_thread].is_active);

    // Update current thread index
    current_thread_index = next_thread;

    printf("Switching to thread %lu\r\n", current_thread_index);

    // Step 3: Set PSP to the new current thread's stack pointer
    __set_PSP((uint32_t)threads[current_thread_index].sp);

    printf("Set PSP to: %p\r\n", threads[current_thread_index].sp);
}

/**
 * @brief Trigger a voluntary context switch via system call
 * This function makes a system call to yield the processor to the next thread
 */
void osYield(void)
{
    __asm("SVC %0" : : "i"(YIELD));
}

/**
 * @brief Initialize the kernel - must be called before creating any threads
 * This function sets up all kernel global variables and prepares the system
 */
void osKernelInitialize(void)
{
    printf("Initializing OS Kernel...\r\n");

    // Get the initial MSP value from the vector table
    msp_initial = *(uint32_t **)0;
    printf("Initial MSP: %p\r\n", msp_initial);

    // Initialize the kernel using the internal function
    kernel_init(msp_initial);

    printf("OS Kernel initialized successfully!\r\n");
}

/**
 * @brief Start the kernel and run the first created thread
 * This function starts executing the first thread that was created with osCreateThread
 */
void osKernelStart(void)
{
    printf("Starting OS Kernel...\r\n");

    // Check if we have any threads to run
    if (num_created_threads == 0)
    {
        printf("ERROR: No threads to run! Call osCreateThread first.\r\n");
        return;
    }

    // Find the first active thread
    current_thread_index = 0;
    while (current_thread_index < MAX_THREADS && !threads[current_thread_index].is_active)
    {
        current_thread_index++;
    }

    if (current_thread_index >= MAX_THREADS)
    {
        printf("ERROR: No active threads found!\r\n");
        return;
    }

    printf("Starting with thread %lu\r\n", current_thread_index);
    printf("Thread function: %p\r\n", threads[current_thread_index].thread_function);
    printf("Thread stack pointer: %p\r\n", threads[current_thread_index].sp);

    // Set the global stackptr for compatibility with the SVC handler
    stackptr = threads[current_thread_index].sp;

    // Make the system call to run the first thread
    __asm("SVC #3"); // Same as run_first_thread() but inline
}

/**
 * @brief Get the initial MSP value stored by the kernel
 * @return Pointer to initial MSP value
 */
uint32_t *osGetInitialMSP(void)
{
    return msp_initial;
}

/**
 * @brief Initialize the kernel stack allocation system (internal function)
 * @param msp_init_val Initial MSP value from vector table
 */
void kernel_init(uint32_t *msp_init_val)
{
    // Initialize the next available stack pointer
    next_stack_ptr = (uint32_t *)((uint32_t)msp_init_val - THREAD_STACK_SIZE);
    allocated_threads = 0;

    // Initialize all threads
    for (int i = 0; i < MAX_THREADS; i++)
    {
        threads[i].sp = NULL;
        threads[i].thread_function = NULL;
        threads[i].is_active = false;
    }

    num_created_threads = 0;
    current_thread_index = 0;

    printf("Kernel stack pool starts at: %p\r\n", next_stack_ptr);
    printf("Thread array initialized (%d slots)\r\n", MAX_THREADS);
}

/**
 * @brief Allocate a new thread stack
 * @return Pointer to the top of the newly allocated stack, or NULL if no space available
 * @note This function only allocates space, it does not set up the stack contents
 */
uint32_t *allocate_thread_stack(void)
{
    // Check if we have space for another thread
    if (allocated_threads >= max_threads)
    {
        printf("ERROR: No more stack space available (max threads: %lu)\r\n", max_threads);
        return NULL;
    }

    // Get the current stack location (this will be the TOP of the stack)
    uint32_t *new_stack_top = next_stack_ptr;

    // Update for next allocation (subtract another stack size)
    next_stack_ptr = (uint32_t *)((uint32_t)next_stack_ptr - THREAD_STACK_SIZE);

    // Increment thread counter
    allocated_threads++;

    printf("Allocated stack %lu at: %p (top)\r\n", allocated_threads, new_stack_top);

    return new_stack_top;
}

/**
 * @brief Create and setup a new thread
 * @param thread_func Pointer to the function this thread will execute
 * @return true if thread was successfully created, false if failed (no space available)
 */
bool osCreateThread(void (*thread_func)(void *))
{
    printf("Creating new thread...\r\n");

    // Step 1: Find an available thread slot
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
        printf("ERROR: No available thread slots (max: %d)\r\n", MAX_THREADS);
        return false;
    }

    // Step 2: Allocate a new stack
    uint32_t *new_stack_top = allocate_thread_stack();
    if (new_stack_top == NULL)
    {
        printf("ERROR: Failed to allocate stack for new thread\r\n");
        return false;
    }

    // Step 3: Set up the stack frame (like we did in Lab 2)
    uint32_t *stack_ptr = new_stack_top;

    // Set up the stack frame for the new thread
    *(--stack_ptr) = 1 << 24;               // xPSR - magic number for thumb mode
    *(--stack_ptr) = (uint32_t)thread_func; // PC - function to run
    *(--stack_ptr) = 0xA;                   // LR (R14)

    // Fill the remaining 13 register slots with 0xA
    // (R12, R3, R2, R1, R0, R11, R10, R9, R8, R7, R6, R5, R4)
    for (int i = 0; i < 13; i++)
    {
        *(--stack_ptr) = 0xA;
    }

    // Step 4: Update the thread struct
    threads[thread_index].sp = stack_ptr;                // Save the stack pointer
    threads[thread_index].thread_function = thread_func; // Save the function pointer
    threads[thread_index].is_active = true;              // Mark as active

    // Step 5: Update global counters
    num_created_threads++;

    printf("Thread %d created successfully!\r\n", thread_index);
    printf("  Stack top: %p\r\n", new_stack_top);
    printf("  Stack pointer: %p\r\n", threads[thread_index].sp);
    printf("  Function: %p\r\n", threads[thread_index].thread_function);
    printf("  Total threads created: %lu\r\n", num_created_threads);

    return true;
}

void SVC_Handler_Main(unsigned int *svc_args)
{
    unsigned int svc_number;
    svc_number = ((char *)svc_args[6])[-2];
    switch (svc_number)
    {
    case 1:
        printf("System Call 1 - SUCCESS\r\n");
        break;
    case 2:
        printf("System Call 2 - ERROR\r\n");
        break;
    case RUN_FIRST_THREAD:
        // Use the currently selected thread's stack pointer
        if (current_thread_index < MAX_THREADS && threads[current_thread_index].is_active)
        {
            printf("SVC: Setting PSP to thread %lu stack: %p\r\n",
                   current_thread_index, threads[current_thread_index].sp);
            __set_PSP((uint32_t)threads[current_thread_index].sp);
            runFirstThread();
        }
        else
        {
            printf("ERROR: Invalid thread index in SVC handler\r\n");
        }
        break;
    case YIELD:
        // Pend an interrupt to do the context switch
        {
            _ICSR |= 1 << 28;
            __asm("isb");
        }
        break;
    case 17:
        printf("Success!\r\n");
        break;
    default:
        break;
    }
}
