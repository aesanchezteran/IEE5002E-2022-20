/* Include Files */
#include "xparameters.h"
#include "xgpio.h"
#include "xstatus.h"
#include "xil_printf.h"

/*Include SCU Timer Driver*/
#include "xscutimer.h"

/* Definitions */
#define GPIO_DEVICE_ID  XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device that LEDs are connected to */
#define LED 0x9										/* Initial LED value - X00X */
#define LED_CHANNEL 1								/* GPIO port for LEDs */
#define printf xil_printf							/* smaller, optimised printf */

/* Timer Definitions */
#define TIMER_LOAD_VALUE 0xffffffff
#define TIMER_DEVICE_ID	XPAR_XSCUTIMER_0_DEVICE_ID
#define TIMER_PRESCALER	0x03

/* *********************************************************
 *
 * TIMER_PERIOD = (PRESCALER+1)x(M+1)/PERIPHCLK
 *
 * M = TIMER_PERIOD x PERIPHCLK/(PRESCALER+1) -1
 *
 * PERIPHCLK = 667 MHz/2 = 333.5 MHz (Zynq TRM pp.239, 8.2.1 Clocking)
 * PRESCALER = 3  The value should not match the PERIPHCLK frequency
 * TIMER_PERIOD = 1s
 *
 * M = (1)(333.5E6)/(3+1)-1 = 83374999
 *
 * Since it is a count-down timer then instead of counting down
 * from M it is better to count the cycles from 2^32 down to an
 * offset. This offset is calculated as the difference between 2^32
 * and M.
 *
 * ONE_SECOND_TMR_OFFSET = 2^32-M = 4211592297
 *
 * *********************************************************/

#define ONE_SECOND_TMR_OFFSET 4211592297

XGpio Gpio;											/* GPIO Device driver instance */

/* Timer driver instances */
XScuTimer myTimer;									/* Timer Device driver instance */

/* Timer specific pointers and instances */
XScuTimer_Config *ConfigPtr;					//Pointer for configuration
XScuTimer *TimerInstancePtr = &myTimer;			// Pointer to timer device

int LEDOutputExample(void)
{
	volatile u32 CntValue = 0;   /* Timer Counter Value */

	int Status;
	int led = LED; /* Hold current LED value. Initialise to LED definition */



/*****************************************************************************
 *
 *  GPIO Configuration for LEDS
 *
 * ***************************************************************************/
		/* GPIO driver initialisation */
		Status = XGpio_Initialize(&Gpio, GPIO_DEVICE_ID);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		/*Set the direction for the LEDs to output. */
		XGpio_SetDataDirection(&Gpio, LED_CHANNEL, 0x0);

/*****************************************************************************
*
*  TIMER configuration section
*
* ***************************************************************************/

		/* Timer configuration and initialization */
		ConfigPtr = XScuTimer_LookupConfig(TIMER_DEVICE_ID);

		Status = XScuTimer_CfgInitialize(TimerInstancePtr, ConfigPtr, ConfigPtr->BaseAddr);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		/* Load Timer Counter Register */
		XScuTimer_LoadTimer(TimerInstancePtr, TIMER_LOAD_VALUE);

		// Enable autoreload
		XScuTimer_EnableAutoReload(TimerInstancePtr);

		// Set Prescaler
		XScuTimer_SetPrescaler(TimerInstancePtr, TIMER_PRESCALER);

		/* Start the timer */
		XScuTimer_Start(TimerInstancePtr);

/*****************************************************************************
*
*  TIMER Configuration end
*
* ***************************************************************************/

		CntValue = XScuTimer_GetCounterValue(TimerInstancePtr);

		/* Loop forever blinking the LED. */
			while (1) {
				/* Load Timer Counter Register */
				XScuTimer_LoadTimer(TimerInstancePtr, TIMER_LOAD_VALUE);

				/* Update CntValue to enter while loop */
				CntValue = XScuTimer_GetCounterValue(TimerInstancePtr);

				/* Wait a small amount of time so that the LED blinking is visible. POLLING */
				while(CntValue >= ONE_SECOND_TMR_OFFSET) {
					CntValue = XScuTimer_GetCounterValue(TimerInstancePtr);
				}

				/* Flip LEDs. */
				led = ~led;

				/* Write output to the LEDs. */
				XGpio_DiscreteWrite(&Gpio, LED_CHANNEL, led);
			}

		return XST_SUCCESS; /* Should be unreachable */
}

/* Main function. */
int main(void){

	int Status;

	/* Execute the LED output. */
	Status = LEDOutputExample();
	if (Status != XST_SUCCESS) {
		xil_printf("GPIO output to the LEDs failed!\r\n");
	}

	return 0;
}

