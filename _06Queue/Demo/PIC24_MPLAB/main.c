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
//UBaseType_t start_PRIORITY = 5;
UBaseType_t vLEDShark_PRIORITY = 2;
UBaseType_t vTaskKey_PRIORITY = 5;
UBaseType_t vTaskKeyProcess_PRIORITY = 3;
//TaskHandle_t xStateTask;
//uint16_t configMINIMAL_STACK_SIZE = 128;
//uint16_t configMINIMAL_STACK_SIZE 128;
//uint16_t configMINIMAL_STACK_SIZE 128;
void Uart3SendString(void* strings);
void vKeyProcess(void* parameter);
void vKeyInput(void * parameter);
void vLEDShark(void*);

struct A_Message {
    char ucMessageID;
    char ucDatap[20];
};
typedef struct A_Message AMessage;
QueueHandle_t xQueue;
#define QUEUE_LENGTH 5
#define QUEUE_ITEM_SIZE sizeof(AMessage)

/*
 * Create the demo tasks then start the scheduler.
 */
int main(void) {//2¸öLEDµÆÉÁË¸
    /* Configure any hardware required for this demo. */
    prvSetupHardware();
    /*create Queue*/
    xQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
    if (xQueue == NULL) {
        Uart3SendString("Create Queue Fail\r\n");
    }
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
    xTaskCreate(vLEDShark, "vLEDShark", mainCHECK_TAKS_STACK_SIZE, NULL, mainCHECK_TASK_PRIORITY, NULL);
    //    xTaskCreate(vKeyInput, "vKeyInput", mainCHECK_TAKS_STACK_SIZE, NULL, vTaskKey_PRIORITY, NULL);
    xTaskCreate(vKeyProcess, "vKeyProcess", mainCHECK_TAKS_STACK_SIZE, NULL, vTaskKeyProcess_PRIORITY, NULL);
    /* Start the high frequency interrupt test. */
    //    vSetupTimerTest(mainTEST_INTERRUPT_FREQUENCY);

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
            //            _LATE5 ^= 1;
            pcByte++;
        }
    }
    /*final set led dark*/
    //    _LATE5 = 0;
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
//void vKeyProcess(void* parameter) 

void vKeyProcess(void* parameter) {
    AMessage xBuffer;
    BaseType_t xRetuen;
    for (;;) {
        /*no wait */
        xRetuen = xQueueReceive(xQueue, &xBuffer, 1000);
        if (xRetuen == pdTRUE) {
            switch (xBuffer.ucMessageID) {
                case 1:
                {
                    Uart3SendString("S1 Button is pushed!\r\n");
                    break;
                }
                case 2:
                {
                    Uart3SendString("S2 Button is pushed!\r\n");
                    break;
                }
                case 3:
                {
                    Uart3SendString("S3 Button is pushed!\r\n");
                    break;
                }
            }
            Uart3SendString("xBuffer.ucMessageID is ");
            Uart3SendInterger((UBaseType_t) xBuffer.ucMessageID);
            Uart3SendString("\r\n");
            Uart3SendString("xBuffer.ucDatap: ");
            Uart3SendString(xBuffer.ucDatap);
            Uart3SendString("\r\n");
        }
        //vTaskDelay(1000);
        _LATE5 ^= 1;
    }
}
//void task_start(void *parameter) {
//    xTaskCreate(vLEDShark, "vLEDShark", mainCHECK_TAKS_STACK_SIZE, NULL, mainCHECK_TASK_PRIORITY, NULL);
//    xTaskCreate(vKeyInput, "vKeyInput", mainCHECK_TAKS_STACK_SIZE, NULL, vTaskKey_PRIORITY, NULL);
//    xTaskCreate(vKeyProcess, "vKeyProcess", mainCHECK_TAKS_STACK_SIZE, NULL, vTaskStatu_PRIORITY, NULL);
//    vTaskDelete(NULL);
//}


