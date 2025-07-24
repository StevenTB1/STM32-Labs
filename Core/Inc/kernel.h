#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h> // For bool type

/* Kernel Defines */
#define RUN_FIRST_THREAD 0x3
#define THREAD_STACK_SIZE 0x400 // 1024 bytes, divisible by 8 for ARM alignment

/* Thread Structure Definition */
typedef struct k_thread
{
    uint32_t *sp;                    // stack pointer
    void (*thread_function)(void *); // function pointer
} thread;

/* Kernel Global Variables */
extern uint32_t *stackptr;
extern uint32_t *next_stack_ptr;   // Points to the next available stack location
extern uint32_t max_threads;       // Maximum number of threads we can support
extern uint32_t allocated_threads; // Number of threads currently allocated
extern uint32_t *msp_initial;      // Store initial MSP value

/* Global Thread Instance */
extern thread current_thread; // Single thread instance (will become array in Lab 4)

/* Kernel Function Declarations */
void osKernelInitialize(void); // Initialize kernel before creating threads
void osKernelStart(void);      // Start running threads
void SVC_Handler_Main(unsigned int *svc_args);
uint32_t *allocate_thread_stack(void);            // Returns pointer to top of new stack or NULL
bool osCreateThread(void (*thread_func)(void *)); // Create and setup a new thread

/* Internal kernel functions */
void kernel_init(uint32_t *msp_init_val); // Initialize kernel stack allocation

/* Assembly function declarations */
extern void runFirstThread(void);

#endif /* KERNEL_H */
