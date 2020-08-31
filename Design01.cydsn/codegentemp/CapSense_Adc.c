/***************************************************************************//**
* \file CapSense_Adc.c
* \version 5.0
*
* \brief
*   This file provides implementation for the ADC module of the CapSense
*   Component.
*
* \see CapSense v5.0 Datasheet
*
*//*****************************************************************************
* Copyright (2016-2017), Cypress Semiconductor Corporation.
********************************************************************************
* This software is owned by Cypress Semiconductor Corporation (Cypress) and is
* protected by and subject to worldwide patent protection (United States and
* foreign), United States copyright laws and international treaty provisions.
* Cypress hereby grants to licensee a personal, non-exclusive, non-transferable
* license to copy, use, modify, create derivative works of, and compile the
* Cypress Source Code and derivative works for the sole purpose of creating
* custom software in support of licensee product to be used only in conjunction
* with a Cypress integrated circuit as specified in the applicable agreement.
* Any reproduction, modification, translation, compilation, or representation of
* this software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH
* REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* Cypress reserves the right to make changes without further notice to the
* materials described herein. Cypress does not assume any liability arising out
* of the application or use of any product or circuit described herein. Cypress
* does not authorize its products for use as critical components in life-support
* systems where a malfunction or failure may reasonably be expected to result in
* significant injury to the user. The inclusion of Cypress' product in a life-
* support systems application implies that the manufacturer assumes all risk of
* such use and in doing so indemnifies Cypress against all charges. Use may be
* limited by and subject to the applicable Cypress software license agreement.
*******************************************************************************/
#include <cytypes.h>
#include "CyLib.h"
#include "CapSense_ISR.h"
#include "CapSense_Configuration.h"
#include "CapSense_Structure.h"
#include "CapSense_Adc.h"
#include "CapSense_Sensing.h"
#if (CapSense_ENABLE == CapSense_SELF_TEST_EN)
    #include "CapSense_SelfTest.h"
#endif

#if (CapSense_ADC_EN)

static uint8 CapSense_initVar;
uint16 CapSense_adcVrefMv = CapSense_ADC_VREF_MV;


/*******************************************************************************
* Module local function declarations
*******************************************************************************/
/**
* \cond SECTION_ADC_INTERNAL
* \addtogroup group_adc_internal
* \{
*/

void CapSense_AdcSetModClkClockDivider(uint32 modClk);

/** \}
* \endcond */

/*******************************************************************************
* Local definition
*******************************************************************************/
#define CapSense_AdcINIT_DONE                    (1u)
#define CapSense_AdcINIT_NEEDED                  (0u)
#define CapSense_AdcCAL_WATCHDOG_CYCLES_NUM      (0x0000FFFFLu)

#if (CapSense_ENABLE == CapSense_ADC_STANDALONE_EN)
    /*******************************************************************************
    * Function Name: CapSense_Start
    ****************************************************************************//**
    * \cond SECTION_STANDALONE_ADC
    * \brief
    *   Configures the hardware and performs calibration.
    *
    * \details
    *   Configures the hardware and performs calibration.
    * \endcond
    *
    *******************************************************************************/
    void CapSense_Start(void)
    {
        CapSense_ClearAdcChannels();

        if (CapSense_AdcINIT_DONE != CapSense_initVar)
        {
            CapSense_AdcDsInitialize();
        }
        CapSense_ConfigAdcResources();
        (void) CapSense_AdcCalibrate();
        while(CapSense_AdcIsBusy() != CapSense_AdcSTATUS_IDLE)
        {
        }
    }

    /*******************************************************************************
    * Function Name: CapSense_Sleep
    ****************************************************************************//**
    * \cond SECTION_STANDALONE_ADC
    * \brief
    *  Prepares the Component for deep sleep.
    *
    * \details
    *  Currently this function is empty and exists as a place for future updates,
    *  this function will be used to prepare the Component to enter deep sleep.
    * \endcond
    *
    *******************************************************************************/
    void CapSense_Sleep(void)
    {
    }


    /*******************************************************************************
    * Function Name: CapSense_Wakeup
    ****************************************************************************//**
    * \cond SECTION_STANDALONE_ADC
    * \brief
    *  This function resumes the Component after sleep.
    *
    * \details
    *  Currently this function is empty and exists as a place for future updates,
    *  this function will be used to resume the Component after exiting deep sleep.
    * \endcond
    *
    *******************************************************************************/
    void CapSense_Wakeup(void)
    {
    }
#endif  /* (CapSense_ENABLE != CapSense_ADC_STANDALONE_EN) */

