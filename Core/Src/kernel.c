#include "kernel.h"
#include "main.h"

/* Kernel Global Variables */
uint32_t *stackptr;
uint32_t *next_stack_ptr;       // Points to the next available stack location
uint32_t max_threads = 8;       // Maximum number of threads (conservative estimate)
uint32_t allocated_threads = 0; // Number of threads currently allocated
uint32_t *msp_initial;          // Store initial MSP value

/* Global Thread Instance */
thread current_thread; // Single thread instance (will become array in Lab 4)

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
 * @brief Start the kernel and run the created thread
 * This function starts executing the thread that was created with osCreateThread
 */
void osKernelStart(void)
{
    printf("Starting OS Kernel...\r\n");

    // Check if we have a thread to run
    if (current_thread.sp == NULL || current_thread.thread_function == NULL)
    {
        printf("ERROR: No thread to run! Call osCreateThread first.\r\n");
        return;
    }

    printf("Running thread with function: %p\r\n", current_thread.thread_function);
    printf("Thread stack pointer: %p\r\n", current_thread.sp);

    // Set the global stackptr for compatibility with the SVC handler
    stackptr = current_thread.sp;

    // Make the system call to run the first thread
    __asm("SVC #3"); // Same as run_first_thread() but inline
}

/**
 * @brief Initialize the kernel stack allocation system (internal function)
 * @param msp_init_val Initial MSP value from vector table
 */
void kernel_init(uint32_t *msp_init_val)
{
    // Initialize the next available stack pointer
    // Start allocating stacks from MSP - THREAD_STACK_SIZE
    next_stack_ptr = (uint32_t *)((uint32_t)msp_init_val - THREAD_STACK_SIZE);
    allocated_threads = 0;

    // Initialize the thread struct
    current_thread.sp = NULL;
    current_thread.thread_function = NULL;

    printf("Kernel stack pool starts at: %p\r\n", next_stack_ptr);
    printf("Thread struct initialized\r\n");
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
 * @return true if thread was successfully created, false if failed (no stack space)
 */
bool osCreateThread(void (*thread_func)(void *))
{
    printf("Creating new thread...\r\n");

    // Step 1: Allocate a new stack
    uint32_t *new_stack_top = allocate_thread_stack();
    if (new_stack_top == NULL)
    {
        printf("ERROR: Failed to allocate stack for new thread\r\n");
        return false;
    }

    // Step 2: Set up the stack frame (like we did in Lab 2)
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

    // Step 3: Update the kernel data structures
    current_thread.sp = stack_ptr;                // Save the stack pointer
    current_thread.thread_function = thread_func; // Save the function pointer

    printf("Thread created successfully!\r\n");
    printf("  Stack top: %p\r\n", new_stack_top);
    printf("  Stack pointer: %p\r\n", current_thread.sp);
    printf("  Function: %p\r\n", current_thread.thread_function);

    return true;
}

void SVC_Handler_Main(unsigned int *svc_args)
{
    unsigned int svc_number;
    /*
     * Stack contains:
     * r0, r1, r2, r3, r12, r14, the return address and xPSR
     * First argument (r0) is svc_args[0]
     */
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
        // Modified: Now use the global thread struct's stack pointer
        printf("SVC: Setting PSP to thread stack: %p\r\n", current_thread.sp);
        __set_PSP((uint32_t)current_thread.sp);
        runFirstThread();
        break;
    case 17: // 17 is sort of arbitrarily chosen
        printf("Success!\r\n");
        break;
    default: /* unknown SVC */
        break;
    }
}
