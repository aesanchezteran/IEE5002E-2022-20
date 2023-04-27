/****************************************************************************/
/**
*
* @file XadcZ7-20Interrupt.c
*
* Author: Alberto Sánchez
*
* Date: 8 Dec 2021
*
* Description
*
* This file contains a design example using the driver functions
* of the XADC driver. The example reads CH14 using a interrupt. The XAdc hard macro is instantiated in Vivado
* using the XAdc wizard with the AXI interface.
*
* The XAdc driver "xsysmon.h" allows to interface with the XAdc when the IP is instantiated using the AXI bus using
* the XAdc wizard. "xadcps.h" is used when interfacing directly through the serial interface (DevC)
*
*
*****************************************************************************/

/***************************** Include Files ********************************/

#include "xparameters.h"
#include "xsysmon.h"  //Use xsysmon.h instead of xadcps.h when accessing the adc from an axi interface
#include "xstatus.h"
#include "stdio.h"
#include "xil_printf.h"
#include "xscugic.h" // API for interruptions GIC
#include "xil_exception.h" // API for exceptions

/************************** Constant Definitions ****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define XADC_DEVICE_ID 		XPAR_SYSMON_0_DEVICE_ID
#define XADCPS_CH_AUX_14	XSM_CH_AUX_MIN+14
#define XADC_INT_ID			XPAR_FABRIC_XADC_WIZ_0_IP2INTC_IRPT_INTR
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define XAdcEOCMask 		XSM_IPIXR_EOC_MASK   //0x00000020



/**************************** Instances ******************************/

XSysMon XAdcInst; //Adc instance
XScuGic IntCInst; //Gic instance

/***************** Macros (Inline Functions) Definitions ********************/

#define printf xil_printf /* Small foot-print printf function */


/************************ Global Variables **********************************/
static volatile  int XAdcHit = FALSE;

/************************* Function Prototypes *****************************/

static void XAdcInterruptHandler(void *CallBackRef);
static int XAdcSetupIntrSystem(XScuGic *IntcInstancePtr,XSysMon *XAdcInstancePtr, u16 XAdcIntrId);




int main(void)
{
	XSysMon_Config *XAdcConfigPtr;
	XSysMon *XAdcInstPtr = &XAdcInst;
	int Status;
	u16 Ch14RawData = 0;
	u32 IntrStatusValue;

	// Initialize the XAdc driver.
	XAdcConfigPtr = XSysMon_LookupConfig(XADC_DEVICE_ID); //Pointer to address 0x43C00000U (look in vivado memory map)
	if (XAdcConfigPtr == NULL) {
		return XST_FAILURE;
	}
	XSysMon_CfgInitialize(XAdcInstPtr, XAdcConfigPtr,
				XAdcConfigPtr->BaseAddress);

	Status = XAdcSetupIntrSystem(&IntCInst,XAdcInstPtr,XADC_INT_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XSysMon_SetSequencerMode(XAdcInstPtr, XSM_SEQ_MODE_SAFE); //Safe mode for configuration
	XSysMon_SetSequencerMode(XAdcInstPtr, XSM_SEQ_MODE_SINGCHAN);
	XSysMon_SetSingleChParams(XAdcInstPtr,XADCPS_CH_AUX_14,1,0,0); //Ch14, 10 ADCLK cycles for acquistion time, continous mode, unipolar
	XSysMon_SetAlarmEnables(XAdcInstPtr, 0x0); //Disable all alarms
	XSysMon_SetCalibEnables(XAdcInstPtr,XSM_CFR1_CAL_VALID_MASK);

	IntrStatusValue = Xil_In16(XAdcConfigPtr->BaseAddress+XSM_IPIER_OFFSET); //Read and print interrupt enable register
	printf("Interrupt Enable register %x \n",(unsigned int)IntrStatusValue);
	IntrStatusValue = Xil_In16(XAdcConfigPtr->BaseAddress+XSM_IPISR_OFFSET); //Read and print interrupt status register
	printf("Interrupt Status register %x \n",(unsigned int)IntrStatusValue);
	IntrStatusValue = Xil_In16(XAdcConfigPtr->BaseAddress+XSM_GIER_OFFSET); //Read and print general interrupt enable register
	printf("Global Int Enable %x \n",(unsigned int)IntrStatusValue);

	XSysMon_IntrEnable(XAdcInstPtr, XAdcEOCMask); // Enable EOC interrupt
	XSysMon_IntrGlobalEnable(XAdcInstPtr); // Enable All interruptions


	while(1){
		Status = XSysMon_IntrGetEnabled(XAdcInstPtr); //Read enabled interruptions
		Status = XSysMon_IntrGetStatus(XAdcInstPtr); //Read interruption status

		if (XAdcHit == TRUE) {
			XAdcHit = FALSE;
			Ch14RawData = (XSysMon_GetAdcData(XAdcInstPtr, 14));
			printf("Ch14RawData %x \n",Ch14RawData);
		}
	}

}



static int XAdcSetupIntrSystem(XScuGic *IntcInstancePtr,XSysMon *XAdcInstancePtr, u16 XAdcIntrId){
	int Status;

	XScuGic_Config *IntcConfig;

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XScuGic_Connect(IntcInstancePtr,  XAdcIntrId,
				(Xil_ExceptionHandler)XAdcInterruptHandler,
				(void *)XAdcInstancePtr);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	/*
	 * Enable the interrupt for the device.
	 */
	XScuGic_Enable(IntcInstancePtr, XAdcIntrId);

	Xil_ExceptionInit();

	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
				(Xil_ExceptionHandler)XScuGic_InterruptHandler,
				IntcInstancePtr);

	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */

	/*
	 * Enable interrupts in the Processor.
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

static void XAdcInterruptHandler(void *CallBackRef){

XSysMon *XAdcPtr = (XSysMon *)CallBackRef;
u32 IntrStatusValue;

/*
 * Get the interrupt status from the device and check the value.
 */

IntrStatusValue = XSysMon_IntrGetStatus(XAdcPtr);

if (IntrStatusValue & XAdcEOCMask) {

	XAdcHit = TRUE;

}

/*
 * Clear bits in Interrupt Status Register.
 */
XSysMon_IntrClear(XAdcPtr, IntrStatusValue);
}
