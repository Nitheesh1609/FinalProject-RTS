#include "scheduler.h"

#define schedUSE_TCB_ARRAY 1

#if(schedOverhead == 1)
	TaskHandle_t xLastTask = NULL;
static TickType_t SchedTimer = 0;
#endif 

/* Extended Task control block for managing periodic tasks within this library. */
typedef struct xExtended_TCB
{
	TaskFunction_t pvTaskCode; 		/* Function pointer to the code that will be run periodically. */
	const char *pcName; 			/* Name of the task. */
	UBaseType_t uxStackDepth; 			/* Stack size of the task. */
	void *pvParameters; 			/* Parameters to the task function. */
  UBaseType_t uxPriority;     /* Priority of the task. */
  UBaseType_t uxBasePriority;     /* Priority of the task. */
	TaskHandle_t *pxTaskHandle;		/* Task handle for the task. */
	TickType_t xReleaseTime;		/* Release time of the task. */
	TickType_t xRelativeDeadline;	/* Relative deadline of the task. */
	TickType_t xAbsoluteDeadline;	/* Absolute deadline of the task. */
	TickType_t xPeriod;				/* Task period. */
	TickType_t xLastWakeTime; 		/* Last time stamp when the task was running. */
	TickType_t xMaxExecTime;		/* Worst-case execution time of the task. */
	TickType_t xExecTime;			/* Current execution time of the task. */

	BaseType_t xWorkIsDone; 		/* pdFALSE if the job is not finished, pdTRUE if the job is finished. */

	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xPriorityIsSet; 	/* pdTRUE if the priority is assigned. */
		BaseType_t xInUse; 			/* pdFALSE if this extended TCB is empty. */
	#endif

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		BaseType_t xExecutedOnce;	/* pdTRUE if the task has executed once. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 || schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		TickType_t xAbsoluteUnblockTime; /* The task will be unblocked at this time if it is blocked by the scheduler task. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME || schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		BaseType_t xSuspended; 		/* pdTRUE if the task is suspended. */
		BaseType_t xMaxExecTimeExceeded; /* pdTRUE when execTime exceeds maxExecTime. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	
  #if( schedUSE_POLLING_SERVER == 1 )
    BaseType_t xIsPollingServer; /* pdTRUE if the task is a polling server. */
  #endif /* schedUSE_POLLING_SERVER */
  
	/* add if you need anything else */	
	
} SchedTCB_t;


#if( schedUSE_APERIODIC_TASKS == 1 )
  /* Aperiodic Tasks Control Block for mannaging Aperiodic Tasks. */
  typedef struct xAperiodicTaskControlBlock
  {
    TaskFunction_t pvTaskCode;  
    const char *pcName;     
    void *pvParameters;     
    TickType_t xMaxExecTime;  
    TickType_t xExecTime; 
	TickType_t xStartTime;
  } ATCB_t;
#endif /* schedUSE_APERIODIC_TASKS */

TaskHandle_t xPreviousTaskHandle = NULL;


#if( schedUSE_TCB_ARRAY == 1 )
	static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle );
	static void prvInitTCBArray( void );
	/* Find index for an empty entry in xTCBArray. Return -1 if there is no empty entry. */
	static BaseType_t prvFindEmptyElementIndexTCB( void );
	/* Remove a pointer to extended TCB from xTCBArray. */
	static void prvDeleteTCBFromArray( BaseType_t xIndex );
#endif /* schedUSE_TCB_ARRAY */

static TickType_t xSystemStartTime = 0;

static void prvPeriodicTaskCode( void *pvParameters );
static void prvCreateAllTasks( void );


#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
	static void prvSetFixedPriorities( void );	
#endif /* schedSCHEDULING_POLICY_RMS */

#if( schedUSE_SCHEDULER_TASK == 1 )
	static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB );
	static void prvSchedulerFunction( void );
	static void prvCreateSchedulerTask( void );
	static void prvWakeScheduler( void );

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB );
		static void prvDeadlineMissedHook( SchedTCB_t *pxTCB, TickType_t xTickCount );
		static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount );				
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask );
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_POLLING_SERVER == 1 )
  static void prvPollingServerFunction( void );
  void prvPollingServerCreate( void );
