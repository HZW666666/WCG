/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

 ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
 ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

 ***************************************************************************
 *                                                                       *
 *    FreeRTOS provides completely free yet professionally developed,    *
 *    robust, strictly quality controlled, supported, and cross          *
 *    platform software that is more than just the market leader, it     *
 *    is the industry's de facto standard.                               *
 *                                                                       *
 *    Help yourself get started quickly while simultaneously helping     *
 *    to support the FreeRTOS project by purchasing a FreeRTOS           *
 *    tutorial book, reference manual, or both:                          *
 *    http://www.FreeRTOS.org/Documentation                              *
 *                                                                       *
 ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
 */
// ****************************************************************************
/*
 * Creates all the demo application tasks, then starts the scheduler.  The WEB
 * documentation provides more details of the standard demo application tasks.
 * In addition to the standard demo tasks, the following tasks and tests are
 * defined and/or created within this file:
 *
 * "Fast Interrupt Test" - A high frequency periodic interrupt is generated
 * using a free running timer to demonstrate the use of the 
 * configKERNEL_INTERRUPT_PRIORITY configuration constant.  The interrupt 
 * service routine measures the number of processor clocks that occur between
 * each interrupt - and in so doing measures the jitter in the interrupt 
 * timing.  The maximum measured jitter time is latched in the usMaxJitter 
 * variable, and displayed on the LCD by the 'Check' as described below.  
 * The fast interrupt is configured and handled in the timer_test.c source 
 * file.
 *
 * "LCD" task - the LCD task is a 'gatekeeper' task.  It is the only task that
 * is permitted to access the LCD directly.  Other tasks wishing to write a
 * message to the LCD send the message on a queue to the LCD task instead of 
 * accessing the LCD themselves.  The LCD task just blocks on the queue waiting 
 * for messages - waking and displaying the messages as they arrive.  The LCD
 * task is defined in lcd.c.  
 * 
 * "Check" task -  This only executes every three seconds but has the highest 
 * priority so is guaranteed to get processor time.  Its main function is to 
 * check that all the standard demo tasks are still operational.  Should any
 * unexpected behaviour within a demo task be discovered the 'check' task will
 * write "FAIL #n" to the LCD (via the LCD task).  If all the demo tasks are 
 * executing with their expected behaviour then the check task writes the max
 * jitter time to the LCD (again via the LCD task), as described above.
 */

/* Standard includes. */
#include <stdio.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"

/* Demo application includes. */
#include "BlockQ.h"
#include "crflash.h"
#include "blocktim.h"
#include "integer.h"
#include "comtest2.h"
#include "partest.h"
#include "lcd.h"
#include "timertest.h"
#include "serial.h"


/* Demo task priorities. */
#define mainBLOCK_Q_PRIORITY				( tskIDLE_PRIORITY + 2 )
#define mainCHECK_TASK_PRIORITY				( tskIDLE_PRIORITY + 3 )
#define mainCOM_TEST_PRIORITY				( 2 )

/* The check task may require a bit more stack as it calls sprintf(). */
#define mainCHECK_TAKS_STACK_SIZE			( configMINIMAL_STACK_SIZE * 2 )

/* The execution period of the check task. */
#define mainCHECK_TASK_PERIOD				( ( TickType_t ) 3000 / portTICK_PERIOD_MS )

/* The number of flash co-routines to create. */
#define mainNUM_FLASH_COROUTINES			( 5 )

/* Baud rate used by the comtest tasks. */
#define mainCOM_TEST_BAUD_RATE				( 19200 )

/* The LED used by the comtest tasks.  mainCOM_TEST_LED + 1 is also used.
See the comtest.c file for more information. */
#define mainCOM_TEST_LED					( 6 )

/* The frequency at which the "fast interrupt test" interrupt will occur. */
#define mainTEST_INTERRUPT_FREQUENCY		( 20000 )

/* The number of processor clocks we expect to occur between each "fast
interrupt test" interrupt. */
#define mainEXPECTED_CLOCKS_BETWEEN_INTERRUPTS ( configCPU_CLOCK_HZ / mainTEST_INTERRUPT_FREQUENCY )

/* The number of nano seconds between each processor clock. */
#define mainNS_PER_CLOCK ( ( unsigned short ) ( ( 1.0 / ( double ) configCPU_CLOCK_HZ ) * 1000000000.0 ) )

/* Dimension the buffer used to hold the value of the maximum jitter time when
it is converted to a string. */
#define mainMAX_STRING_LENGTH				( 20 )

/*-----------------------------------------------------------*/

/*
 * The check task as described at the top of this file.
 */
//static void vCheckTask(void *pvParameters);

/*
 * Setup the processor ready for the demo.
 */
static void prvSetupHardware(void);

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);

/*-----------------------------------------------------------*/

