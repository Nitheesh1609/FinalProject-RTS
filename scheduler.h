#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <projdefs.h>
#include <timers.h>
#include <list.h>
#include <croutine.h>
#include <portable.h>
#include <stack_macros.h>
#include <mpu_wrappers.h>
#include <FreeRTOSVariant.h>
#include <message_buffer.h>
#include <semphr.h>
#include <FreeRTOSConfig.h>
#include <stream_buffer.h>
#include <portmacro.h>
#include <event_groups.h>
#include <queue.h>
#include <Arduino.h>



#ifdef __cplusplus
extern "C" {
#endif

/* The scheduling policy can be chosen from one of these. */
#define schedSCHEDULING_POLICY_RMS 1 		/* Rate-monotonic scheduling */
#define schedSCHEDULING_POLICY_DMS 2

#define schedOverhead 1

/* Configure scheduling policy by setting this define to the appropriate one. */
#define schedSCHEDULING_POLICY schedSCHEDULING_POLICY_RMS //schedSCHEDULING_POLICY_EDF

/* Maximum number of periodic tasks that can be created. (Scheduler task is
 * not included) */
#define schedMAX_NUMBER_OF_PERIODIC_TASKS 5

/* Set this define to 1 to enable aperiodic jobs. */
#define schedUSE_APERIODIC_TASKS 1

#if( schedUSE_APERIODIC_TASKS == 1)
  /* Enable Polling Server. */
  #define schedUSE_POLLING_SERVER 1
#else
  /* Disable Polling Server. */
  #define schedUSE_POLLING_SERVER 0
#endif /* schedUSE_APERIODIC_TASKS */

#if ( schedUSE_APERIODIC_TASKS == 1 )
  /* Maximum number of aperiodic jobs. */
  #define schedMAX_NUMBER_OF_APERIODIC_TASKS 4
#endif /* schedUSE_APERIODIC_TASKS */


/* Set this define to 1 to enable Timing-Error-Detection for detecting tasks
 * that have missed their deadlines. Tasks that have missed their deadlines
 * will be deleted, recreated and restarted during next period. */
#define schedUSE_TIMING_ERROR_DETECTION_DEADLINE 1

/* Set this define to 1 to enable Timing-Error-Detection for detecting tasks
 * that have exceeded their worst-case execution time. Tasks that have exceeded
 * their worst-case execution time will be preempted until next period. */
#define schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME 1

/* Set this define to 1 to enable the scheduler task. This define must be set to 1
* when using following features:
* EDF scheduling policy, Timing-Error-Detection of execution time,
* Timing-Error-Detection of deadline, Polling Server. */
#define schedUSE_SCHEDULER_TASK 1


#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Priority of the scheduler task. */
	#define schedSCHEDULER_PRIORITY ( configMAX_PRIORITIES - 1 )
	/* Stack size of the scheduler task. */
	#define schedSCHEDULER_TASK_STACK_SIZE 200 
	/* The period of the scheduler task in software ticks. */
	#define schedSCHEDULER_TASK_PERIOD pdMS_TO_TICKS( 50 )	
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_POLLING_SERVER == 1 )
  /* The period of the Polling Server. */
  #define schedPOLLING_SERVER_PERIOD pdMS_TO_TICKS( 600 )
  /* Deadline of Polling Server will only be used for setting priority if
   * scheduling policy is DMS or EDF. Polling Server will not be preempted
   * when exceeding deadline if Timing-Error-Detection for deadline is
   * enabled. */
  #define schedPOLLING_SERVER_DEADLINE pdMS_TO_TICKS( 600 )
  /* Stack size of the Polling Server. */
  #define schedPOLLING_SERVER_STACK_SIZE 2000
  /* Execution budget of the Polling Server. */
  #define schedPOLLING_SERVER_MAX_EXECUTION_TIME pdMS_TO_TICKS( 200 )
  #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_MANUAL )
    /* Priority of the Polling Server if scheduling policy is manual. */
    #define schedPOLLING_SERVER_PRIORITY 5
  #endif /* schedSCHEDULING_POLICY_MANUAL */
#endif /* schedUSE_POLLING_SERVER */

struct shareInt {
  int res;
  int ceil_res;
  SemaphoreHandle_t xSem;
  TaskHandle_t* handle[5] = {NULL};
};

/* This function must be called before any other function call from scheduler.h. */
void vSchedulerInit( void );

/* Creates a periodic task.
 *
 * pvTaskCode: The task function.
 * pcName: Name of the task.
 * usStackDepth: Stack size of the task in words, not bytes.
 * pvParameters: Parameters to the task function.
 * uxPriority: Priority of the task. (Only used when scheduling policy is set to manual)
 * pxCreatedTask: Pointer to the task handle.
 * xPhaseTick: Phase given in software ticks. Counted from when vSchedulerStart is called.
 * xPeriodTick: Period given in software ticks.
 * xMaxExecTimeTick: Worst-case execution time given in software ticks.
 * xDeadlineTick: Relative deadline given in software ticks.
 * */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
		TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick );

/* Deletes a periodic task associated with the given task handle. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle );

/* Check and change priority when resource contention - PIP. */
void vSchedlerPriorityInversion(TaskHandle_t xSemHoldTaskHandle, TaskHandle_t xCurrTaskHandle);

/* Check and change priority when resource contention - PCP. */
int vSchedlerPriorityInversionPCP(TaskHandle_t xSemHoldTaskHandle, TaskHandle_t xCurrentTaskHandle, int tempCeil);

/* Set Ceil priority of the resource */
void vSetCeil(struct shareInt *res);

/* Revert to base priority when exiting critical section. */
void vSchedlerRevertPriority(TaskHandle_t xCurrTaskHandle);

#if ( schedUSE_APERIODIC_TASKS == 1 )
  /* Creates an aperiodic job.
   *
   * pvTaskCode: The job function.
   * pcName: Name of the job.
   * pvParameters: Parameters to the job function.
   * xMaxExecTimeTick: Worst-case execution time given in software ticks.
   * */
  void vSchedulerAperiodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, void *pvParameters, TickType_t xMaxExecTimeTick );
#endif /* schedUSE_APERIODIC_TASKS */

/* Initialize scheduling tasks. */
void vSchedulerStartInit();

/* Starts scheduling tasks. */
void vSchedulerStart();

#ifdef __cplusplus
}
#endif


#endif /* SCHEDULER_H_ */