/*******************************************************************************
* Function Name: CapSense_SetNonDedicatedAdcChannel
****************************************************************************//**
*
* \brief
*   Sets the non dedicated channel to the given state.
*
* \details
*   Connects/disconnects the pin and the analog muxbus B. Sets the drive mode
*   of the pin as well.
*
* \param chId  The ID of the non dedicated channel to be set.
* \param state The state in which the channel is to be put:
*         - (0) CapSense_AdcCHAN_DISCONNECT
*         - (1) CapSense_AdcCHAN_CONNECT
*
*******************************************************************************/
void CapSense_SetNonDedicatedAdcChannel(uint8 chId, uint32 state)
{
    CapSense_FLASH_IO_STRUCT const *ptr2adcIO;
    uint32 newRegisterValue;
    uint8  interruptState;
    uint32 pinHSIOMShift;
    uint32 pinModeShift;
    uint32 tmpVal;

    ptr2adcIO = &CapSense_adcIoList[chId];
    pinHSIOMShift = (uint32)ptr2adcIO->hsiomShift;
    pinModeShift = (uint32)ptr2adcIO->shift;
    
    /* Clear port connections. */
    tmpVal = CY_GET_REG32(ptr2adcIO->hsiomPtr);
    tmpVal &= ~(CapSense_HSIOM_SEL_MASK << pinHSIOMShift);
    
    interruptState = CyEnterCriticalSection();

    switch (state)
    {
    case CapSense_AdcCHAN_CONNECT:
        /* Connect AMuxBusB to the selected port */
        CY_SET_REG32(ptr2adcIO->hsiomPtr, (tmpVal | (CapSense_HSIOM_SEL_AMUXB << pinHSIOMShift)));
        /* Update port configuration register (drive mode) to HiZ Analog */
        newRegisterValue = CY_GET_REG32(ptr2adcIO->pcPtr);
        newRegisterValue &= ~(CapSense_GPIO_PC_MASK << pinModeShift);
        newRegisterValue |= (CapSense_AdcGPIO_PC_INPUT << pinModeShift);
        CY_SET_REG32(ptr2adcIO->pcPtr, newRegisterValue);
    
        /* Cmod and Ctank are not typical GPIO, require CSD setting. */
        if (0u != ((uint32)(ptr2adcIO->hsiomPtr) & CapSense_AdcSW_CMOD_PORT_MASK))
        {
            if (CapSense_AdcSW_CTANK_PINSHIFT == pinModeShift)
            {
                CY_SET_REG32(CapSense_SW_DSI_SEL_PTR, CapSense_SW_DSI_CTANK);
            }
            else if (CapSense_AdcSW_CMOD_PINSHIFT == pinModeShift)
            {
                CY_SET_REG32(CapSense_SW_DSI_SEL_PTR, CapSense_SW_DSI_CMOD);
            }
            else { /* No action */ }
        }
        break;
        
    /* Disconnection is a safe default state. Fall-through is intentional. */
    case CapSense_AdcCHAN_DISCONNECT:
    default:
        /* Disconnec AMuxBusB from the selected port */
        CY_SET_REG32(ptr2adcIO->hsiomPtr, tmpVal);
    
        /* Update port configuration register (drive mode) to HiZ input/output by clearing PC */
        newRegisterValue = CY_GET_REG32(ptr2adcIO->pcPtr);
        newRegisterValue &= ~(CapSense_GPIO_PC_MASK << pinModeShift);
        CY_SET_REG32(ptr2adcIO->pcPtr, newRegisterValue);
        
        /* Cmod and Ctank are not typical GPIO, require CSD setting. */
        if (0u != ((uint32)(ptr2adcIO->hsiomPtr) & CapSense_AdcSW_CMOD_PORT_MASK))
        {
            if ((CapSense_AdcSW_CTANK_PINSHIFT == pinModeShift) ||
                (CapSense_AdcSW_CMOD_PINSHIFT == pinModeShift ))
            {
                CY_SET_REG32(CapSense_SW_DSI_SEL_PTR, 0u);  
            }
        }
        break;
    }

    CyExitCriticalSection(interruptState);
    
    /* Set logic 0 to port data register */
    tmpVal = CY_GET_REG32(ptr2adcIO->drPtr);
    tmpVal &= (uint32)~(uint32)((uint32)1u << ptr2adcIO->drShift);
    CY_SET_REG32(ptr2adcIO->drPtr, tmpVal);
}


/*******************************************************************************
* Function Name: CapSense_SetAdcChannel
****************************************************************************//**
*
* \brief
*   Sets the given channel to the given state.
*
* \details
*   Connects/disconnects the pin and the analog muxbus B.  Sets the drive mode
*   of the pin as well.
*
* \param chId  The ID of the channel to be set.
* \param state The state in which the channel is to be put:
*         - (0) CapSense_AdcCHAN_DISCONNECT
*         - (1) CapSense_AdcCHAN_CONNECT
*
*******************************************************************************/
void CapSense_SetAdcChannel(uint8 chId, uint32 state)
{
    #if (0u != CapSense_ADC_AMUXB_INPUT_EN)
        #if (CapSense_ADC_MIN_CHANNELS < CapSense_ADC_TOTAL_CHANNELS)
            if(CapSense_ADC_SELECT_AMUXB_CH != chId)
            {
                CapSense_SetNonDedicatedAdcChannel(chId, state);
            }
        #endif /* (0u != CapSense_ADC_TOTAL_CHANNELS) */
    #else
        CapSense_SetNonDedicatedAdcChannel(chId, state);
    #endif /* (0u != CapSense_ADC_AMUXB_INPUT_EN) */
}