#endif /* schedUSE_POLLING_SERVER */

#if( schedUSE_POLLING_SERVER == 1 )
  static TaskHandle_t xPollingServerHandle = NULL;
  #if( schedUSE_APERIODIC_TASKS == 1 )
    static ATCB_t *pxCurrentAperiodicTask;
  #endif /* schedUSE_APERIODIC_TASKS */
#endif /* schedUSE_POLLING_SERVER */

#if( schedUSE_APERIODIC_TASKS == 1 )
  static ATCB_t *prvGetNextAperiodicTask( void );
  static BaseType_t prvFindEmptyElementIndexATCB( void );
#endif /* schedUSE_APERIODIC_TASKS */

#if( schedUSE_TCB_ARRAY == 1 )
	/* Array for extended TCBs. */
	static SchedTCB_t xTCBArray[ schedMAX_NUMBER_OF_PERIODIC_TASKS ] = { 0 };
	/* Counter for number of periodic tasks. */
	static BaseType_t xTaskCounter = 0;
#endif /* schedUSE_TCB_ARRAY */

#if( schedUSE_APERIODIC_TASKS == 1 )
  /* Array for extended ATCBs (Aperiodic Job Control Block). */
  static ATCB_t xATCBQueue[ schedMAX_NUMBER_OF_APERIODIC_TASKS ] = { 0 };
  static BaseType_t xATCBQueueHead = 0;
  static BaseType_t xATCBQueueTail = 0;
  static UBaseType_t uxAperiodicTaskCounter = 0;
#endif /* schedUSE_APERIODIC_TASKS */