/* The queue used to send messages to the LCD task. */
//static QueueHandle_t xLCDQueue;

/*-----------------------------------------------------------*/
void task_start(void *parameter);
UBaseType_t start_PRIORITY = 4;
UBaseType_t vLEDShark_PRIORITY = 2;
UBaseType_t vTaskStatu_PRIORITY = 3;

TaskHandle_t xStateTask;
//uint16_t configMINIMAL_STACK_SIZE = 128;
//uint16_t configMINIMAL_STACK_SIZE 128;
//uint16_t configMINIMAL_STACK_SIZE 128;

/*
 * Create the demo tasks then start the scheduler.
 */
int main(void) {//2¸öLEDµÆÉÁË¸
    /* Configure any hardware required for this demo. */
    prvSetupHardware();

    /* Create the standard demo tasks. */
    //    vStartBlockingQueueTasks(mainBLOCK_Q_PRIORITY);
    //    vStartIntegerMathTasks(tskIDLE_PRIORITY);
    //    vStartFlashCoRoutines(mainNUM_FLASH_COROUTINES);
    vAltStartComTestTasks(mainCOM_TEST_PRIORITY, mainCOM_TEST_BAUD_RATE, mainCOM_TEST_LED);
    //    vCreateBlockTimeTasks();

    /* Create the test tasks defined within this file. */
    //    xTaskCreate(vCheckTask, "Check", mainCHECK_TAKS_STACK_SIZE, NULL, mainCHECK_TASK_PRIORITY, NULL);

    /* Start the task that will control the LCD.  This returns the handle
    to the queue used to write text out to the task. */
    //	xLCDQueue = xStartLCDTask();
    xTaskCreate(task_start, "start_task", 512, NULL, start_PRIORITY, NULL); /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
    /* Start the high frequency interrupt test. */
    vSetupTimerTest(mainTEST_INTERRUPT_FREQUENCY);

    /* Finally start the scheduler. */
    vTaskStartScheduler();

    /* Will only reach here if there is insufficient heap available to start
    the scheduler. */
    return 0;
}

/*-----------------------------------------------------------*/

static void prvSetupHardware(void) {
    vParTestInitialise();
}

/*-----------------------------------------------------------*/

//static void vCheckTask(void *pvParameters) {
//    /* Used to wake the task at the correct frequency. */
//    TickType_t xLastExecutionTime;
//
//    /* The maximum jitter time measured by the fast interrupt test. */
//    extern unsigned short usMaxJitter;
//
//    /* Buffer into which the maximum jitter time is written as a string. */
//    static char cStringBuffer[ mainMAX_STRING_LENGTH ];
//
//    /* The message that is sent on the queue to the LCD task.  The first
//    parameter is the minimum time (in ticks) that the message should be
//    left on the LCD without being overwritten.  The second parameter is a pointer
//    to the message to display itself. */
//    xLCDMessage xMessage = {0, cStringBuffer};
//
//    /* Set to pdTRUE should an error be detected in any of the standard demo tasks. */
//    unsigned short usErrorDetected = pdFALSE;
//
//    /* Remove compiler warnings. */
//    (void) pvParameters;
//
//    /* Initialise xLastExecutionTime so the first call to vTaskDelayUntil()
//    works correctly. */
//    xLastExecutionTime = xTaskGetTickCount();
//
//    for (;;) {
//        /* Wait until it is time for the next cycle. */
//        vTaskDelayUntil(&xLastExecutionTime, mainCHECK_TASK_PERIOD);
//
//        /* Has an error been found in any of the standard demo tasks? */
//
//        if (xAreIntegerMathsTaskStillRunning() != pdTRUE) {
//            usErrorDetected = pdTRUE;
//            sprintf(cStringBuffer, "FAIL #1");
//        }
//
//        if (xAreComTestTasksStillRunning() != pdTRUE) {
//            usErrorDetected = pdTRUE;
//            sprintf(cStringBuffer, "FAIL #2");
//        }
//
//        if (xAreBlockTimeTestTasksStillRunning() != pdTRUE) {
//            usErrorDetected = pdTRUE;
//            sprintf(cStringBuffer, "FAIL #3");
//        }
//
//        if (xAreBlockingQueuesStillRunning() != pdTRUE) {
//            usErrorDetected = pdTRUE;
//            sprintf(cStringBuffer, "FAIL #4");
//        }
//
//        if (usErrorDetected == pdFALSE) {
//            /* No errors have been discovered, so display the maximum jitter
//            timer discovered by the "fast interrupt test". */
//            sprintf(cStringBuffer, "%dns max jitter", (short) (usMaxJitter - mainEXPECTED_CLOCKS_BETWEEN_INTERRUPTS) * mainNS_PER_CLOCK);
//        }
//
//        /* Send the message to the LCD gatekeeper for display. */
//        xQueueSend(xLCDQueue, &xMessage, portMAX_DELAY);
//    }
//}

