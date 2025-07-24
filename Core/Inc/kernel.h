#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h> // For bool type

/* Kernel Defines */
#define RUN_FIRST_THREAD 0x3
#define YIELD 0x4                     // New system call for yielding processor
#define THREAD_STACK_SIZE 0x400       // 1024 bytes, divisible by 8 for ARM alignment
#define MAX_THREADS 8                 // Maximum number of threads supported
#define SHPR2 *(uint32_t *)0xE000ED1C // for setting SVC priority, bits 31-24
#define SHPR3 *(uint32_t *)0xE000ED20 // PendSV is bits 23-16
#define _ICSR *(uint32_t *)0xE000ED04 // This lets us trigger PendSV

/* Thread Structure Definition */
typedef struct k_thread
{
    uint32_t *sp;                    // stack pointer
    void (*thread_function)(void *); // function pointer
    bool is_active;                  // thread slot is in use
} thread;

/* Kernel Global Variables */
extern uint32_t *stackptr;
extern uint32_t *next_stack_ptr;   // Points to the next available stack location
extern uint32_t max_threads;       // Maximum number of threads we can support
extern uint32_t allocated_threads; // Number of threads currently allocated
extern uint32_t *msp_initial;      // Store initial MSP value

/* Global Thread Management */
extern thread threads[MAX_THREADS];   // Array of thread instances
extern uint32_t num_created_threads;  // Number of threads created
extern uint32_t current_thread_index; // Index of currently running thread

/* Kernel Function Declarations */
void osKernelInitialize(void);   // Initialize kernel before creating threads
void osKernelStart(void);        // Start running threads
uint32_t *osGetInitialMSP(void); // Get the initial MSP value stored by kernel
void osSched(void);              // Context switch scheduler function
void osYield(void);              // Trigger a context switch via system call
void SVC_Handler_Main(unsigned int *svc_args);
uint32_t *allocate_thread_stack(void);            // Returns pointer to top of new stack or NULL
bool osCreateThread(void (*thread_func)(void *)); // Create and setup a new thread

/* Internal kernel functions */
void kernel_init(uint32_t *msp_init_val); // Initialize kernel stack allocation

/* Assembly function declarations */
extern void runFirstThread(void);

#endif /* KERNEL_H */