#if( schedUSE_SCHEDULER_TASK )
	static TickType_t xSchedulerWakeCounter = 0; /* useful. why? */
	static TaskHandle_t xSchedulerHandle = NULL; /* useful. why? */
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_TCB_ARRAY == 1 )
	/* Returns index position in xTCBArray of TCB with same task handle as parameter. */
	static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle )
	{
		static BaseType_t xIndex = 0;
		BaseType_t xIterator;

		for( xIterator = 0; xIterator < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIterator++ )
		{
		
			if( pdTRUE == xTCBArray[ xIndex ].xInUse && *xTCBArray[ xIndex ].pxTaskHandle == xTaskHandle )
			{
				return xIndex;
			}
		
			xIndex++;
			if( schedMAX_NUMBER_OF_PERIODIC_TASKS == xIndex )
			{
				xIndex = 0;
			}
		}
		return -1;
	}

	/* Initializes xTCBArray. */
	static void prvInitTCBArray( void )
	{
	UBaseType_t uxIndex;
		for( uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
		{
			xTCBArray[ uxIndex ].xInUse = pdFALSE;
		}
	}

	/* Find index for an empty entry in xTCBArray. Returns -1 if there is no empty entry. */
	static BaseType_t prvFindEmptyElementIndexTCB( void )
	{
		/* your implementation goes here */
		BaseType_t uxIndex;
		for( uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
		{
			if(xTCBArray[ uxIndex ].xInUse == pdFALSE)
			{
				return uxIndex;
			}
		}
		return -1;
	}

	/* Remove a pointer to extended TCB from xTCBArray. */
	static void prvDeleteTCBFromArray( BaseType_t xIndex )
	{
		/* your implementation goes here */
		configASSERT(xIndex >= 0 && xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS );
		configASSERT(pdTRUE == xTCBArray[ xIndex ].xInUse );

		if( xTCBArray[xIndex].xInUse == pdTRUE)
		{
			xTCBArray[xIndex].xInUse = pdFALSE;
			xTaskCounter--;
		}
	}
	
#endif /* schedUSE_TCB_ARRAY */


/* The whole function code that is executed by every periodic task.
 * This function wraps the task code specified by the user. */
static void prvPeriodicTaskCode( void *pvParameters )
{
 
	SchedTCB_t *pxThisTask;	
	TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();  
	BaseType_t Index = prvGetTCBIndexFromHandle(xCurrentTaskHandle);

	pxThisTask = &xTCBArray[Index];
	
   	if( pxThisTask == NULL){
		   return;
	}

	if( pxThisTask->xReleaseTime != 0 )
	{
		xTaskDelayUntil( &pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime );
	}
    
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
        /* your implementation goes here */
		pxThisTask->xExecutedOnce = pdTRUE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
    
	if( 0 == pxThisTask->xReleaseTime )
	{
		pxThisTask->xLastWakeTime = xSystemStartTime;
	}

	for( ; ; )
	{	
    	//Serial.print("Starting ");
    	//Serial.println(pxThisTask->pcName);
		/* Execute the task function specified by the user. */
		#if(schedOverhead == 1)
			if(xLastTask == xSchedulerHandle) {	
			TickType_t EndSched = xTaskGetTickCount();
			SchedTimer = EndSched - SchedTimer;
			//Serial.print("Schduler Taken Time: ");
			//Serial.println(SchedTimer);

			if(SchedTimer == 0){
				SchedTimer = 1;
			}	
			}
		#endif 
		
		pxThisTask->xWorkIsDone = pdFALSE;
		pxThisTask->pvTaskCode( pvParameters );
		pxThisTask->xWorkIsDone = pdTRUE;  
		pxThisTask->xExecTime = 0 ;
       	//Serial.print("Ending ");
    	//Serial.println(pxThisTask->pcName);
		#if(schedOverhead == 1) 
			xLastTask = xCurrentTaskHandle;
			pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xRelativeDeadline + pxThisTask->xPeriod + SchedTimer;       
		#else
			pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xRelativeDeadline + pxThisTask->xPeriod;
		#endif


		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xPeriod);
		
	}
}

/* Creates a periodic task. */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
		TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick )
{
	taskENTER_CRITICAL();
	SchedTCB_t *pxNewTCB;
	
	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex = prvFindEmptyElementIndexTCB();
		configASSERT( xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS );
		configASSERT( xIndex != -1 );
		pxNewTCB = &xTCBArray[ xIndex ];	
	#endif /* schedUSE_TCB_ARRAY */

	/* Intialize item. */
		
	pxNewTCB->pvTaskCode = pvTaskCode;
	pxNewTCB->pcName = pcName;
	pxNewTCB->uxStackDepth = uxStackDepth;
	pxNewTCB->pvParameters = pvParameters;
  	pxNewTCB->uxPriority = uxPriority;
  	pxNewTCB->uxBasePriority = uxPriority;
	pxNewTCB->pxTaskHandle = pxCreatedTask;
	pxNewTCB->xReleaseTime = xPhaseTick;
	pxNewTCB->xPeriod = xPeriodTick;

    /* Populate the rest */
    /* your implementation goes here */
	pxNewTCB->xAbsoluteDeadline = xDeadlineTick;
    pxNewTCB->xMaxExecTime = xMaxExecTimeTick;
	pxNewTCB->xRelativeDeadline =xDeadlineTick;
	pxNewTCB->xWorkIsDone = pdTRUE;
	pxNewTCB->xExecTime = 0;


	#if( schedUSE_TCB_ARRAY == 1 )
		pxNewTCB->xInUse = pdTRUE;
	#endif /* schedUSE_TCB_ARRAY */
	
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
		/* member initialization */
        /* your implementation goes here */
		pxNewTCB->xPriorityIsSet = pdFALSE;
	#endif /* schedSCHEDULING_POLICY */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		/* member initialization */
        /* your implementation goes here */
		pxNewTCB->xExecutedOnce = pdFALSE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
        pxNewTCB->xSuspended = pdFALSE;
        pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
		//Serial.println(pxNewTCB->xMaxExecTimeExceeded);
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */	
  
  #if( schedUSE_POLLING_SERVER == 1)
    pxNewTCB->xIsPollingServer = pdFALSE;
  #endif /* schedUSE_POLLING_SERVER */
	#if( schedUSE_TCB_ARRAY == 1 )
		xTaskCounter++;	
	#endif /* schedUSE_TCB_ARRAY */
	taskEXIT_CRITICAL();
  	Serial.print(pxNewTCB->pcName);
	Serial.print(" is created.");
  	Serial.print('\n');
   //Serial.println(pdMS_TO_TICKS(17));
}