/*******************************************************************************
* Function Name: CapSense_ConfigAdcResources
****************************************************************************//**
*
* \brief
*   Configures the CSD block to be used as an ADC.
*
* \details
*   Configures the IDACB, internal switches, REFGEN, HSCOMP, enables CSD
*   block interrupt and set interrupt vector to ADC sensing method.
*
*******************************************************************************/
void CapSense_ConfigAdcResources(void)
{
    uint8  interruptState;
    uint32 newRegValue;

    /* Configure clocks */
    CapSense_AdcSetModClkClockDivider(CapSense_ADC_MODCLK_DIV_DEFAULT);
    CY_SET_REG32(CapSense_SENSE_PERIOD_PTR, (CapSense_AdcSENSE_DIV_DEFAULT - 1uL));

    /* Configure the IDAC */
    CY_SET_REG32(CapSense_CONFIG_PTR, CapSense_AdcCONFIG_DEFAULT);
    CY_SET_REG32(CapSense_IDACB_PTR, CapSense_AdcIDACB_CONFIG | CapSense_dsRam.adcIdac);

    /* Configure AZ Time */
    CY_SET_REG32(CapSense_SEQ_TIME_PTR, (CapSense_AdcSEQ_TIME_DEFAUL - 1uL));

    CY_SET_REG32(CapSense_CSDCMP_PTR, 0uL);
    CY_SET_REG32(CapSense_SW_DSI_SEL_PTR, 0uL);

    CY_SET_REG32(CapSense_SENSE_DUTY_PTR, 0uL);
    CY_SET_REG32(CapSense_SEQ_INIT_CNT_PTR, 1uL);
    CY_SET_REG32(CapSense_SEQ_NORM_CNT_PTR, 2uL);

    /* Configure the block-level routing */
    CY_SET_REG32(CapSense_SW_HS_P_SEL_PTR, CapSense_AdcSW_HSP_DEFAULT);
    CY_SET_REG32(CapSense_SW_HS_N_SEL_PTR, CapSense_AdcSW_HSN_DEFAULT);
    CY_SET_REG32(CapSense_SW_SHIELD_SEL_PTR, CapSense_AdcSW_SHIELD_DEFAULT);
    CY_SET_REG32(CapSense_SW_CMP_P_SEL_PTR, CapSense_AdcSW_CMPP_DEFAULT);
    CY_SET_REG32(CapSense_SW_CMP_N_SEL_PTR, CapSense_AdcSW_CMPN_DEFAULT);
    CY_SET_REG32(CapSense_SW_FW_MOD_SEL_PTR, CapSense_AdcSW_FWMOD_DEFAULT);
    CY_SET_REG32(CapSense_SW_FW_TANK_SEL_PTR, CapSense_AdcSW_FWTANK_DEFAULT);
    CY_SET_REG32(CapSense_SW_REFGEN_SEL_PTR, CapSense_AdcSW_REFGEN_DEFAULT);

    interruptState = CyEnterCriticalSection();
    newRegValue = CY_GET_REG32(CapSense_SW_BYP_SEL_PTR);
    newRegValue |= CapSense_AdcSW_BYP_DEFAULT;
    CY_SET_REG32(CapSense_SW_BYP_SEL_PTR, newRegValue);
    CyExitCriticalSection(interruptState);

    /* Config RefGen */
    #if (CapSense_CYDEV_VDDA_MV < CapSense_AdcLVTHRESH)
        /* The routing of the HS_N and AMUXBUF switches depend on RefGen */
        CY_SET_REG32(CapSense_REFGEN_PTR, CapSense_REFGEN_LV);
        CY_SET_REG32(CapSense_SW_AMUXBUF_SEL_PTR, CapSense_AdcSW_AMUBUF_LV);
        CY_SET_REG32(CapSense_AMBUF_PTR, CapSense_AdcAMBUF_LV);
    #else
        CY_SET_REG32(CapSense_REFGEN_PTR, CapSense_AdcREFGEN_NORM);
        CY_SET_REG32(CapSense_SW_AMUXBUF_SEL_PTR, CapSense_AdcSW_AMUBUF_NORM);
    #endif /* CapSense__CYDEV_VDDA_MV < CapSense_AdcLVTHRESH */
    
    /* Configure HSCOMP */
    CY_SET_REG32(CapSense_HSCMP_PTR, CapSense_AdcHSCMP_AZ_DEFAULT);

    /* Clear all pending interrupts of CSD block */
    CY_SET_REG32(CapSense_INTR_PTR, CapSense_INTR_ALL_MASK);
    
    /* Mask all CSD block interrupts (disable all interrupts) */
    CY_SET_REG32(CapSense_INTR_MASK_PTR, CapSense_INTR_MASK_CLEAR_MASK);

    /* Set the ISR vector */
    CapSense_ISR_StartEx(&CapSense_AdcIntrHandler);

    /* Component is initialized */
    CapSense_initVar = CapSense_AdcINIT_DONE;
}