/*-----------------------------------------------------------*/

void vApplicationIdleHook(void) {
    /* Schedule the co-routines from within the idle task hook. */
    //    vCoRoutineSchedule();
}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) {
    (void) pcTaskName;
    (void) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();
    for (;;);
}

//void __DefaultInterrupt(void) {
//    Nop();
//}

void vLEDShark(void *pvParameters) {
    pvParameters = pvParameters;

    for (;;) {
        _LATE7 ^= 1; //green led
        vTaskDelay(500);
    }
}

void Uart3SendString(void*strings) {
    char *pcByte;
    pcByte = (char*) strings;
    /*send strings until get '\0' */
    while (*pcByte != '\0') {
        if (xSerialPutChar(NULL, *pcByte, 0) == pdPASS) {
            _LATE5 ^= 1;
            pcByte++;
        }
    }
    /*final set led dark*/
    _LATE5 = 0;
}

void Uart3SendInterger(UBaseType_t number) {
    char pcBytes[6] = {0, 0, 0, 0, 0, 0};
    //    pcByte = (char*) parameter;
    char cnt = 0;
    unsigned char headchar = 4;
    UBaseType_t numbers = number;

    for (cnt = 4; cnt >= 0; cnt--) {
        pcBytes[(unsigned char) cnt] = numbers % 10;
        numbers /= 10;
        if (pcBytes[(unsigned char) cnt]) {
            headchar = cnt;
        }
        pcBytes[(unsigned char) cnt] += 0x30;
    }
    Uart3SendString(pcBytes + headchar);
}

void vConfigureTimerForTickless(void) {
    /*Init Timer1*/
    //TMR1 0; 
    TMR1 = 0x00;
    //Period = 10 s; Frequency = 31000 Hz; PR1 38750; 
    PR1 = 38750; //0x975e
    //TCKPS 1:8; TON enabled; TSIDL disabled; TCS External; TECS LPRC; TSYNC disabled; TGATE disabled; 
    T1CON = 0x0212;

    _T1IP = 3;
    _T1IE = 0;
    _T1IF = 0;
    //    T3CONbits.TON = 1;
}

uint8_t T1Flag = 0;

void __attribute__((__interrupt__, auto_psv)) _T1Interrupt(void) {
    //    ulTickLessTimerCount++;
    T1Flag = 1;
    _T1IF = 0;
}

void vStartExternalTimer(TickType_t xExpectedIdleTime) {
    //    PR1 = xExpectedIdleTime;
    T1CONbits.TON = 1;
    _T1IE = 1;
    _T1IF = 0;
    //    ulTickLessTimerCount = 0;
}

void vStopExternalTimer(void) {
    T1CONbits.TON = 0;
    _T1IE = 0;
}

void prvSleep(void) {
    Sleep();
}

void prvStopTickInterruptTimer(void) {
    T3CONbits.TON = 0;
    _T3IE = 0;
}

void prvStartTickInterruptTimer(void) {
    T3CONbits.TON = 1;
    _T3IE = 1;
}

unsigned long ulGetExternalTime(void) {
    return 10000; //10s
}

void vSetWakeTimeInterrupt(TickType_t expectedIdleTime) {
    T1Flag = 0;
    vConfigureTimerForTickless();
    vStartExternalTimer(expectedIdleTime);
    //    while (1) {
    //        if (T1Flag) {
    //            T1Flag = 0;
    //            break;
    //        } else {
    //            Uart3SendString("Enter TickLess mode!!!");
    //        }
    //    }
}
UBaseType_t xEntryTickLessMode = pdFALSE;

void vWCGSleep(TickType_t expectedIdleTime) {
//    unsigned long ulLowPowerTimeBeforSleep, ulLowPowerTimeAfterSleep;

    if (xEntryTickLessMode) {
        Uart3SendString("Enter");
//        ulLowPowerTimeBeforSleep = 0;
        prvStopTickInterruptTimer();
        //        vSetWakeTimeInterrupt(expectedIdleTime);
        prvSleep();
        //        ulLowPowerTimeAfterSleep = ulGetExternalTime();
        //        vStopExternalTimer();
        //        vTaskStepTick(ulLowPowerTimeAfterSleep);
        prvStartTickInterruptTimer();
        Uart3SendString("EXIT");
    }
}

void task_start(void *parameter) {
    xTaskCreate(vLEDShark, "vLEDShark", mainCHECK_TAKS_STACK_SIZE, NULL, mainCHECK_TASK_PRIORITY, NULL);
    //    xTaskCreate(vStatusTask, "vStatusTask", mainCHECK_TAKS_STACK_SIZE, NULL, vTaskStatu_PRIORITY, xStateTask);
    vTaskDelete(NULL);
}