/* Deletes a periodic task. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle )
{
	/* your implementation goes here */
	
	BaseType_t xIndex = -1;
	
	/* your implementation goes here */
	if(xTaskHandle != NULL)
	{
		xIndex = prvGetTCBIndexFromHandle(xTaskHandle);
		if(xIndex < 0)
		{
			return;
		}
	  
		prvDeleteTCBFromArray( xIndex );
		vTaskDelete( xTaskHandle );
	}
}

/* Creates all periodic tasks stored in TCB array, or TCB list. */
static void prvCreateAllTasks( void )
{
	SchedTCB_t *pxTCB;

	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex;
		for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
		{
			configASSERT( pdTRUE == xTCBArray[ xIndex ].xInUse );
			pxTCB = &xTCBArray[ xIndex ];

			BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle );
					
		}	
	#endif /* schedUSE_TCB_ARRAY */
}

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
	/* Initiazes fixed priorities of all periodic tasks with respect to RMS policy. */
static void prvSetFixedPriorities( void )
{
	BaseType_t xIter, xIndex;
	TickType_t xShortest, xPreviousShortest=0;
	SchedTCB_t *pxShortestTaskPointer, *pxTCB;

	#if( schedUSE_SCHEDULER_TASK == 1 )
		BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY; 
	#else
		BaseType_t xHighestPriority = configMAX_PRIORITIES;
	#endif /* schedUSE_SCHEDULER_TASK */

	for( xIter = 0; xIter < xTaskCounter; xIter++ )
	{
		xShortest = portMAX_DELAY;

		/* search for shortest period */
		for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
		{
			/* your implementation goes here */
			pxTCB = &xTCBArray[ xIndex ];
			if(pxTCB->xPriorityIsSet == pdTRUE)
			{
				continue;
			}
			#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS )
				/* your implementation goes here */
				if( pxTCB->xPeriod <= xShortest )
				{
					xShortest = pxTCB->xPeriod;
					pxShortestTaskPointer = pxTCB;
				}
			#endif /* schedSCHEDULING_POLICY */

			#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
				/* your implementation goes here */
				if( pxTCB->xRelativeDeadline <= xShortest )
				{
					xShortest = pxTCB->xPeriod;
					pxShortestTaskPointer = pxTCB;
				}
			#endif /* schedSCHEDULING_POLICY */
		}
		
		/* set highest priority to task with xShortest period (the highest priority is configMAX_PRIORITIES-1) */		
		
		/* your implementation goes here */
		if( xPreviousShortest != xShortest && xHighestPriority > 0)
		{
			xHighestPriority--;
		}

    pxShortestTaskPointer->uxPriority = xHighestPriority;
    pxShortestTaskPointer->uxBasePriority = xHighestPriority;
    //Serial.print("==> ");
    //Serial.print(pxShortestTaskPointer->pcName);
    //Serial.println(pxShortestTaskPointer->uxPriority);
    pxShortestTaskPointer->xPriorityIsSet = pdTRUE;
		xPreviousShortest = xShortest;
	}
}
#endif /* schedSCHEDULING_POLICY */