/*******************************************************************************
* Function Name: CapSense_StartAdcFSM
****************************************************************************//**
*
* \brief
*   Starts the CSD state machine with correct parameters to initialize an ADC
*   conversion.
*
* \details
*   Starts the CSD state machine with correct parameters to initialize an ADC
*   conversion.
*
* \param measureMode The FSM mode:
*        - (0) CapSense_AdcMEASMODE_OFF
*        - (1) CapSense_AdcMEASMODE_VREF
*        - (2) CapSense_AdcMEASMODE_VREFBY2
*        - (3) CapSense_AdcMEASMODE_VIN
*
*******************************************************************************/
void CapSense_StartAdcFSM(uint32 measureMode)
{
    uint32 tmpStartVal;

    /* Set the mode and acquisition time */
    CY_SET_REG32(CapSense_ADC_CTL_PTR, (measureMode | (CapSense_AdcACQUISITION_BASE - 1u)));

    if(CapSense_AdcMEASMODE_VREF == measureMode)
    {
        tmpStartVal =
            CapSense_AdcFSMSETTING_AZSKIP_DEFAULT    |
            CapSense_AdcFSMSETTING_DSIIGNORE   |
            CapSense_AdcFSMSETTING_NOABORT     |
            CapSense_AdcFSMSETTING_SEQMODE     |
            CapSense_AdcFSMSETTING_START;
    }
    else if (CapSense_AdcMEASMODE_OFF == measureMode)
    {
        tmpStartVal = CapSense_AdcFSMSETTING_ABORT;
    }
    /* This setting is used for both MEASMODE_VREFBY2 and MEASMODE_VIN */
    else
    {
        tmpStartVal =
            CapSense_AdcFSMSETTING_AZSKIP_DEFAULT    |
            CapSense_AdcFSMSETTING_DSIIGNORE   |
            CapSense_AdcFSMSETTING_NOABORT     |
            CapSense_AdcFSMSETTING_SEQMODE     |
            CapSense_AdcFSMSETTING_START;
    }

    /* Enable HSComp */
    CY_SET_REG32(CapSense_SEQ_START_PTR, tmpStartVal);
}

/*******************************************************************************
* Function Name: CapSense_AdcCaptureResources
****************************************************************************//**
*
* \brief
*   Releases CSD resources from sensing mode, and sets it into ADC mode.
*
* \details
*   Releases CSD resources from sensing mode, and sets it into ADC mode.
*
* \return     The function returns cystatus of its operation.
*   - CYRET_LOCKED  - The sensing hardware is in-use and could not be released.
*   - CYRET_SUCCESS - Block is configured for ADC use.
*
*******************************************************************************/
cystatus CapSense_AdcCaptureResources(void)
{
    cystatus tmpStatus = CYRET_SUCCESS;

    #if !(CapSense_ENABLE == CapSense_ADC_STANDALONE_EN)
        tmpStatus = CapSense_SsReleaseResources();

        if (CapSense_AdcINIT_NEEDED == CapSense_initVar)
        {
            if(CYRET_SUCCESS == tmpStatus)
            {
                CapSense_ConfigAdcResources();
            }
            else
            {
                tmpStatus = CYRET_LOCKED;
            }
        }
    #else
        if (CapSense_AdcINIT_NEEDED == CapSense_initVar)
        {
            CapSense_ConfigAdcResources();
        }
    #endif /* !(CapSense_ENABLE == CapSense_ADC_STANDALONE_EN) */

    return tmpStatus;
}


