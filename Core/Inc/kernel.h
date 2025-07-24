#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* Kernel Defines */
#define RUN_FIRST_THREAD 0x3
#define YIELD 0x4
#define THREAD_STACK_SIZE 0x400
#define MAX_THREADS 8
#define DEFAULT_TIMESLICE_MS 5
#define SHPR2 *(uint32_t *)0xE000ED1C // for setting SVC priority, bits 31-24
#define SHPR3 *(uint32_t *)0xE000ED20 // PendSV is bits 23-16
#define _ICSR *(uint32_t *)0xE000ED04 // This lets us trigger PendSV

/* Thread Structure Definition */
typedef struct k_thread
{
    uint32_t *sp;                    // stack pointer
    void (*thread_function)(void *); // function pointer
    bool is_active;                  // whether this thread slot is in use
    uint32_t timeslice;              // ms this thread is allowed to run
    uint32_t runtime;                // ms left to run
} thread;

/* Kernel Global Variables */
extern uint32_t *stackptr;
extern uint32_t *next_stack_ptr;
extern uint32_t max_threads;
extern uint32_t allocated_threads;
extern uint32_t *msp_initial;

/* Global Thread Management */
extern thread threads[MAX_THREADS];
extern uint32_t num_created_threads;
extern uint32_t current_thread_index;
extern bool kernel_running;

/* Kernel Function Declarations */
void osKernelInitialize(void);
void osKernelStart(void);
void osSched(void);
void osYield(void);
void SVC_Handler_Main(unsigned int *svc_args);
uint32_t *allocate_thread_stack(void);
bool osCreateThread(void (*thread_func)(void *), void *args);
bool osCreateThreadWithDeadline(void (*thread_func)(void *), void *args, uint32_t deadline_ms);

/* Assembly function declarations */
extern void runFirstThread(void);
extern void PendSV_Handler(void);

#endif /* KERNEL_H */