#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

	/* Recreates a deleted task that still has its information left in the task array (or list). */
	static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB )
	{
		BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle );
		if( pdPASS == xReturnValue )
		{
			/* your implementation goes here */	
			Serial.print(pxTCB->pcName);
			Serial.print(" is recreated. ");	
			pxTCB->xExecutedOnce = pdFALSE;
			#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
				pxTCB->xSuspended = pdFALSE;
				pxTCB->xMaxExecTimeExceeded = pdFALSE;
			#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */	
		}
		else
		{
			/* if task creation failed */
		}
	}

	/* Called when a deadline of a periodic task is missed.
	 * Deletes the periodic task that has missed it's deadline and recreate it.
	 * The periodic task is released during next period. */
	static void prvDeadlineMissedHook( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{
		/* Delete the pxTask and recreate it. */ 
		//Serial.print(pxTCB->pcName);
		//Serial.print(" has missed deadline. ");
		vTaskDelete( *pxTCB->pxTaskHandle);
		prvPeriodicTaskRecreate( pxTCB );	
		pxTCB->xExecTime = 0;
		/* Need to reset next WakeTime for correct release. */
		/* your implementation goes here */
		pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
		pxTCB->xLastWakeTime = 0;
		pxTCB->xAbsoluteDeadline = pxTCB->xRelativeDeadline + pxTCB->xReleaseTime;
		
	}

	/* Checks whether given task has missed deadline or not. */
	static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{ 
		/* check whether deadline is missed. */     		
		/* your implementation goes here */
    if(pxTCB->xIsPollingServer == pdTRUE)
      return;
		if( (pxTCB != NULL) && (pxTCB->xWorkIsDone == pdFALSE) && (pxTCB->xExecutedOnce == pdTRUE))
		{
			pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xRelativeDeadline;
		
			if( ( signed ) ( pxTCB->xAbsoluteDeadline - xTickCount ) < 0 )
			{
				
				prvDeadlineMissedHook( pxTCB, xTickCount );
			}
		}
			
	}	
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */


#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

	/* Called if a periodic task has exceeded its worst-case execution time.
	 * The periodic task is blocked until next period. A context switch to
	 * the scheduler task occur to block the periodic task. */
	static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask )
	{

        pxCurrentTask->xMaxExecTimeExceeded = pdTRUE;
		//Serial.print(pxCurrentTask->pcName);
		//Serial.println(" has exceeded. Suspending the task. ");
        /* Is not suspended yet, but will be suspended by the scheduler later. */
        pxCurrentTask->xSuspended = pdTRUE;
        pxCurrentTask->xAbsoluteUnblockTime = pxCurrentTask->xLastWakeTime + pxCurrentTask->xPeriod;
        pxCurrentTask->xExecTime = 0;
        #if( schedUSE_POLLING_SERVER == 1)
        if( pdTRUE == pxCurrentTask->xIsPollingServer )
        {
           pxCurrentTask->xAbsoluteDeadline = pxCurrentTask->xAbsoluteUnblockTime + pxCurrentTask->xRelativeDeadline;
        }
    #endif /* schedUSE_POLLING_SERVER */
        BaseType_t xHigherPriorityTaskWoken;
        vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
        xTaskResumeFromISR(xSchedulerHandle);
	}
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */


#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Called by the scheduler task. Checks all tasks for any enabled
	 * Timing Error Detection feature. */
	static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB )
	{
		/* your implementation goes here */
		if(pxTCB->xInUse == pdFALSE)
		{
			return;
		}

		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )						
			/* check if task missed deadline */
            /* your implementation goes here */
			if( ( signed ) ( xTickCount - pxTCB->xLastWakeTime ) > 0 )
			{
				pxTCB->xWorkIsDone = pdFALSE;
			}
			
			prvCheckDeadline( pxTCB, xTickCount );						
		#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
		

		#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
	
        if(pxTCB->xMaxExecTimeExceeded == pdTRUE)
        {
            pxTCB->xMaxExecTimeExceeded = pdFALSE;
			
            vTaskSuspend( *pxTCB->pxTaskHandle );
        }
        if(pxTCB->xSuspended == pdTRUE)
        {
            if( ( signed ) ( pxTCB->xAbsoluteUnblockTime - xTickCount ) <= 0 )
            {
                pxTCB->xSuspended = pdFALSE;
				pxTCB->xLastWakeTime = xTickCount;
                vTaskResume( *pxTCB->pxTaskHandle );
            }
        }
		#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

		return;
	}



	/* Function code for the scheduler task. */
	static void prvSchedulerFunction( void *pvParameters )
	{

    
		for( ; ; )
		{ 
			//Serial.println("Scheduler task!!");
			#if(schedOverhead == 1)
				SchedTimer = xTaskGetTickCount();
			#endif

			
     		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
				TickType_t xTickCount = xTaskGetTickCount();
        		SchedTCB_t *pxTCB;
        		
				/* your implementation goes here. */			
				/* You may find the following helpful...
                    prvSchedulerCheckTimingError( xTickCount, pxTCB );
                 */
				BaseType_t xIndex;
				for( xIndex = 0; xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIndex++ )
				{
					pxTCB = &xTCBArray[ xIndex ];
					prvSchedulerCheckTimingError( xTickCount, pxTCB );
				}

		#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

			#if(schedOverhead == 1)
				xLastTask = xSchedulerHandle;
			#endif
			ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		}
	}

	/* Creates the scheduler task. */
	static void prvCreateSchedulerTask( void )
	{
		xTaskCreate( (TaskFunction_t) prvSchedulerFunction, "Scheduler", schedSCHEDULER_TASK_STACK_SIZE, NULL, schedSCHEDULER_PRIORITY, &xSchedulerHandle );                             
	}