/*******************************************************************************
* Function Name: CapSense_AdcReleaseResources
****************************************************************************//**
*
* \brief
*   Releases CSD resources from ADC mode.
*
* \details
*   Releases CSD resources from ADC mode.
*
* \return     The function returns cystatus of its operation.
*   CYRET_SUCCESS   Block resources no longer in use.
*
*******************************************************************************/
cystatus CapSense_AdcReleaseResources(void)
{
    uint8  interruptState;
    uint32 newRegValue;

    if (CapSense_AdcINIT_DONE == CapSense_initVar)
    {
        /* If the FSM is running, shut it down. */
        if(CapSense_AdcSTATUS_IDLE != (CapSense_dsRam.adcStatus
                            & CapSense_AdcSTATUS_FSM_MASK))
        {
            CapSense_StartAdcFSM(CapSense_AdcMEASMODE_OFF);
            CapSense_SetAdcChannel((CapSense_dsRam.adcStatus
                            & (uint8)(CapSense_AdcSTATUS_LASTCHAN_MASK)),
                            CapSense_AdcCHAN_DISCONNECT);
            CapSense_dsRam.adcStatus = CapSense_AdcSTATUS_IDLE;
        }

        /* Disable the subblocks. */
        CY_SET_REG32(CapSense_IDACB_PTR, 0u);
        CY_SET_REG32(CapSense_REFGEN_PTR, 0u);
        CY_SET_REG32(CapSense_AMBUF_PTR, 0u);
        CY_SET_REG32(CapSense_HSCMP_PTR, 0u);

        /* Reset the block-level routing */
        CY_SET_REG32(CapSense_SW_HS_P_SEL_PTR, 0u);
        CY_SET_REG32(CapSense_SW_HS_N_SEL_PTR, 0u);
        CY_SET_REG32(CapSense_SW_SHIELD_SEL_PTR, 0u);
        CY_SET_REG32(CapSense_SW_CMP_P_SEL_PTR, 0u);
        CY_SET_REG32(CapSense_SW_CMP_N_SEL_PTR, 0u);
        CY_SET_REG32(CapSense_SW_FW_MOD_SEL_PTR, 0u);
        CY_SET_REG32(CapSense_SW_FW_TANK_SEL_PTR, 0u);

        interruptState = CyEnterCriticalSection();
        newRegValue = CY_GET_REG32(CapSense_SW_BYP_SEL_PTR);
        newRegValue &= (uint32)(~CapSense_AdcSW_BYP_DEFAULT);
        CY_SET_REG32(CapSense_SW_BYP_SEL_PTR, newRegValue);
        CyExitCriticalSection(interruptState);

         /* Disconnect all ADC channels */
        CapSense_ClearAdcChannels();

        CapSense_initVar = CapSense_AdcINIT_NEEDED;
    }

    return CYRET_SUCCESS;
}


/*******************************************************************************
* Function Name: CapSense_AdcStartConvert
****************************************************************************//**
*
* \brief
*  Initializes the hardware and initiates an analog-to-digital conversion on the
*  selected input channel.
*
* \details
*  Initializes the hardware and initiates an analog-to-digital conversion on the
*  selected input channel. This API only initiates a conversion and does not
*  wait for the conversion to be completed, therefore the
*  CapSense_AdcIsBusy() API must be used to check the
*  status and ensure that the conversion is complete prior to reading the result,
*  starting a new conversion with the same or a different channel, or reconfiguring
*  the hardware for different functionality.
*
* \param chId
*  The ID of the channel to be converted.
*
* \return 
*  The function returns cystatus of its operation.
*    - CYRET_SUCCESS - A conversion has started.
*    - CYRET_LOCKED - The hardware is already in-use by a previously initialized
*      conversion or other functionality. No new conversion is started by this API.
*    - CYRET_BAD_PARAM - An invalid channel Id. No conversion is started.
*
*******************************************************************************/
cystatus CapSense_AdcStartConvert(uint8 chId)
{
    uint32 tmpStatus = CYRET_SUCCESS;

    /* If non-standalone ADC, try to Capture resources */
    #if (CapSense_ENABLE != CapSense_ADC_STANDALONE_EN)
        tmpStatus = CapSense_AdcCaptureResources();

        if (CYRET_SUCCESS == tmpStatus)
        {
    #else /* Otherwise, configure resources if needed. */
        if (CapSense_AdcINIT_NEEDED == CapSense_initVar)
        {
            CapSense_ConfigAdcResources();
        }
    #endif /* (CapSense_ENABLE != CapSense_ADC_STANDALONE_EN) */

    if(chId >= CapSense_ADC_TOTAL_CHANNELS)
    {
        tmpStatus = CYRET_BAD_PARAM;
    }
    
    if (CYRET_SUCCESS == tmpStatus)
    {
        if(CapSense_AdcSTATUS_IDLE != (CapSense_dsRam.adcStatus & CapSense_AdcSTATUS_FSM_MASK))
        {
            tmpStatus = CYRET_LOCKED;
        }

        if(CYRET_SUCCESS == tmpStatus)
        {
            #if(CapSense_ADC_ANALOG_STARTUP_DELAY_US > 0uL)
                CyDelayUs(CapSense_ADC_ANALOG_STARTUP_DELAY_US);
            #endif /* (CapSense_ADC_ANALOG_STARTUP_DELAY_US > 0uL) */

            /* Set Component Status */
            CapSense_dsRam.adcStatus = (CapSense_AdcSTATUS_CONVERTING | chId);

            /* Configure a desired channel if needed */
            if (chId != CapSense_dsRam.adcActiveCh)
            {
                if (CapSense_AdcNO_CHANNEL != CapSense_dsRam.adcActiveCh)
                {
                    /* Disconnect existing channel */
                    CapSense_SetAdcChannel(CapSense_dsRam.adcActiveCh, CapSense_AdcCHAN_DISCONNECT);
                }
                /* Connect desired input */
                CapSense_SetAdcChannel(chId, CapSense_AdcCHAN_CONNECT);
                CapSense_dsRam.adcActiveCh = chId;
            }

            /* Un-mask ADC_RES interrupt (enable interrupt) */
            CY_SET_REG32(CapSense_INTR_MASK_PTR, CapSense_INTR_MASK_ADC_RES_MASK);
            CapSense_StartAdcFSM(CapSense_AdcMEASMODE_VIN);
        }
    }
    
    #if (CapSense_ENABLE != CapSense_ADC_STANDALONE_EN)
        }
    #endif /* (CapSense_ENABLE != CapSense_ADC_STANDALONE_EN) */
    
    return tmpStatus;
}