#endif /* schedUSE_SCHEDULER_TASK */

#if( schedUSE_POLLING_SERVER == 1 )

  static void prvPollingServerFunction( void )
  {
    for( ; ; )
    {
      #if( schedUSE_APERIODIC_TASKS == 1 )
        pxCurrentAperiodicTask = prvGetNextAperiodicTask();
        if( NULL == pxCurrentAperiodicTask )
        {
          /* No ready aperiodic job in the queue. */
          return;
        }
        else
        {
          /* Run aperiodic job */
          pxCurrentAperiodicTask->pvTaskCode( pxCurrentAperiodicTask->pvParameters );
 		  TickType_t EndSched = xTaskGetTickCount();
		  TickType_t ResponseTime = EndSched - pxCurrentAperiodicTask->xStartTime;
          uxAperiodicTaskCounter--;
          Serial.print("Response Time of ");
		  Serial.print(pxCurrentAperiodicTask->pcName);
		  Serial.print(" is ");
          Serial.println(ResponseTime);

        }
      #endif /* schedUSE_APERIODIC_TASKS */
    }
  }

  /* Creates Polling Server as a periodic task. */
  void prvPollingServerCreate( void )
  {
    taskENTER_CRITICAL();
  SchedTCB_t *pxNewTCB;
    #if( schedUSE_TCB_ARRAY == 1 )
      BaseType_t xIndex = prvFindEmptyElementIndexTCB();
      configASSERT( xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS );
      configASSERT( xIndex != -1 );
      pxNewTCB = &xTCBArray[ xIndex ];
    #endif /* schedUSE_TCB_ARRAY */

    /* Initialize item. */
  pxNewTCB->pvTaskCode = (TaskFunction_t) prvPollingServerFunction; 
  pxNewTCB->pcName = "PS";
  pxNewTCB->uxStackDepth = schedPOLLING_SERVER_STACK_SIZE;
  pxNewTCB->pvParameters = NULL;
  pxNewTCB->pxTaskHandle = &xPollingServerHandle;
  pxNewTCB->xReleaseTime = 0;
  pxNewTCB->xPeriod = schedPOLLING_SERVER_PERIOD;
  pxNewTCB->xMaxExecTime = schedPOLLING_SERVER_MAX_EXECUTION_TIME;
  pxNewTCB->xRelativeDeadline = schedPOLLING_SERVER_DEADLINE;
  pxNewTCB->xWorkIsDone = pdTRUE;
  pxNewTCB->xExecTime = 0;
  pxNewTCB->xIsPollingServer = pdTRUE;
  
  pxNewTCB->xInUse = pdTRUE;
    #if( schedUSE_TCB_ARRAY == 1 )
      pxNewTCB->xInUse = pdTRUE;
    #endif /* schedUSE_TCB_ARRAY */

    #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
      pxNewTCB->xPriorityIsSet = pdFALSE;
    #endif /* schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY_DMS */
    #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_MANUAL )
      pxNewTCB->uxPriority = schedPOLLING_SERVER_PRIORITY;
      pxNewTCB->xPriorityIsSet = pdTRUE;
    #endif /* schedSCHEDULING_POLICY_MANUAL */
    #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
      pxNewTCB->xSuspended = pdFALSE;
      pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
    #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
  
    #if( schedUSE_TCB_ARRAY == 1 )
      xTaskCounter++;
    #endif /* schedUSE_TCB_ARRAY */
      taskEXIT_CRITICAL();
  }
#endif /* schedUSE_POLLING_SERVER */