/*******************************************************************************
* Function Name: CapSense_AdcIsBusy
****************************************************************************//**
*
* \brief
*   The function returns the status of the ADC's operation.
*
* \details
*   The function returns the status of the ADC's operation. A new conversion or
*   calibration must not be started unless the ADC is in the IDLE state.
*
* \return
*  The function returns the status of the ADC's operation.
*    - CapSense_AdcSTATUS_IDLE - The ADC is not busy,
*       a new conversion can be initiated.
*    - CapSense_AdcSTATUS_CONVERTING - A previously
*       initiated conversion is in progress.
*    - CapSense_AdcSTATUS_CALIBPH1 - The ADC is in the
*      first phase (of 3) of calibration.
*    - CapSense_AdcSTATUS_CALIBPH2 - The ADC is in the
*      second phase (of 3) of calibration.
*    - CapSense_AdcSTATUS_CALIBPH3 - The ADC is in the
*      third phase (of 3) of calibration.
*    - CapSense_AdcSTATUS_OVERFLOW - The most recent
*      measurement caused an overflow. The root cause of the overflow may be
*      the previous calibration values being invalid or the VDDA setting in cydwr
*      and hardware do not match. Perform re-calibration or set the
*      appropriate VDDA value in cydwr to avoid this error condition.
*
*******************************************************************************/
uint8 CapSense_AdcIsBusy(void)
{
    uint8 tmpStatus;

    if (0u != (CapSense_ADC_RES_REG & CapSense_AdcADC_RES_OVERFLOW_MASK))
    {
        tmpStatus = CapSense_AdcSTATUS_OVERFLOW;
    }
    else
    {
        tmpStatus = (uint8)((*(volatile uint8 *)&CapSense_dsRam.adcStatus) & CapSense_AdcSTATUS_FSM_MASK);
    }

    return tmpStatus;
}


/*******************************************************************************
* Function Name: CapSense_AdcReadResult_mVolts
****************************************************************************//**
*
* \brief
*  This is a blocking API. It initiates a conversion, waits for completion and
*  returns the result.
*
* \details
*  This is a blocking API. Internally, it starts a conversion using
*  CapSense_AdcStartConvert(), checks the status using
*  CapSense_AdcIsBusy(), waits until the conversion is
*  completed and returns the result.
*
* \param chId
*  The ID of the channel to be measured
*
* \return
*  The function returns voltage in millivolts or
*  CapSense_AdcVALUE_BAD_RESULT if:
*   - chId is invalid
*   - The ADC conversion is not started
*   - The ADC conversion watch-dog triggered.
*
*******************************************************************************/
uint16 CapSense_AdcReadResult_mVolts(uint8 chId)
{
    cystatus convertStatus;
    uint16 tmpValue;
    uint32 watchdogAdcCounter;

    convertStatus = CapSense_AdcStartConvert(chId);
    if (CYRET_SUCCESS == convertStatus)
    {
        /* Initialize Watchdog Counter with time interval which is enough to ADC conversion is completed */
        watchdogAdcCounter = CapSense_AdcCAL_WATCHDOG_CYCLES_NUM;
        while ((CapSense_AdcIsBusy() != CapSense_AdcSTATUS_IDLE) &&
               (0u != watchdogAdcCounter))
        {
            /* Wait until conversion complete and decrement Watchdog Counter to prevent unending loop */
            watchdogAdcCounter--;
        }
        if (0u != watchdogAdcCounter)
        {
            tmpValue = CapSense_AdcGetResult_mVolts(chId);
        }
        else
        {
            tmpValue = (uint16) CapSense_AdcVALUE_BAD_RESULT;
        }
    }
    else
    {
        tmpValue = (uint16) CapSense_AdcVALUE_BAD_RESULT;
    }

    return tmpValue;
}