#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Wakes up (context switches to) the scheduler task. */
	static void prvWakeScheduler( void )
	{
		BaseType_t xHigherPriorityTaskWoken;
		vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
		xTaskResumeFromISR(xSchedulerHandle);    
	}

	/* Called every software tick. */
	void vApplicationTickHook( void )
	{            
		SchedTCB_t *pxCurrentTask;		
		TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();		
        UBaseType_t flag = 0;
        BaseType_t xIndex;

		xIndex = prvGetTCBIndexFromHandle(xCurrentTaskHandle);
		if(xIndex >= 0){
			pxCurrentTask = &xTCBArray[xIndex];
		}

  

		if( xCurrentTaskHandle != xSchedulerHandle && xCurrentTaskHandle != xTaskGetIdleTaskHandle() )
		{
			
			pxCurrentTask->xExecTime++;     
			#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
            if( pxCurrentTask->xMaxExecTime <= pxCurrentTask->xExecTime )
            {
				
                if(pxCurrentTask->xMaxExecTimeExceeded == pdFALSE)
                {
                    if(pxCurrentTask->xSuspended == pdFALSE)
                    {
                        prvExecTimeExceedHook( xTaskGetTickCountFromISR(), pxCurrentTask );
                    }
                }
            }
			#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
		}

		
		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )    
			xSchedulerWakeCounter++;      
			if( xSchedulerWakeCounter == schedSCHEDULER_TASK_PERIOD )
			{
				xSchedulerWakeCounter = 0;        
				prvWakeScheduler();
			}
		#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	}
#endif /* schedUSE_SCHEDULER_TASK */

/* Check and change priority when resource contention. - PIP */
void vSchedlerPriorityInversion(TaskHandle_t xSemHoldTaskHandle, TaskHandle_t xCurrentTaskHandle)
{
  SchedTCB_t *pxThisTask;
  SchedTCB_t *pxSemHoldingTask;
  BaseType_t xIndex;
  
  xIndex = prvGetTCBIndexFromHandle(xSemHoldTaskHandle);
  configASSERT(xIndex >= 0);
  pxSemHoldingTask = &xTCBArray[xIndex];
  
  xIndex = prvGetTCBIndexFromHandle(xCurrentTaskHandle);
  configASSERT(xIndex >= 0);
  pxThisTask = &xTCBArray[xIndex];

  //Serial.println("Priority Inverted");
  if(pxThisTask->uxPriority >  pxSemHoldingTask->uxPriority){
    pxSemHoldingTask->uxPriority = pxThisTask->uxPriority;
    Serial.println("Priority Inverted");
  }
}

/* Set Ceil priority of the resource */
void vSetCeil(struct shareInt *res){
  SchedTCB_t *pxThisTask;
  SchedTCB_t *pxTempTask;
  BaseType_t xIndex;
  res->ceil_res = 0;
  for(int i=0; i<5; i++){
    if(*res->handle[i]){      
      xIndex = prvGetTCBIndexFromHandle(*res->handle[i]);
      configASSERT(xIndex >= 0);
      pxTempTask = &xTCBArray[xIndex];
      if(pxTempTask->uxBasePriority >  res->ceil_res){
          res->ceil_res = pxTempTask->uxBasePriority;
      }
    }
  }
 }
 
/* Check and change priority when resource contention - PCP. */
int vSchedlerPriorityInversionPCP(TaskHandle_t xSemHoldTaskHandle, TaskHandle_t xCurrentTaskHandle, int tempCeil)
{
  SchedTCB_t *pxThisTask;
  SchedTCB_t *pxSemHoldingTask;
  BaseType_t xIndex;
  int ret=0;
  
  xIndex = prvGetTCBIndexFromHandle(xSemHoldTaskHandle);
  configASSERT(xIndex >= 0);
  pxSemHoldingTask = &xTCBArray[xIndex];
  
  xIndex = prvGetTCBIndexFromHandle(xCurrentTaskHandle);
  configASSERT(xIndex >= 0);
  pxThisTask = &xTCBArray[xIndex];

  if(pxThisTask->uxPriority <=  tempCeil){
    pxSemHoldingTask->uxPriority = pxThisTask->uxPriority;
    Serial.println("Priority Inverted");
    ret = 1;
  }
  return ret;
}