/*******************************************************************************
* Function Name: CapSense_AdcGetResult_mVolts
****************************************************************************//**
*
* \brief
*  This API does not perform an ADC conversion and returns the last valid result
*  for the specified channel.
*
* \details
*  Returns the last valid result from the data structure for the specified channel.
*  This function can be used to read a previous result of any channel even if the
*  ADC is busy or a conversion is in progress. However, it is highly recommended
*  not to use this function with a channel that is in an active conversion.
*
* \param chId
*  The ID of the channel to be measured
*
* \return
*  The function returns a voltage in millivolts or
*  CapSense_AdcVALUE_BAD_CHAN_ID if chId is invalid.
*
*******************************************************************************/
uint16 CapSense_AdcGetResult_mVolts(uint8 chId)
{
    uint32 tmpRetVal = CapSense_AdcVALUE_BAD_CHAN_ID;

    if(chId < CapSense_ADC_TOTAL_CHANNELS)
    {
        tmpRetVal = CapSense_dsRam.adcResult[chId];
    }
    return (uint16)tmpRetVal;
}


/*******************************************************************************
* Function Name: CapSense_AdcCalibrate
****************************************************************************//**
*
* \brief
*  Performs calibration of the ADC module.
*
* \details
*  Performs calibration for the ADC to identify the appropriate hardware configuration
*  to produce accurate results. It is recommended to run the calibration
*  periodically (for example every 10 seconds) for accuracy and compensations.
*
* \return
*  The function returns cystatus of its operation.
*    - CYRET_SUCCESS - The block is configured for the ADC use.
*    - CYRET_LOCKED - The hardware is already in-use by a previously initialized
*      conversion or other functionality. No new conversion is started by this API.
*
*******************************************************************************/
cystatus CapSense_AdcCalibrate(void)
{
    uint32 tmpStatus;
    uint32 watchdogAdcCounter;
    uint32 tmpVrefCal;

    tmpStatus = CapSense_AdcCaptureResources();

    if(CapSense_AdcSTATUS_IDLE != (CapSense_dsRam.adcStatus
        & CapSense_AdcSTATUS_FSM_MASK))
    {
        tmpStatus = CYRET_LOCKED;
    }

    if (CYRET_SUCCESS == tmpStatus)
    {
        #if(CapSense_ADC_ANALOG_STARTUP_DELAY_US > 0uL)
            CyDelayUs(CapSense_ADC_ANALOG_STARTUP_DELAY_US);
        #endif /* (CapSense_ADC_ANALOG_STARTUP_DELAY_US > 0uL) */

        /* Disconnect a channel if connected */
        if (CapSense_AdcNO_CHANNEL != CapSense_dsRam.adcActiveCh)
        {
            CapSense_SetAdcChannel(CapSense_dsRam.adcActiveCh, CapSense_AdcCHAN_DISCONNECT);
            CapSense_dsRam.adcActiveCh = CapSense_AdcNO_CHANNEL;
        }
        
        /* Get Vref trim-value */
        tmpVrefCal = (uint32)CY_GET_REG8(CYREG_SFLASH_CSDV2_CSD0_ADC_TRIM1) | 
            (((uint32)CY_GET_REG8(CYREG_SFLASH_CSDV2_CSD0_ADC_TRIM2)) << 8u);

        /* Update nominal Vref to real Vref */
        tmpVrefCal *= CapSense_ADC_VREF_MV;
        tmpVrefCal /= CapSense_AdcVREFCALIB_BASE;

        CapSense_adcVrefMv = (uint16)tmpVrefCal;

        CapSense_dsRam.adcIdac = (uint8)CapSense_ADC_IDAC_DEFAULT;
        CY_SET_REG32(CapSense_IDACB_PTR, CapSense_AdcIDACB_CONFIG |
                                                     CapSense_dsRam.adcIdac);

        CapSense_dsRam.adcStatus = (CapSense_AdcSTATUS_CALIBPH1);

        /* Un-mask ADC_RES interrupt (enable interrupt) */
        CY_SET_REG32(CapSense_INTR_MASK_PTR, CapSense_INTR_MASK_ADC_RES_MASK);
        CapSense_StartAdcFSM(CapSense_AdcMEASMODE_VREF);

        /* Global CRC update */
        #if (CapSense_ENABLE == CapSense_SELF_TEST_EN)
            #if (CapSense_ENABLE ==CapSense_TST_GLOBAL_CRC_EN)
                CapSense_DsUpdateGlobalCrc();
            #endif /* (CapSense_ENABLE == CapSense_TST_GLOBAL_CRC_EN) */
        #endif /* (CapSense_ENABLE == CapSense_SELF_TEST_EN) */

        /* Initialize Watchdog Counter with time interval which is enough to ADC calibration is completed */
        watchdogAdcCounter = CapSense_AdcCAL_WATCHDOG_CYCLES_NUM;
        while (((*(volatile uint8 *)&CapSense_dsRam.adcStatus & CapSense_AdcSTATUS_FSM_MASK)
                != 0u) &&  (0u != watchdogAdcCounter))
        {
            /* Wait until scan complete and decrement Watchdog Counter to prevent unending loop */
            watchdogAdcCounter--;
        }
    }

    return tmpStatus;
}