/* Revert to base priority when exiting critical section. */
void vSchedlerRevertPriority(TaskHandle_t xCurrTaskHandle)
{
  SchedTCB_t *pxThisTask;
  BaseType_t xIndex;
  
  xIndex = prvGetTCBIndexFromHandle(xCurrTaskHandle);
  configASSERT(xIndex >= 0);
  pxThisTask = &xTCBArray[xIndex];

  if(pxThisTask->uxPriority != pxThisTask->uxBasePriority){
    pxThisTask->uxPriority = pxThisTask->uxBasePriority;
    Serial.println("Priority Revert");
  }
}


#if( schedUSE_APERIODIC_TASKS == 1 )
  /* Returns ATCB of first aperiodic job stored in Queue. Returns NULL if
   * the Queue is empty. */
  static ATCB_t *prvGetNextAperiodicTask( void )
  {
    /* If there is no aperiodic tasks in the ATCBarray. */
    if( 0 == uxAperiodicTaskCounter)
    {
      return NULL;
    }

    ATCB_t *pxReturnValue = &xATCBQueue[ xATCBQueueHead ];

    /* As aperiodic tasks are executed, head is incremented and if reached end of the array, reseted to 0 */
    xATCBQueueHead++;
    if( schedMAX_NUMBER_OF_APERIODIC_TASKS == xATCBQueueHead )
    {
      xATCBQueueHead = 0;
    }

    return pxReturnValue;
  }

  /* Find index for an empty entry in xATCBArray. Returns -1 if there is
   * no empty entry. */
  static BaseType_t prvFindEmptyElementIndexATCB( void )
  {
    /* If the number of Aperiodic task is the max apriodic tasks allowed. */
    if( schedMAX_NUMBER_OF_APERIODIC_TASKS == uxAperiodicTaskCounter )
    {
      return -1;
    }

    BaseType_t xReturnValue = xATCBQueueTail;

    /* As aperiodic tasks are added, tail is incremented and if reached end of the array, reseted to 0 */
    xATCBQueueTail++;
    if( schedMAX_NUMBER_OF_APERIODIC_TASKS == xATCBQueueTail )
    {
      xATCBQueueTail = 0;
    }

    return xReturnValue;
  }

  /* Creates an aperiodic job. */
  void vSchedulerAperiodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, void *pvParameters, TickType_t xMaxExecTimeTick )
  {
    taskENTER_CRITICAL();
  BaseType_t xIndex = prvFindEmptyElementIndexATCB();
    if( -1 == xIndex)
    {
      /* The ATCBQueue is full. */
      taskEXIT_CRITICAL();
      return;
    }
    configASSERT( uxAperiodicTaskCounter < schedMAX_NUMBER_OF_APERIODIC_TASKS );
    ATCB_t *pxNewATCB = &xATCBQueue[ xIndex ];

    /* Add item to ATCBList. */
    *pxNewATCB = ( ATCB_t ) { .pvTaskCode = pvTaskCode, .pcName = pcName, .pvParameters = pvParameters, .xMaxExecTime = xMaxExecTimeTick, .xExecTime = 0,.xStartTime = xTaskGetTickCount()};
    
    uxAperiodicTaskCounter++;
    taskEXIT_CRITICAL();
  }
#endif /* schedUSE_APERIODIC_JOBS */

/* This function must be called before any other function call from this module. */
void vSchedulerInit( void )
{
	#if( schedUSE_TCB_ARRAY == 1 )
		prvInitTCBArray();
	#endif /* schedUSE_TCB_ARRAY */
}

/*  Initialize scheduling tasks. . All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStartInit()
{
  #if( schedUSE_POLLING_SERVER == 1 )
    prvPollingServerCreate();
  #endif /* schedUSE_POLLING_SERVER */
  
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
		prvSetFixedPriorities();	
	#endif /* schedSCHEDULING_POLICY */

	#if( schedUSE_SCHEDULER_TASK == 1 )
		prvCreateSchedulerTask();
	#endif /* schedUSE_SCHEDULER_TASK */

	prvCreateAllTasks();

	xSystemStartTime = xTaskGetTickCount();
}

/* Starts scheduling tasks. */
void vSchedulerStart()
{
  vTaskStartScheduler();
}