/*******************************************************************************
* Function Name: CapSense_AdcInitialize
****************************************************************************//**
*
* \brief
*   Configures the hardware to ADC mode and begins a calibration.
*
* \details
*   Configures the hardware to ADC mode and begins a calibration.
*
*******************************************************************************/
void CapSense_AdcInitialize(void)
{
    CapSense_ConfigAdcResources();
    (void)CapSense_AdcCalibrate();
}


/*******************************************************************************
* Function Name: CapSense_AdcStop
****************************************************************************//**
*
* \brief
*   Disables the hardware sub-blocks that are in use while in the ADC mode,
*   and frees the routing.
*
* \details
*   This function stops the Component operation. No ADC conversion can be
*   initiated when the Component is stopped. Once stopped, the hardware block
*   may be reconfigured by the application program for any other special usage.
*   The ADC operation can be resumed by calling the
*   CapSense_AdcResume() function or the Component can
*   be reset by calling the CapSense_Start() function.
*   This function is called when no ADC conversion is in progress.
*
*******************************************************************************/
void CapSense_AdcStop(void)
{
    #if (CapSense_ENABLE != CapSense_ADC_STANDALONE_EN)
        (void)CapSense_AdcReleaseResources();
    #endif /* (CapSense_ENABLE != CapSense_ADC_STANDALONE_EN) */
    CapSense_initVar = CapSense_AdcINIT_NEEDED;
}


/*******************************************************************************
* Function Name: CapSense_AdcResume
****************************************************************************//**
*
* \brief
*   Resumes the ADC operation after a stop call.
*
* \details
*   Resumes the ADC operation if the operation is stopped
*   previously by the CapSense_AdcStop() API.
*
*******************************************************************************/
void CapSense_AdcResume(void)
{
    CapSense_AdcInitialize();
}


/*******************************************************************************
* Function Name: CapSense_ClearAdcChannels
****************************************************************************//**
*
* \brief
*  Resets all the ADC channels to disconnected state.
*
* \details
*   The function goes through all the ADC channels and disconnects the pin
*   and the analog muxbus B.  Sets the drive mode of the pin as well.
*
*******************************************************************************/
void CapSense_ClearAdcChannels(void)
{
    uint32 chId;

    for (chId = 0u; chId < CapSense_ADC_TOTAL_CHANNELS; chId++)
    {
        CapSense_SetAdcChannel((uint8)chId, CapSense_AdcCHAN_DISCONNECT);
    }
    CapSense_dsRam.adcActiveCh = CapSense_AdcNO_CHANNEL;
}

/*******************************************************************************
* Function Name: CapSense_AdcSetModClkClockDivider
****************************************************************************//**
*
* \brief
*   Sets the divider values for the modulator clock and then starts
*   the modulator clock.
*
* \details
*   It is not recommended to call this function directly by the application layer.
*   It is used by initialization to enable the clocks.
*
* \param
*   modClk The divider value for the modulator clock.
*
*******************************************************************************/
void CapSense_AdcSetModClkClockDivider(uint32 modClk)
{
    /* Stop modulator clock   */
    CY_SET_REG32(CapSense_MODCLK_CMD_PTR,
                 ((uint32)CapSense_ModClk__DIV_ID <<
                 CapSense_MODCLK_CMD_DIV_SHIFT)|
                 ((uint32)CapSense_MODCLK_CMD_DISABLE_MASK));

    /*
     * Set divider value for modClk.
     * 1u is subtracted from modClk because Divider register value 0 corresponds
     * to dividing by 1.
     */
    CY_SET_REG32(CapSense_MODCLK_DIV_PTR, ((modClk - 1u) << 8u));

    /* Check whether previous modulator clock start command has finished. */
    while(0u != (CY_GET_REG32(CapSense_MODCLK_CMD_PTR) & CapSense_MODCLK_CMD_ENABLE_MASK))
    {
        /* Wait until previous modulator clock start command has finished. */
    }

    /* Start modulator clock, aligned to HFCLK */
    CY_SET_REG32(CapSense_MODCLK_CMD_PTR,
                 (uint32)(((uint32)CapSense_ModClk__DIV_ID << CapSense_MODCLK_CMD_DIV_SHIFT) |
                  ((uint32)CapSense_ModClk__PA_DIV_ID << CapSense_MODCLK_CMD_PA_DIV_SHIFT) |
                  CapSense_MODCLK_CMD_ENABLE_MASK));
}

#endif /* #if CapSense_ADC_EN */


/* [] END OF FILE */
