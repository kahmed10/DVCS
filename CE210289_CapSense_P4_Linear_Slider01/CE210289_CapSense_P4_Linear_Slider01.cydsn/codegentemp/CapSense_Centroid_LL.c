/***************************************************************************//**
* \file CapSense_Centroid_LL.c
* \version 3.10
*
* \brief
* This file provides the source code for the centroid calculation methods
* of the Component.
*
* \see CapSense P4 v3.10 Datasheet
* 
*//*****************************************************************************
* Copyright (2016), Cypress Semiconductor Corporation.
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

#include "cytypes.h"
#include "CyLib.h"
#include "CapSense_Centroid_LL.h"
#include "CapSense_Configuration.h"
#include "CapSense_Structure.h"
#include "CapSense_Filter.h"

/*******************************************************************************
* Local definition
*******************************************************************************/
#define CapSense_CENTROID_LEGACY_MODE                (0x01u)
#define CapSense_CENTROID_MULTI_TOUCH_MODE           (0x02u)

#if (CapSense_NUM_CENTROIDS == 0x01u)
    #define CapSense_CENTROID_MODE  CapSense_CENTROID_LEGACY_MODE
#else   
    #define CapSense_CENTROID_MODE  CapSense_CENTROID_MULTI_TOUCH_MODE
#endif /* #if CapSense_NUM_CENTROIDS == 0x01u */

#if CapSense_CSX_TOUCHPAD_WIDGET_EN
    #define CapSense_CSX_TOUCHPAD_BUFFER_SIZE \
            (((CapSense_CSX_MAX_LOCAL_PEAKS + 3u) * sizeof(int8)) +                                 /* fingerPosIndex */      \
            (CapSense_CSX_MAX_LOCAL_PEAKS * CapSense_CSX_MAX_LOCAL_PEAKS * sizeof(int32)) + /* distanceMap */         \
            (CapSense_CSX_MAX_LOCAL_PEAKS * sizeof(int32)) +                                        /* int32 mins       [] */ \
            (CapSense_CSX_MAX_LOCAL_PEAKS * sizeof(int8)) +                                         /* int8  links      [] */ \
            (CapSense_CSX_MAX_LOCAL_PEAKS * sizeof(int8)) +                                         /* int8  visited    [] */ \
            (CapSense_CSX_MAX_LOCAL_PEAKS * sizeof(int8)))                                          /* int8  markIndices[] */

    #define CapSense_CSX_TOUCHPAD_FT_ON_FAIL                (0x81u)
    #define CapSense_CSX_TOUCHPAD_TOUCH_ABSENT_ID           (0x82u)
    #define CapSense_CSX_TOUCHPAD_DEBOUNCE_ENABLE           (0u)
    #define CapSense_CSX_TOUCHPAD_TOUCH_MIN_ID              (0u)
    #define CapSense_CSX_TOUCHPAD_TOUCH_MAX_ID              (CapSense_CSX_MAX_LOCAL_PEAKS - 1u)
    #define CapSense_CSX_TOUCHPAD_INT32                     (0x7FFFFFFFu)
    #define CapSense_CSX_TOUCHPAD_TOUCH_DOWN                (0x80u)
    #define CapSense_CSX_TOUCHPAD_LIFT_OFF                  (0x40u)
    #define CapSense_CSX_TOUCHPAD_DEBOUNCE_MASK             (0x3Fu)
    #define CapSense_CSX_TOUCHPAD_POSITION_FILTER_EN        \
                (CapSense_POS_IIR_FILTER_EN                     || \
                 CapSense_CSX_TOUCHPAD_POS_MEDIAN_FILTER_EN     || \
                 CapSense_POS_AVERAGE_FILTER_EN                 || \
                 CapSense_POS_JITTER_FILTER_EN)
    #define CapSense_CSX_TOUCHPAD_POSITION_NONE             (0xFFFFu)
    #define CapSense_CSX_TOUCHPAD_Z_SHIFT                   (0x04u)
    #define CapSense_CSX_TOUCHPAD_PADDING                   (3Lu)
    #define CapSense_CSX_TOUCHPAD_CENTROID_LENGTH           (3u)
    #define CapSense_CSX_TOUCHPAD_CENTROID_PREVIOUS         (0u)
    #define CapSense_CSX_TOUCHPAD_CENTROID_CENTER           (1u)
    #define CapSense_CSX_TOUCHPAD_CENTROID_NEXT             (2u)

#endif /* CapSense_CSX_TOUCHPAD_WIDGET_EN */

/*******************************************************************************
* Static variables definition
*******************************************************************************/
static uint8 localMax[CapSense_NUM_CENTROIDS];
static CapSense_THRESHOLD_TYPE localDiffArray[CapSense_NUM_CENTROIDS][3u];
static uint8 localMaxIndex;

#if ( 0u !=CapSense_CSX_TOUCHPAD_WIDGET_EN)    
    static CapSense_CSX_TOUCHPAD_DATA_STRUCT newTouches;
    static CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrNewTouches = &newTouches;
    static uint8 newActiveIDs = 0u;
    static uint8 oldActiveIDs = 0u;

    typedef int32 CapSense_CSX_TOUCHPAD_DISTANCE_MAP[CapSense_CSX_MAX_LOCAL_PEAKS];
#endif /* ( 0u !=CapSense_CSX_TOUCHPAD_WIDGET_EN) */

/*******************************************************************************
* API Prototypes
*******************************************************************************/
#if (0u !=CapSense_CSX_TOUCHPAD_WIDGET_EN)
    CY_INLINE static void CapSense_TransferTouch(uint32 newIndex, uint32 oldIndex, CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches);
    static void CapSense_NewTouch(uint32 newIndex, uint8 onDebounce, uint16 fingerOnThreshold);
    CY_INLINE static uint8 CapSense_GetLowestId(uint16 idMask);
    CY_INLINE static uint32 CapSense_CalcDistance(uint32 newIndex, uint32 oldIndex, CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches);
    static void CapSense_Hungarian(uint8 rowCount, uint8 colCount, int8 *fingerPosIndex,
                                           CapSense_CSX_TOUCHPAD_DISTANCE_MAP *distanceMap);
    static void CapSense_FillDistanceMap(CapSense_CSX_TOUCHPAD_DISTANCE_MAP *distanceMap, 
                                                 CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches);
    CY_INLINE static void CapSense_TouchDownDebounce(void);
    CY_INLINE static void CapSense_SortByAge(void);
    CY_INLINE static void CapSense_CopyTouchRecord(CapSense_CSX_TOUCHPAD_PEAK_STRUCT *destination, 
                                                           CapSense_CSX_TOUCHPAD_PEAK_STRUCT *source);    
    #if (0u != CapSense_OFF_DEBOUNCE_EN)
    CY_INLINE static void CapSense_LiftOffDebounce(uint8 offDebounce, 
                                                           CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches);
    #endif /* (0u != CapSense_OFF_DEBOUNCE_EN) */     
#endif /* (0u !=CapSense_CSX_TOUCHPAD_WIDGET_EN) */


/*******************************************************************************
* Function Name: CapSense_DpFindLocalMaxSd
****************************************************************************//**
*
* \brief
*   Finds the maximum in the widget differences and prepares array with maximum difference 
*   and it's neighbour differences in Legacy mode (only one touch can be found).
*   Finds the maxima in the widget differences and prepares arrays with maxima difference 
*   and theirs neighbour differences in Multi-Touch mode (maximum three touches can be found).
*   This API is used in conjunction with CapSense_DpCalcLinearCentroid or 
*   CapSense__DpCalcRadialCentroid, because they use shared static data: localMax[], 
*   localDiffArray[][3u], localMaxIndex.
*
* \details
*   The localMax[] is used to store the index of the maximum/maxima.
*   The localDiffArray[][] is used to store values of the maximum/maxima and neighbour differences.
*   The localMaxIndex is used to store the number of maxima have been actually found.
*
* \param ptrSns The pointer to the first sensor.
* \param snsCount The total sensor number in the widget.
* \param fingerThreshold The finger threshold in the widget.
*
* \return Returns the number of maxima found.
*
*******************************************************************************/    
#if (CapSense_NUM_CENTROIDS == 0x01u)

uint32 CapSense_DpFindLocalMaxSd(
                        CapSense_RAM_SNS_STRUCT ptrSns[],
                        uint32 snsCount, 
                        uint32 fingerThreshold)
{
    uint32 sensorId;        
    CapSense_RAM_SNS_STRUCT *localPtrSns = ptrSns;   
    CapSense_THRESHOLD_TYPE temp = 0u;
    localMaxIndex = 0u;
    
    for (sensorId = 0u; sensorId < snsCount; sensorId++)
    {                      
        if ((localPtrSns->diff > fingerThreshold) && (localPtrSns->diff > temp))
        {
            localMax[0u] = LO8(sensorId);
            temp = localPtrSns->diff;
            localMaxIndex = 1u;
        }
        localPtrSns++;
    }

    /* Prepare single-dimension centroid calculation array */
    if (localMaxIndex != 0u)
    {
        localDiffArray[0u][CapSense_CENTROID_POS] = ptrSns[localMax[0u]].diff;

        if (localMax[0u] == 0u)
        {
            localDiffArray[0u][CapSense_CENTROID_POS_PREV] = ptrSns[snsCount - 1u].diff;
            localDiffArray[0u][CapSense_CENTROID_POS_NEXT] = ptrSns[localMax[0u] + 1u].diff;
        }    
        else if (localMax[0u] == (snsCount - 1u))
        {
            localDiffArray[0u][CapSense_CENTROID_POS_PREV] = ptrSns[localMax[0u] - 1u].diff;
            localDiffArray[0u][CapSense_CENTROID_POS_NEXT] = ptrSns[0u].diff;
        }
        else 
        {
            localDiffArray[0u][CapSense_CENTROID_POS_PREV] = ptrSns[localMax[0u] - 1u].diff;
            localDiffArray[0u][CapSense_CENTROID_POS_NEXT] = ptrSns[localMax[0u] + 1u].diff;
        }
    }    
    /* Return status to check if at least one maximum has been found */
    return (uint32)(localMaxIndex);
}

#else /* (CapSense_NUM_CENTROIDS == 0x01u) */
    
uint32 CapSense_DpFindLocalMaxSd(
                        CapSense_RAM_SNS_STRUCT ptrSns[],
                        uint32 snsCount, 
                        uint32 fingerThreshold)
{
    uint32 sensorId;
    uint32 i;
    uint32 proceed = 0u;
    localMaxIndex = 0u;

    /* Check if local max is located on the first sensor */
    if ((ptrSns[0u].diff > fingerThreshold) && (ptrSns[0u].diff >= ptrSns[1u].diff))
    {
        localMax[localMaxIndex] = 0u;
        localMaxIndex++;
    }

    /* Check if local max is located on the last sensor */
    if ((ptrSns[snsCount - 1u].diff > fingerThreshold) && (ptrSns[snsCount - 1u].diff > ptrSns[snsCount - 2u].diff))
    {
        localMax[localMaxIndex] = (uint8)snsCount - 1u;
        localMaxIndex++;
        
        #if (CapSense_NUM_CENTROIDS == 0x02u)
            if (localMaxIndex >= CapSense_NUM_CENTROIDS)
            {
                proceed = 1u;
            }
        #endif /* (CapSense_NUM_CENTROIDS == 0x02u) */
    }

    /* Check all rest sensors for local max */
    for (sensorId = 1u; sensorId < (snsCount - 1u); sensorId++)
    {                      
        if ((proceed == 0u) && (ptrSns[sensorId].diff > fingerThreshold) && (ptrSns[sensorId].diff >= ptrSns[sensorId + 1u].diff))
        {
            if (ptrSns[sensorId].diff > ptrSns[sensorId - 1u].diff)
            {   
                localMax[localMaxIndex] = (uint8)sensorId;
                localMaxIndex++;

                if (localMaxIndex >= CapSense_NUM_CENTROIDS)
                {
                    proceed = 1u;
                }
            }
        }
    }   
    
    /* Save found local max signal for the further processing */
    for (i = 0u; i < localMaxIndex; i++)
    {
        localDiffArray[i][CapSense_CENTROID_POS] = ptrSns[localMax[i]].diff;

        if (localMax[i] == 0u)
        {
            localDiffArray[i][CapSense_CENTROID_POS_PREV] = ptrSns[snsCount - 1u].diff;
            localDiffArray[i][CapSense_CENTROID_POS_NEXT] = ptrSns[localMax[i] + 1u].diff;
        }
        else if (localMax[i] == (snsCount - 1u))
        {
            localDiffArray[i][CapSense_CENTROID_POS_PREV] = ptrSns[localMax[i] - 1u].diff;
            localDiffArray[i][CapSense_CENTROID_POS_NEXT] = ptrSns[0u].diff;
        }
        else
        {
            localDiffArray[i][CapSense_CENTROID_POS_PREV] = ptrSns[localMax[i] - 1u].diff;
            localDiffArray[i][CapSense_CENTROID_POS_NEXT] = ptrSns[localMax[i] + 1u].diff;
        }
    }
   
    return (uint32)(localMaxIndex);
}
#endif /* #if (CapSense_NUM_CENTROIDS == 0x01u) */


#if CapSense_TOTAL_DIPLEXED_SLIDERS
/*******************************************************************************
* Function Name: CapSense_DpFindLocalMaxDiplex
****************************************************************************//**
*
* \brief
*   Finds the maximum in the widget differences. Used only for diplexed linear slider. 
*
* \details
*   The localMax[] is used to store the index of the maximum/maxima.
*   The localDiffArray[][] is used to store values of the maximum/maxima and neighbour differences.
*   The localMaxIndex is used to store the number of maxima have been actually found.
*
* \param ptrSns The pointer to the first sensor.
* \param snsCount The total sensor number in the widget.
* \param diplexTable The pointer to the diplex table.
* \param fingerThreshold The finger threshold in the widget.
*
* \return Returns the number of maxima found.
*
*******************************************************************************/
uint32 CapSense_DpFindLocalMaxDiplex(
                        CapSense_RAM_SNS_STRUCT ptrSns[],
                        uint32 snsCount,
                        uint8 const diplexTable[],
                        uint32 fingerThreshold)
{
    uint32 sensorId;
    uint32 diplexSensorId;

    uint32 biggestStartPos = 0u;
    uint32 biggestSize = 0u;
    uint32 curStartPos = 0u;
    uint32 curSize = 0u;
    CapSense_THRESHOLD_TYPE temp = 0u;
    
    localMaxIndex = 0u;
    
    for (sensorId = 0u; sensorId < (snsCount << 1u); sensorId++)
    {                      
        diplexSensorId = diplexTable[sensorId];
        
        if (ptrSns[diplexSensorId].diff > fingerThreshold) 
        {
            if (0u == curSize)
            {
                curStartPos = sensorId;
            }

            curSize++;
        }
        else
        {
            if (curSize > biggestSize)
            {
                biggestSize = curSize;
                biggestStartPos = curStartPos;
            }
            curSize = 0u;
        }
    }
    
    if (curSize > biggestSize)
    {
        biggestSize = curSize;
        biggestStartPos = curStartPos;
    }

    if (biggestSize >= CapSense_CENTROID_DIPLEX_SECTION_LENGTH)
    {
        for (sensorId = biggestStartPos; sensorId < (biggestStartPos + biggestSize); sensorId++)
        {
            diplexSensorId = diplexTable[sensorId];

            if (ptrSns[diplexSensorId].diff > temp)
            {
                localMax[0u] = LO8(sensorId);
                temp = ptrSns[diplexSensorId].diff;
                localMaxIndex = 1u;
            }
        }

        /* Prepare single-dimension centroid calculation array */
        if (localMax[0u] == 0u)
        {
            localDiffArray[0u][CapSense_CENTROID_POS_PREV] = 0u;
            localDiffArray[0u][CapSense_CENTROID_POS] = ptrSns[localMax[0u]].diff;
            localDiffArray[0u][CapSense_CENTROID_POS_NEXT] = ptrSns[localMax[0u] + 1u].diff;
            
        }    
        else if (localMax[0u] == ((snsCount << 1u) - 1u))
        {
            diplexSensorId = diplexTable[localMax[0u] - 1u];
            localDiffArray[0u][CapSense_CENTROID_POS_PREV] = ptrSns[diplexSensorId].diff;
            diplexSensorId = diplexTable[localMax[0u]];
            localDiffArray[0u][CapSense_CENTROID_POS] = ptrSns[diplexSensorId].diff;
            localDiffArray[0u][CapSense_CENTROID_POS_NEXT] = 0u;
        }
        else
        {
            diplexSensorId = diplexTable[localMax[0u] - 1u];
            localDiffArray[0u][CapSense_CENTROID_POS_PREV] = ptrSns[diplexSensorId].diff;
            diplexSensorId = diplexTable[localMax[0u]];
            localDiffArray[0u][CapSense_CENTROID_POS] = ptrSns[diplexSensorId].diff;
            diplexSensorId = diplexTable[localMax[0u] + 1u];
            localDiffArray[0u][CapSense_CENTROID_POS_NEXT] = ptrSns[diplexSensorId].diff;
        }
    } 

    return (uint32)(localMaxIndex);
}
#endif /* #if CapSense_TOTAL_DIPLEXED_SLIDERS */


/*******************************************************************************
* Function Name: CapSense_DpCalcLinearCentroid
****************************************************************************//**
*
* \brief
*   Calculates the position of the linear slider. 
*
* \details
*   The localMax[] is used to store the index of the maximum/maxima.
*   The localDiffArray[][] is used to store values of the maximum/maxima and neighbour differences.
*   The localMaxIndex is used to store the number of maxima have been actually found.
*
* \param  *position The pointer to the position array for temporal position storage.
* \param multiplier The multiplier used for position calculation. 
*                       Contains the widget resolution and total sensor information. It is generated by customizer.
* \param snsCount The number of sensors in the linear slider. The doubled snsCount has to be passed
*                       if this API is called for diplexed slider.                       
*
* \return Returns the number of positions calculated.
*
*******************************************************************************/
uint32 CapSense_DpCalcLinearCentroid(uint16 position[], uint32 multiplier, uint32 snsCount)
{    
    int32 numerator;
    int32 denominator;
    uint32 i;

    for (i = 0u; i < localMaxIndex; i++)
    {
        /* 
         * The CapSense_DpFindLocalMaxSd API prepares the unified localDiffArray[][] for linear and radial sliders.
         *  Therefore, the difference of the first and the last elements of linear slider are cleared.
         */
        
        if (localMax[i] == 0u)
        {
            localDiffArray[i][CapSense_CENTROID_POS_PREV] = 0u;
        }
        else
        {
            if (localMax[i] == (snsCount - 1u))
            {
                localDiffArray[i][CapSense_CENTROID_POS_NEXT] = 0u;
            }
        }
        
        /* Si+1 - Si-1 */
        numerator = (int32) localDiffArray[i][CapSense_CENTROID_POS_NEXT] -
                    (int32) localDiffArray[i][CapSense_CENTROID_POS_PREV];

        /* Si+1 + Si + Si-1 */
        denominator = (int32) localDiffArray[i][CapSense_CENTROID_POS_PREV] + 
                      (int32) localDiffArray[i][CapSense_CENTROID_POS] + 
                      (int32) localDiffArray[i][CapSense_CENTROID_POS_NEXT];    
        
        denominator = ((numerator * (int32)multiplier) / denominator) + 
                      ((int32)localMax[i] * (int32)multiplier);
        
        /* Round result and shift 8bits left */
        position[i] = LO16(((uint32)denominator + CapSense_CENTROID_ROUND_VALUE) >> 8u);
    }
    return (uint32)(localMaxIndex);
}


#if CapSense_TOTAL_RADIAL_SLIDERS
/*******************************************************************************
* Function Name: CapSense_DpCalcRadialCentroid
****************************************************************************//**
*
* \brief
*   Calculates the position of the radial slider. 
*
* \details
*   The localMax[] is used to store the index of the maximum/maxima.
*   The localDiffArray[][] is used to store values of the maximum/maxima and neighbour differences.
*   The localMaxIndex is used to store the number of maxima have been actually found.
*
* \param *position The pointer to the position array for temporal storage.
* \param multiplier The multiplier used for position calculation. 
*                       Contains the widget resolution and total sensor information. It is generated by customizer.
* \param snsCount The total sensor number in the widget.
*
* \return Returns the number of positions calculated.
*       
*******************************************************************************/
uint32 CapSense_DpCalcRadialCentroid(uint16 position[], uint32 multiplier, uint32 snsCount)
{
    int32 numerator;
    int32 denominator;
    uint32 i;

    for (i = 0u; i < localMaxIndex; i++)
    {
        /* Si+1 - Si-1 */
        numerator = (int32) localDiffArray[i][CapSense_CENTROID_POS_NEXT] -
                    (int32) localDiffArray[i][CapSense_CENTROID_POS_PREV];

        /* Si+1 + Si + Si-1 */
        denominator = (int32) localDiffArray[i][CapSense_CENTROID_POS_PREV] + 
                      (int32) localDiffArray[i][CapSense_CENTROID_POS] + 
                      (int32) localDiffArray[i][CapSense_CENTROID_POS_NEXT];

        denominator = ((numerator * (int32)multiplier) / denominator) + 
                       ((int32)localMax[i] * (int32)multiplier);
        
        if (denominator < 0)
        {
            denominator += ((int32)snsCount * (int32)multiplier);
        }

        /* Round result and shift 8bits left */
        position[i] = LO16(((uint32)denominator + CapSense_CENTROID_ROUND_VALUE) >> 8u);
    }
    return (uint32)(localMaxIndex);
}
#endif /* #if CapSense_TOTAL_RADIAL_SLIDERS */


#if ( 0u != CapSense_CSX_TOUCHPAD_WIDGET_EN)
/*******************************************************************************
* Function Name: CapSense_DpFindLocalMaxDd
****************************************************************************//**
*
* \brief
*   Finds up to 5 local maxima in the CSX TouchPad difference data.
*
* \details
*  The column and row number of each maximum and total number of found maxima
*  are stored in the structure pointed by ptr2CsxTouchpad in 
*  the Flash Widget Structure.
*
* \param ptrFlashWdgt The pointer to the Flash Widget Structure.
*
*******************************************************************************/    
void CapSense_DpFindLocalMaxDd(
                        CapSense_FLASH_WD_STRUCT const *ptrFlashWdgt)
{    
    CapSense_RAM_WD_CSX_TOUCHPAD_STRUCT *ptrWidgetRam = ptrFlashWdgt->ptr2WdgtRam;
    CapSense_RAM_SNS_STRUCT *ptrSensor =  ptrFlashWdgt->ptr2SnsRam;    
    uint16 signalThreshold = (uint16)ptrWidgetRam->fingerTh - ptrWidgetRam->hysteresis;
    uint16 currDiff;
    uint8 rx; 
    uint8 tx;
    uint8 snsShift;
    uint8 lastRx = ptrFlashWdgt->numCols - 1u;
    uint8 lastTx = ptrFlashWdgt->numRows - 1u;     
    uint32 proceed = 0u;

    /* Initialize new touches structure */
    ptrNewTouches->touchNum = 0u;
    
    /* Go through all Rx electrodes */
    for (rx = 0u; rx <= lastRx; rx++)
    {
        /* Go through all Tx and RX(changed above) electrodes intersections and check if local maximum reqirement is met */
        for (tx = 0u; tx <= lastTx; tx++)
        {           
            proceed = 0u;

            currDiff = ptrSensor->diff;           

            if (currDiff < signalThreshold)
            {
                proceed = 1u;
            }
            
            /* Check local maximum requirement: Comparing RawCount of a local maximum candidate with 
             * RawCounts of sensors from the previous row */
            if ((0u == proceed) && (rx > 0u))
            {
                /* Sensor(i-1, j+1) */
                snsShift = lastTx;
                
                if ((tx < lastTx) && (currDiff <= (ptrSensor - snsShift)->diff))
                {
                    proceed = 1u;
                }    

                if (0u == proceed)
                {
                    /* Sensor(i-1, j) */
                    snsShift++;
                
                    if (currDiff <= (ptrSensor - snsShift)->diff)
                    {
                        proceed = 1u;
                    }                
                }
                
                if (0u == proceed)
                {
                    /* Sensor(i-1, j-1) */
                    snsShift++;
                
                    if ((tx > 0u) && (currDiff <= (ptrSensor - snsShift)->diff))
                    {
                        proceed = 1u;
                    }
                }
            }
            
            /* Check local maximum requirement: Comparing RawCount of a local maximum candidate with 
             * RawCounts of sensors from the next row */
            if ((0u == proceed) && (rx < lastRx))
            {
                /* Sensor(i+1, j+1) */
                snsShift = lastTx + 2u;

                if ((tx < lastTx) && (currDiff < (ptrSensor + snsShift)->diff))
                {
                    proceed = 1u;
                }

                if (0u == proceed)            
                {
                    /* Sensor(i+1, j) */
                    snsShift--;

                    if (currDiff < (ptrSensor + snsShift)->diff)
                    {
                        proceed = 1u;
                    }
                }

                if (0u == proceed)            
                {            
                    /* Sensor(i+1, j-1) */
                    snsShift--;

                    if ((tx > 0u) && (currDiff < (ptrSensor + snsShift)->diff))
                    {
                        proceed = 1u;
                    }
                }
            }

            /* Check local maximum requirement: Comparing RawCount of a local maximum candidate with 
             * RawCounts of sensors from the same row 
             * Sensor(i, j+1) */
            if ((0u == proceed) && (tx < lastTx))
            {               
                if (currDiff < (ptrSensor + 1u)->diff)
                {
                    proceed = 1u;
                }
            }

            /* Sensor(i, j-1) */
            if ((0u == proceed) && (tx > 0u))
            {                
                if (currDiff <= (ptrSensor - 1u)->diff)
                {
                    proceed = 1u;
                }
            }
            
            /* Add local maximum to the touch structure if there is a room */
            if ((0u == proceed) && (ptrNewTouches->touchNum < CapSense_CSX_MAX_LOCAL_PEAKS))
            {
                ptrNewTouches->touch[ptrNewTouches->touchNum].x = rx;
                ptrNewTouches->touch[ptrNewTouches->touchNum].y = tx;
                
                ptrNewTouches->touch[ptrNewTouches->touchNum].rawCount = currDiff;
                ptrNewTouches->touchNum++;
            }

            ptrSensor++;
        }
    }
}


/*******************************************************************************
* Function Name: CapSense_DpCalcTouchPadCentroid
****************************************************************************//**
*
* \brief
*   Calculates the position for each local maximum using the 3x3 algorithm.
*
* \details
*  Stores a result in the structure pointed by ptr2CsxTouchpad at the same 
*  fields as the column/row number.
*
* \param ptrFlashWdgt The pointer to the Flash Widget Structure.
*
*******************************************************************************/    
void CapSense_DpCalcTouchPadCentroid(
                        CapSense_FLASH_WD_STRUCT const *ptrFlashWdgt)
{    
    CapSense_RAM_SNS_STRUCT *ptrSensor =  ptrFlashWdgt->ptr2SnsRam;
    uint8 number;
    uint8 i;
    uint8 j;    
    uint8 lastRx = ptrFlashWdgt->numCols - 1u;
    uint8 lastTx = ptrFlashWdgt->numRows - 1u;
    uint16 centroid[3u][3u];
    int32 weightedSumX;
    int32 weightedSumY;
    uint32 totalSum;

    for(number = 0u; number < ptrNewTouches->touchNum; number++)
    {
        /* Set the sensor pointer to the local maximum sensor */
        ptrSensor =  ptrFlashWdgt->ptr2SnsRam;
        ptrSensor += (ptrNewTouches->touch[number].y + (ptrNewTouches->touch[number].x * ptrFlashWdgt->numRows));

        /* Prepare 3x3 centroid two dimensional array */
        /* Fill each row */
        for (i = 0u; i < CapSense_CSX_TOUCHPAD_CENTROID_LENGTH; i++)
        {
            /* The first  condition could be valid only when local max on the first row (0 row) of Touchpad
             * The second condition could be valid only when local max on the last row of Touchpad
             * Then corresponding row (zero or the last) of 3x3 array is initialized to 0u */
            if (((((int32)ptrNewTouches->touch[number].x - 1) + (int32)i) < 0) || 
                ((((int32)ptrNewTouches->touch[number].x - 1) + (int32)i) > (int32)lastRx))
            {   
                centroid[CapSense_CSX_TOUCHPAD_CENTROID_PREVIOUS][i] = 0u;
                centroid[CapSense_CSX_TOUCHPAD_CENTROID_CENTER][i] = 0u;
                centroid[CapSense_CSX_TOUCHPAD_CENTROID_NEXT][i] = 0u;
            }
            else
            {   
                /* Fill each column */
                for (j = 0u; j < CapSense_CSX_TOUCHPAD_CENTROID_LENGTH; j++)
                {
                    /* The first  condition could be valid only when local max on the first column (0 row) of Touchpad
                     * The second condition could be valid only when local max on the last column of Touchpad
                     * Then corresponding column (zero or the last) of 3x3 array is initialized to 0u */
                    if (((((int32)ptrNewTouches->touch[number].y - 1) + (int32)j) < 0) || 
                        ((((int32)ptrNewTouches->touch[number].y - 1) + (int32)j) > (int32)lastTx)) 
                    {   
                        centroid[j][i] = 0u;
                    }
                    else
                    {
                        centroid[j][i] = (ptrSensor + (((i - 1u) * ptrFlashWdgt->numRows) + (j - 1u)))->diff;   
                    }
                }
            }
        }
        
        weightedSumX = 0;
        weightedSumY = 0;
        totalSum = 0u;

        /* Calculate centroid */
        for (i = 0u; i < CapSense_CSX_TOUCHPAD_CENTROID_LENGTH; i++)
        {
            for (j = 0u; j < CapSense_CSX_TOUCHPAD_CENTROID_LENGTH; j++)
            {
                totalSum += centroid[i][j];
                weightedSumX += (int32)centroid[i][j] * ((int32)j - 1);
                weightedSumY += (int32)centroid[i][j] * ((int32)i - 1);
            }
        }

        /* The X position is calculated. 
        * The weightedSumX value depends on a finger position shifted regarding the X electrode (ptrNewTouches->touch[number].x)
        * The multiplier ptrFlashWdgt->xCentroidMultiplier is a short from:
        * CapSense_TOUCHPAD0_X_RESOLUTION * 256u) / (CapSense_TOUCHPAD0_NUM_RX - 1u))
        */
        weightedSumX = ((weightedSumX * (int32)ptrFlashWdgt->xCentroidMultiplier) / (int32)totalSum) + 
                        ((int32)ptrNewTouches->touch[number].x * (int32)ptrFlashWdgt->xCentroidMultiplier);

        /* The X position is rounded to the nearest integer value and normalized to the resolution range */
        ptrNewTouches->touch[number].x = LO16(((uint32)weightedSumX + CapSense_CENTROID_ROUND_VALUE) >> 8u);
        
        /* The Y position is calculated. 
        * The weightedSumY value depends on a finger position shifted regarding the Y electrode (ptrNewTouches->touch[number].y)
        * The multiplier ptrFlashWdgt->yCentroidMultiplier is a short from:
        * CapSense_TOUCHPAD0_Y_RESOLUTION * 256u) / (CapSense_TOUCHPAD0_NUM_TX - 1u))
        */
        weightedSumY = ((weightedSumY * (int32)ptrFlashWdgt->yCentroidMultiplier) / (int32)totalSum) +
                        ((int32)ptrNewTouches->touch[number].y * (int32)ptrFlashWdgt->yCentroidMultiplier);

        /* The Y position is rounded to the nearest integer value and normalized to the resolution range */
        ptrNewTouches->touch[number].y = LO16(((uint32)weightedSumY + CapSense_CENTROID_ROUND_VALUE) >> 8u);

        /* The z value is a sum of RawCounts of sensors that form 3x3 matrix with a local maximum in the center */
        ptrNewTouches->touch[number].z = LO8(totalSum >> CapSense_CSX_TOUCHPAD_Z_SHIFT);
    }    
}


/*******************************************************************************
* Function Name: CapSense_DpTouchTracking
****************************************************************************//**
*
* \brief
*   Tracks the found touches: 
*    - associates them with previous touches (DistanceMap, Hungarian)
*    - applies the position filters
*    - suppresses excessive touches
*
* \details
*  The final touch data is placed into a structure pointed by the ptr2CsxTouchpad
*  member of the Flash Widget Structure.
*
* \param ptrFlashWdgt The pointer to the Flash Widget Structure.
*
*******************************************************************************/    
void CapSense_DpTouchTracking(
                        CapSense_FLASH_WD_STRUCT const *ptrFlashWdgt)
{
    CapSense_RAM_WD_CSX_TOUCHPAD_STRUCT *ptrWidgetRam = ptrFlashWdgt->ptr2WdgtRam;
    CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches = ptrFlashWdgt->ptr2CsxTouchpad;
    CapSense_CSX_TOUCHPAD_DISTANCE_MAP *distanceMap;
    int8 *fingerPosIndex;
    static int8 touchPadBuffer[CapSense_CSX_TOUCHPAD_BUFFER_SIZE];
    uint32 i;
    uint8 newTouchNum = ptrNewTouches->touchNum;
    uint16 fingerOnThreshold = (uint16)ptrWidgetRam->fingerTh + ptrWidgetRam->hysteresis;

    if ((0u != newTouchNum) || (0u != ptrOldTouches->touchNum))
    {
        /* Initialize variables */
        newActiveIDs = 0u;

        for (i = 0u; i < ptrOldTouches->touchNum; i++)
        {
            oldActiveIDs |= (uint8)(1u << ptrOldTouches->touch[i].id);
        }
            
        /* Set up the fingerPosIndex to the head of scratch buffer. It will be used to hold association between previous and current fingers */
        fingerPosIndex = touchPadBuffer;
        
        /* This is a int32 matrix sized MAX_LOCAL_PEAKS x MAX_LOCAL_PEAKS. Make sure its start address is divisible by 4 */
        distanceMap = (CapSense_CSX_TOUCHPAD_DISTANCE_MAP *)
                      (((uint32)touchPadBuffer + (CapSense_CSX_MAX_LOCAL_PEAKS + CapSense_CSX_TOUCHPAD_PADDING)) & 
                        (uint32)~CapSense_CSX_TOUCHPAD_PADDING);

        if (0u < newTouchNum)
        {
            for (i = CapSense_CSX_MAX_LOCAL_PEAKS; i-- > 0u;)
            {
                /* Set all new touches structure array members as not assigned */
                ptrNewTouches->touch[i].id = CapSense_CSX_TOUCHPAD_UNDEFINED;
            }

            if (0u == ptrOldTouches->touchNum)
            {
                /* If the previous touch tracking had not any touches. */
                for (i = 0u; i < newTouchNum; i++)
                {
                    /* Initializes a new touch, set ID to next value and Age to 0. */
                    CapSense_NewTouch(i, ptrWidgetRam->onDebounce, fingerOnThreshold);
                }
            }
            else
            {
                /* General case */
                if (newTouchNum >= ptrOldTouches->touchNum)
                {
                    if ((1u == newTouchNum) && (1u == ptrOldTouches->touchNum))
                    {
                        /* Don't call Hungarian for 1 current and 1 previous touch. */
                        fingerPosIndex[0u] = 0;
                    }
                    else
                    {
                        /* Calculating and filling Distance Map array. */
                        CapSense_FillDistanceMap(distanceMap, ptrOldTouches);

                        /* Call Hungarian method procedure. */
                        CapSense_Hungarian(newTouchNum, ptrOldTouches->touchNum, fingerPosIndex, distanceMap);
                    }

                    for (i = 0u; i < ptrOldTouches->touchNum; i++)
                    {
                        if (ptrWidgetRam->velocity <
                            CapSense_CalcDistance((uint32)fingerPosIndex[i], i, ptrOldTouches))
                        {
                            /* Set new ID and reset Age. */
                            CapSense_NewTouch((uint32)fingerPosIndex[i], ptrWidgetRam->onDebounce, fingerOnThreshold);
                        }
                        else
                        {
                            /* Set ID to previous value and increase Age. */
                            newActiveIDs |= (uint8)(1u << (ptrOldTouches->touch[i].id));
                            CapSense_TransferTouch((uint32)fingerPosIndex[i], i, ptrOldTouches);
                        }
                    }

                    /* Added new fingers, they need to assign ID. */
                    if (newTouchNum > ptrOldTouches->touchNum)
                    {
                        /* Search new fingers. */
                        for (i = 0u; i < newTouchNum; i++)
                        {
                            /* New finger found. */
                            if (ptrNewTouches->touch[i].id == CapSense_CSX_TOUCHPAD_UNDEFINED)
                            {
                                CapSense_NewTouch(i, ptrWidgetRam->onDebounce, fingerOnThreshold);
                            }
                        }
                    }
                }
                else
                {
                    /* newTouchNum is less than ptrOldTouches->touchNum */

                    /* Calculating and filling Distance Map array. */
                    CapSense_FillDistanceMap(distanceMap, ptrOldTouches);

                    /* Call Hungarian method procedure. */
                    CapSense_Hungarian(ptrOldTouches->touchNum, newTouchNum, fingerPosIndex, distanceMap);

                    for (i = 0u; i < newTouchNum; i++)
                    {
                        if (ptrWidgetRam->velocity < 
                            CapSense_CalcDistance(i, (uint32)fingerPosIndex[i], ptrOldTouches))
                        {
                            /* Set new ID and reset Age. */
                            CapSense_NewTouch(i, ptrWidgetRam->onDebounce, fingerOnThreshold);
                        }
                        else
                        {
                            /* Set ID to previous value and increase Age. */
                            newActiveIDs |= (uint8)(1u << (ptrOldTouches->touch[i].id));
                            CapSense_TransferTouch(i, (uint32)fingerPosIndex[i], ptrOldTouches);
                        }
                    }
                }
            }

            CapSense_TouchDownDebounce();
        }

        #if CapSense_OFF_DEBOUNCE_EN
            CapSense_LiftOffDebounce(ptrWidgetRam->offDebounce, ptrOldTouches);        
        #endif
       
        if ((ptrNewTouches->touchNum >= 1u) || (ptrOldTouches->touchNum != 0u))
        {
            CapSense_SortByAge();
        }
        
        /* Store new touches as old touches */         
        for (i = ptrNewTouches->touchNum; i-- > 0u;)
        {
            CapSense_CopyTouchRecord(&ptrOldTouches->touch[i], &ptrNewTouches->touch[i]);
        }

        ptrOldTouches->touchNum = ptrNewTouches->touchNum;

        /* Save ID allocation masks for usage in the next iteration. */
        oldActiveIDs = newActiveIDs;
    }
 }


/*******************************************************************************
* Function Name: CapSense_TransferTouch
****************************************************************************//**
*
* \brief
*   Copies ID, increments age and decrements debounce(if debounce > 0) parameters 
*   for the new touch that has been detected also in the old touches. 
*
* \details
*   Copies ID, increments age and decrements debounce(if debounce > 0) parameters 
*   for the new touch that has been detected also in the old touches. 
*
* \param newIndex The touch index of touch array in the new touches structure.
* \param oldIndex The touch index of touch array in the old touches structure.
* \param ptrOldTouches The pointer to the old touches structure.
*
*******************************************************************************/
CY_INLINE static void CapSense_TransferTouch(
                            uint32 newIndex, uint32 oldIndex, CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches)
{
    ptrNewTouches->touch[newIndex].id = ptrOldTouches->touch[oldIndex].id;

    ptrNewTouches->touch[newIndex].age = ptrOldTouches->touch[oldIndex].age + 1u;
    
    /* Decrement debounce counter if the touch in touch down ot lift off stage (debounce counter > 0) */
    if (0u != (ptrOldTouches->touch[oldIndex].debounce & CapSense_CSX_TOUCHPAD_DEBOUNCE_MASK))
    {               
        ptrNewTouches->touch[newIndex].debounce = ptrOldTouches->touch[oldIndex].debounce - 1u;
    }
    else
    {
        ptrNewTouches->touch[newIndex].debounce = ptrOldTouches->touch[oldIndex].debounce;
    }
}


/*******************************************************************************
* Function Name: CapSense_NewTouch
****************************************************************************//**
*
* \brief
*   Set ID, age and on debounce parameters for a new touch. 
*
* \details
*   RawCount of a local maximum has to be higher than fingerOnThreshold threshold 
*   so the new touch is accepted. If RawCount is lower than the fingerOnThreshold 
*   than corresponding touch is marked with CapSense_CSX_TOUCHPAD_FT_ON_FAIL
*   ID and will be deleted from new touches structure in CapSense_SortByAge()
*   (new touches structure is reorganized in this case).  
*
* \param newIndex The touch index of touch array in the new touches structure.
* \param onDebounce Counter value for touch down procedure.
* \param fingerOnThreshold The finger ON threshold (finger threshold + hysteresis)
*
*******************************************************************************/
static void CapSense_NewTouch(uint32 newIndex, uint8 onDebounce, uint16 fingerOnThreshold)
{
    uint32 idx;

    /* Touch is not accepted */
    if (ptrNewTouches->touch[newIndex].rawCount < fingerOnThreshold)
    {
        ptrNewTouches->touch[newIndex].id = CapSense_CSX_TOUCHPAD_FT_ON_FAIL;
    }
    else
    {
        /* Create a bit map of ID's currently used and previously used and search for the new lowest ID */
        idx = CapSense_GetLowestId(((uint16)oldActiveIDs | (uint16)newActiveIDs));

        /* Indicate that ID is now taken */
        newActiveIDs |= (uint8)(1u << idx);

        /* Set the ID */
        ptrNewTouches->touch[newIndex].id = LO8(idx);

        /* Set the age */
        ptrNewTouches->touch[newIndex].age = 1u;
        
        /* New Finger goes through touchdown debounce procedure if required */
        ptrNewTouches->touch[newIndex].debounce = onDebounce | CapSense_CSX_TOUCHPAD_TOUCH_DOWN;
    }
}


/*******************************************************************************
* Function Name: CapSense_GetLowestId
****************************************************************************//**
*
* \brief
*   Returns the lowest available touch ID.
*
* \details
*   Returns the lowest available touch ID.
*
* \param idMask The mask of IDs used in new and old touches.
*
* \return   Returns the lowest available touch ID. If no ID was available, 
*           CapSense_CSX_TOUCHPAD_TOUCH_ABSENT_ID is returned.
*
*******************************************************************************/
CY_INLINE static uint8 CapSense_GetLowestId(uint16 idMask)
{
    uint32 idx;
    uint32 touchId = CapSense_CSX_TOUCHPAD_TOUCH_ABSENT_ID;

    /* Search for the lowest available ID. */
    idMask >>= CapSense_CSX_TOUCHPAD_TOUCH_MIN_ID;
    
    for (idx = CapSense_CSX_TOUCHPAD_TOUCH_MIN_ID; idx <= CapSense_CSX_TOUCHPAD_TOUCH_MAX_ID; idx++)
    {
        /* Determine if the new ID is available. */
        if (0u == (idMask & 1u))
        {
            touchId = idx;
            break;
        }

        idMask >>= 1u;
    }

    /* Return an indicator of failure. */
    return (uint8)(touchId);
}


/*******************************************************************************
* Function Name: CapSense_TouchDownDebounce
****************************************************************************//**
*
* \brief
*   Handles touch down de-bouncing.
*
* \details
*   Even if a new touch is detected it is not considered as active until
*   on debounce counter has not reached zero. If on debounce counter has reached zero
*   the touch down mask is cleared. Otherwise the age of the new finger is cleared 
*   (it is considered as not active)  
*
*******************************************************************************/
CY_INLINE static void CapSense_TouchDownDebounce(void)
{
    uint32 i;
           
    for (i = 0u; i < ptrNewTouches->touchNum; i++)
    {
        /* If it is a touchdown debouncing finger its age is set to zero */
        if (0u != (ptrNewTouches->touch[i].debounce & CapSense_CSX_TOUCHPAD_TOUCH_DOWN))
        {
            if ((ptrNewTouches->touch[i].debounce & CapSense_CSX_TOUCHPAD_DEBOUNCE_MASK) <= 1u)
            {
                /* If debounce counter reached zero than touch down stage 
                 * is finished and corresponding mask is cleared 
                 */
                ptrNewTouches->touch[i].debounce &= 
                    (uint8)~(CapSense_CSX_TOUCHPAD_TOUCH_DOWN | CapSense_CSX_TOUCHPAD_DEBOUNCE_MASK);
            }
            else
            {
                /* Set the age to zero - finger is not active */
                ptrNewTouches->touch[i].age = 0u;
            }
        }        
    }   
}


/*******************************************************************************
* Function Name: CapSense_LiftOffDebounce
****************************************************************************//**
*
* \brief
*   Handles lift off de-bouncing.
*
* \details
*   Even if a touch is not present in new touches, but present in old
*   touches this means it is lift off event. This touch is considered as active and 
*   is added to new touches structure until the offDebounce counter reaches zero.
*   The offDebounce counter is set when lift off event is detected and is decremented 
*   each time the CapSense_LiftOffDebounce() API is called. 
*
* \param offDebounce   The off debounce counter value.
* \param ptrOldTouches The pointer to the old touches structure.
*
*******************************************************************************/
#if CapSense_OFF_DEBOUNCE_EN
CY_INLINE static void CapSense_LiftOffDebounce(uint8 offDebounce, 
                                                       CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches)
{
    uint32 i;
    uint8 liftOffMask = (uint8)(~newActiveIDs) & oldActiveIDs;

    if(liftOffMask != 0u)
    {    
        for (i = 0u; i < ptrOldTouches->touchNum; i++)
        {
            /* 
             * The old touch is not found in the new touches.
             * Lift off procedure starts and the old touch is copied to the new touches if there is a room.
             */
            
            if (((uint8)(0x01u << ptrOldTouches->touch[i].id) & liftOffMask) != 0u) 
            {
                /* If there is a room in new touches structure and the old touch is not under touch down debounce procedure */
                if ((ptrNewTouches->touchNum < CapSense_CSX_MAX_LOCAL_PEAKS) && 
                    (0u == (ptrOldTouches->touch[i].debounce & CapSense_CSX_TOUCHPAD_TOUCH_DOWN)))
                {                                     
                    /* Lift off debouncing starts if there is not lift off mask in the old touch */
                    if ((ptrOldTouches->touch[i].debounce & CapSense_CSX_TOUCHPAD_LIFT_OFF) == 0u)
                    {
                        ptrOldTouches->touch[i].debounce = offDebounce | CapSense_CSX_TOUCHPAD_LIFT_OFF;

                        /* Copy the old finger data to the new one using TransferTouch API to manage age and debounce */
                        CapSense_TransferTouch((uint32)ptrNewTouches->touchNum, i, ptrOldTouches);
                        ptrNewTouches->touch[ptrNewTouches->touchNum].x = ptrOldTouches->touch[i].x;
                        ptrNewTouches->touch[ptrNewTouches->touchNum].y = ptrOldTouches->touch[i].y;
                        ptrNewTouches->touch[ptrNewTouches->touchNum].z = ptrOldTouches->touch[i].z;
                        ptrNewTouches->touch[ptrNewTouches->touchNum].rawCount = ptrOldTouches->touch[i].rawCount;
                        ptrNewTouches->touchNum++;
                    }
                    else
                    {
                        /* Check whether debounce counter is reached zero if there is a lift off mask in the old touch */
                       if ((ptrOldTouches->touch[i].debounce & CapSense_CSX_TOUCHPAD_DEBOUNCE_MASK) == 0u)
                       {
                            /* Clear lift off mask if debounce counter has reached zero */
                            ptrOldTouches->touch[i].debounce &= (uint8)~CapSense_CSX_TOUCHPAD_LIFT_OFF;
                       }
                       else
                       {
                            /* Copy the old finger data to the new one using TransferTouch API to manage age and debounce */
                            CapSense_TransferTouch((uint32)ptrNewTouches->touchNum, i, ptrOldTouches);
                            ptrNewTouches->touch[ptrNewTouches->touchNum].x = ptrOldTouches->touch[i].x;
                            ptrNewTouches->touch[ptrNewTouches->touchNum].y = ptrOldTouches->touch[i].y;
                            ptrNewTouches->touch[ptrNewTouches->touchNum].z = ptrOldTouches->touch[i].z;
                            ptrNewTouches->touch[ptrNewTouches->touchNum].rawCount = ptrOldTouches->touch[i].rawCount;
                            ptrNewTouches->touchNum++;
                       } 
                    }
                }
            }
        }
    }
}
#endif /* CapSense_OFF_DEBOUNCE_EN */

/*******************************************************************************
* Function Name: CapSense_CalcDistance
****************************************************************************//**
*
* \brief
*   Calculates squared distance between old and new touches pointed by the input 
*   parameters.
*
* \details
*   Calculates squared distance between old and new touches pointed by the input 
*   parameters.
*
* \param newIndex      The index of touch in the new touches structure.
* \param oldIndex      The index of touch in the old touches structure.
* \param ptrOldTouches The pointer to the old touches structure.
*
* \return Returns the squared distance.
*
*******************************************************************************/
CY_INLINE static uint32 CapSense_CalcDistance(uint32 newIndex, uint32 oldIndex, 
                                                      CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches)
{
    int32 result;
    int32 xDistance = (int32)(ptrOldTouches->touch[oldIndex].x) - (int32)(ptrNewTouches->touch[newIndex].x);
    int32 yDistance = (int32)(ptrOldTouches->touch[oldIndex].y) - (int32)(ptrNewTouches->touch[newIndex].y);
    
    result = ((xDistance * xDistance) + (yDistance *yDistance));
    return (uint32)result;
}


/*******************************************************************************
* Function Name: CapSense_FillDistanceMap
****************************************************************************//**
*
* \brief
*   Populates the distance map used for the Hungarian method with distance
*   squared values.
*
* \details
*   Distance squared is used instead of distance to eliminate square root
*   calculation, which requires a lot of execution time.
*
* \param distanceMap   The pointer to a 2-dimensional array of int32's 
*                          with dimensions CapSense_CSX_MAX_LOCAL_PEAKS x 
*                          CapSense_CSX_MAX_LOCAL_PEAKS
* \param ptrOldTouches The pointer to the old touches structure.
*
* \return The contents of distanceMap[][] are modified.
*
*******************************************************************************/
static void CapSense_FillDistanceMap(CapSense_CSX_TOUCHPAD_DISTANCE_MAP *distanceMap, 
                                             CapSense_CSX_TOUCHPAD_DATA_STRUCT *ptrOldTouches)
{
    uint32 i;
    uint32 j;

    if (ptrNewTouches->touchNum >= ptrOldTouches->touchNum)
    {
        for (i = ptrNewTouches->touchNum; i-- > 0u; )
        {
            for (j = ptrOldTouches->touchNum; j-- > 0u; )
            {
                distanceMap[i][j] = (int32)CapSense_CalcDistance(i, j, ptrOldTouches);
            }
        }
    }
    else
    {
        for (i = ptrOldTouches->touchNum; i-- > 0u;)
        {
            for (j = ptrNewTouches->touchNum; j-- > 0u;)
            {
                distanceMap[i][j] = (int32)CapSense_CalcDistance(j, i, ptrOldTouches);
            }
        }
    }
}


/*******************************************************************************
* Function Name: CapSense_Hungarian
****************************************************************************//**
*
* \brief
*   Executes the Hungarian Method on a distance map to track motion of two touch sets
*   (old touches vs new touches)
*
* \details
*   This function uses the Hungarian method described in specification 001-63362.
*   There is no bounds checking on the parameters. It is the calling function's 
*   responsibility to ensure parameter validity.
*
* \param rowCount       Number of elements in row of distanceMap matrix. This value must be
*                       greater than or equal to colCount.
* \param colCount       Number of elements in column of distanceMap matrix.This value must 
*                       be greater than or equal to 1.
* \param fingerPosIndex The pointer to an array where associations between the
*                             previous and current touches are returned.
* \param distanceMap    The 2-dimensional map of distances between the nodes in each
*                          coordinate set. The 1st index of distanceMap corresponds to nodes 
*                          in the 1st coordinate data set, and the 2nd index of distanceMap
*                          corresponds to the 2nd coordinate data set. Each element in 
*                          distanceMap is the square of the distance between the 
*                          corresponding coordinates in the 1st and 2nd data set.
*
*
* \return Returns fingerPosIndex, the associations between the 1st and 2nd coordinate set are
*                          returned in the array referenced by this pointer. It is indexed 
*                          by the 2nd coordinate set, and its elements are the corresponding 
*                          index in the 1st coordinate set.
*
*******************************************************************************/
static void CapSense_Hungarian(uint8 rowCount, uint8 colCount, int8 *fingerPosIndex,
                             CapSense_CSX_TOUCHPAD_DISTANCE_MAP *distanceMap)
{
    int32 col[CapSense_CSX_MAX_LOCAL_PEAKS];
    int32 row[CapSense_CSX_MAX_LOCAL_PEAKS];
    int32 *mins;
    int32 delta;

    int8 *links;
    int8 *visited;
    int8 *markIndices;
    
    int32 colValue = 0;
    int32 markedI = 0;
    int32 markedJ = 0;
    int32 i = 0;
    int32 i1 = 0;
    int32 j = 0;
    int32 j1 = 0;

    mins = (int32 *)(distanceMap + CapSense_CSX_MAX_LOCAL_PEAKS);
    links = (int8 *)(mins + CapSense_CSX_MAX_LOCAL_PEAKS);
    visited = links + CapSense_CSX_MAX_LOCAL_PEAKS;
    markIndices = visited + CapSense_CSX_MAX_LOCAL_PEAKS;
    
    (void)memset(col, 0, (uint32)colCount * 4u); 

    /* Initialize row and markIndices arrays */
    for (i1 = (int32)rowCount; i1-- > 0;)
    {
        row[i1] = 0;
        markIndices[i1] = -1;
    }

    /* Go through all columns */
    for (i = (int32)colCount; i-- > 0;)
    {
        /* Initialize visited, links, mins arrays. They used for every column */
        for (i1 = (int32)rowCount; i1-- > 0;)
        {
            visited[i1] = 0;
            links[i1] = -1;
            mins[i1] = (int32)CapSense_CSX_TOUCHPAD_INT32;
        }

        /* Next two variables are used to mark column and row */
        markedI = i;
        markedJ = -1;

        while (markedI != -1)
        {
            j = -1;
            colValue = col[markedI];

            /* Go through all rows */
            for (j1 = (int32)rowCount; j1-- > 0;)
            {
                if (visited[j1] == 0)
                {
                    delta = distanceMap[j1][markedI] - row[j1] - colValue;
                    
                    /* Find the minimum element index in column i */
                    if (mins[j1] > delta)
                    {
                        mins[j1] = delta;
                        links[j1] = (int8)markedJ;
                    }
                    
                    if ((j == -1) || (mins[j1] < mins[j]))
                    {
                        j = j1;
                    }
                }
            }

            delta = mins[j];
            
            /* Go through all row */
            for (j1 = (int32)rowCount; j1-- > 0;)
            {
                if (visited[j1] != 0)
                {
                    col[markIndices[j1]] += delta;
                    row[j1] -= delta;
                }
                else
                {
                    mins[j1] -= delta;
                }
            }
            
            col[i] +=  delta;

            visited[j] = 1;
            markedJ = j;
            markedI = markIndices[j];
        }

        while (links[j] != -1)
        {
            markIndices[j] = markIndices[links[j]];
            j = links[j];
        }
        
        markIndices[j] = (int8)i;
    }

    /* Provide an association between two sets of touches */
    for (j = (int32)rowCount; j-- > 0;)
    {
        if (markIndices[j] != -1)
        {
            fingerPosIndex[markIndices[j]] = (int8)j;
        }
    }
}


/*******************************************************************************
* Function Name: CapSense_SortByAge
****************************************************************************//**
*
* \brief
*   Sorts the new touches array by age (in decrementing order) and Id (in incrementing order) fields. 
*
* \details
*   Sorts the new touches array by age (in decrementing order) and Id (in incrementing order) fields. 
*
*******************************************************************************/
CY_INLINE static void CapSense_SortByAge(void)
{
    uint32 i;
    uint32 j;
    CapSense_CSX_TOUCHPAD_PEAK_STRUCT tempTouch;
    uint32 newTouchNum = ptrNewTouches->touchNum;
    
    /* 
     * Delete failed touch by coping(rewriting) the last touch content to the failed touch.
     * If the last touch record is invalid try the penult touch record and so on.     
     */
    
    for (i = 0u; i < newTouchNum; i++)
    {
        if (ptrNewTouches->touch[i].id > CapSense_CSX_TOUCHPAD_TOUCH_MAX_ID)
        {
            for (j = (newTouchNum - 1u); j > i; j--)
            {
                /* Check the touch records from the last to the current. 
                 * If the touch record is valid then replace the current touch record 
                 */
                if (ptrNewTouches->touch[j].id <= CapSense_CSX_TOUCHPAD_TOUCH_MAX_ID)
                {
                    CapSense_CopyTouchRecord(&ptrNewTouches->touch[i], &ptrNewTouches->touch[j]);

                    /* Finish the loop. The valid touch record is found and copied down */
                    break;
                }
                else
                {
                    /* Decrement the number of touch records.
                     * The last touch record is invalid - try the penult touch record 
                     */
                    newTouchNum--;
                }
            }

            /* Decrement the number of touch records.
             * The last touch record is valid and copied to the current position (i) 
             */
            newTouchNum--;
        }
    }

    /* Set new number of touches */
    ptrNewTouches->touchNum = (uint8)newTouchNum;

    /* Sort new touches structure */
    for (i = 0u; i < newTouchNum; i++)
    {
        for (j = (i + 1u); j < newTouchNum; j++)
        {
            /* if next touches have higher age or lower id with the same age then swap touches */
            if ((ptrNewTouches->touch[i].age < ptrNewTouches->touch[j].age)      || 
                ((ptrNewTouches->touch[i].age == ptrNewTouches->touch[j].age) && 
                 (ptrNewTouches->touch[i].id > ptrNewTouches->touch[j].id)))
            {
                /* Swap touches */
                CapSense_CopyTouchRecord(&tempTouch, &ptrNewTouches->touch[i]);
                CapSense_CopyTouchRecord(&ptrNewTouches->touch[i], &ptrNewTouches->touch[j]);
                CapSense_CopyTouchRecord(&ptrNewTouches->touch[j], &tempTouch);
            }
        }       
    }
}


/*******************************************************************************
* Function Name: CapSense_CopyTouchRecord
****************************************************************************//**
*
* \brief
*   Copies content from the source touch structure to the destination touch structure.
*
* \details
*   Copies content from the source touch structure to the destination touch structure.
*
* \param *destination The pointer to the destination touch structure.
* \param *source      The pointer to the source touch structure.
*
*******************************************************************************/
CY_INLINE static void CapSense_CopyTouchRecord(CapSense_CSX_TOUCHPAD_PEAK_STRUCT *destination, 
                                                       CapSense_CSX_TOUCHPAD_PEAK_STRUCT *source)
{
    destination->x        =  source->x;
    destination->y        =  source->y;
    destination->z        =  source->z;
    destination->rawCount =  source->rawCount;
    destination->id       =  source->id;
    destination->age      =  source->age;
    destination->debounce =  source->debounce;
}


/*******************************************************************************
* Function Name: CapSense_FilterTouchRecord
****************************************************************************//**
*
* \brief
*   Filter position data of every valid touch, if there is a room in RAM Widget Data Structure
*   and update corresponding fields in RAM Widget Data Structure
*
* \details
*  This API checks every touch in the new touches structure if it is valid until there is a room 
*  in the RAM Widget Data Structure (where touch reports are stored). If touch is valid 
*  (valid id and age > 0) then touch is filtered if filter is enabled. At the end the 
*  corresponding fields are updated in the RAM Widget Data Structure.
*
* \param ptrFlashWdgt The pointer to the Flash Widget Structure.
*
* \return The number of reported touches to the RAM Data Structure.
*
*******************************************************************************/
uint32 CapSense_DpFilterTouchRecord(CapSense_FLASH_WD_STRUCT const *ptrFlashWdgt)
{    
    CapSense_RAM_WD_CSX_TOUCHPAD_STRUCT *ptrWidgetRam = ptrFlashWdgt->ptr2WdgtRam;
    uint32 i;
    uint32 reportedTouchNum = 0u;    
   
    #if (0u != CapSense_CSX_TOUCHPAD_POSITION_FILTER_EN)
        uint32 oldIdMask = 0u;
        uint32 j;
        uint32 filterConfig;    
        uint16 filteredXPos = CapSense_CSX_TOUCHPAD_POSITION_NONE;
        uint16 filteredYPos = CapSense_CSX_TOUCHPAD_POSITION_NONE;
        
        #if (0u != CapSense_CSX_TOUCHPAD_POS_MEDIAN_FILTER_EN)
            CapSense_TOUCHPAD_POS_HISTORY_STRUCT *ptrHistory = 
                ptrFlashWdgt->ptr2PosHistory;
        #endif /* 0u != CapSense_CSX_TOUCHPAD_POS_MEDIAN_FILTER_EN */
        
        /* Find what Ids were active in the previous report */
        for (i = 0u; i < CapSense_CSX_MAX_FINGERS; i++)
        {
            oldIdMask |= 1Lu << ptrWidgetRam->touch[i].id;
        }
    #endif /* (0u != CapSense_CSX_TOUCHPAD_POSITION_FILTER_EN) */

    /* Go through all touch fields in RAM Data Structure */
    for (i = 0u; i < CapSense_CSX_MAX_FINGERS; i++)
    {
        /* Check if touch is valid to be reported: age has to be higher than 0. Id has been checked in SortByAge API */
        if ((i < ptrNewTouches->touchNum) && (ptrNewTouches->touch[i].age > 0u))
        {
            #if (0u == CapSense_CSX_TOUCHPAD_POSITION_FILTER_EN)
                /* Report touch to the RAM Data Structure if position filter is disabled in general */
                ptrWidgetRam->touch[i].x = ptrNewTouches->touch[i].x;
                ptrWidgetRam->touch[i].y = ptrNewTouches->touch[i].y;
                ptrWidgetRam->touch[i].z = ptrNewTouches->touch[i].z;
                ptrWidgetRam->touch[i].id = ptrNewTouches->touch[i].id;
            #else
                
                filterConfig = ptrFlashWdgt->staticConfig;

                /* Initialize position history if the touch has the id that was absent in the previous touch report */
                if (0u == (oldIdMask & (1Lu << ptrNewTouches->touch[i].id)))
                {
                    ptrWidgetRam->touch[i].x = ptrNewTouches->touch[i].x;
                    ptrWidgetRam->touch[i].y = ptrNewTouches->touch[i].y;
                    ptrWidgetRam->touch[i].z = ptrNewTouches->touch[i].z;
                    ptrWidgetRam->touch[i].id = ptrNewTouches->touch[i].id;

                    #if (0u != CapSense_CSX_TOUCHPAD_POS_MEDIAN_FILTER_EN)
                    if (0u != (filterConfig & CapSense_POS_MEDIAN_FILTER_MASK))
                    {
                        ptrHistory[i].xPos.posMedianZ1 = ptrNewTouches->touch[i].x;
                        ptrHistory[i].xPos.posMedianZ2 = ptrNewTouches->touch[i].x;
                        ptrHistory[i].yPos.posMedianZ1 = ptrNewTouches->touch[i].y;
                        ptrHistory[i].yPos.posMedianZ2 = ptrNewTouches->touch[i].y;
                    }
                    #endif
                }
                /* Find corresponding position history of the new touch (by id) in the RAW Widget Data Structure and Position History */
                else
                {
                    for(j = 0u; j < CapSense_CSX_MAX_FINGERS; j++)
                    {
                        if(ptrNewTouches->touch[i].id == ptrWidgetRam->touch[j].id)
                        {
                            /* Run filter */
                            #if (0u != CapSense_POS_IIR_FILTER_EN)
                            if (0u != (filterConfig & CapSense_POS_IIR_FILTER_MASK))
                            {
                                filteredXPos = (uint16)CapSense_FtIIR1stOrder((uint32)ptrNewTouches->touch[i].x, 
                                                                                      (uint32)ptrWidgetRam->touch[j].x , 
                                                                                      CapSense_POS_IIR_COEFF, 0u);
                                filteredYPos = (uint16)CapSense_FtIIR1stOrder((uint32)ptrNewTouches->touch[i].y, 
                                                                                      (uint32)ptrWidgetRam->touch[j].y , 
                                                                                      CapSense_POS_IIR_COEFF, 0u);

                            }
                            #endif

                            #if (0u != CapSense_CSX_TOUCHPAD_POS_MEDIAN_FILTER_EN)
                            if (0u != (filterConfig & CapSense_POS_MEDIAN_FILTER_MASK))
                            {
                                CYASSERT(ptrHistory); /* ptrHistory cannot be NULL if median filter is enabled*/
                                                                
                                /* Run median filter */
                                filteredXPos = CapSense_FtMedian(ptrHistory[i].xPos.posMedianZ2, ptrHistory[i].xPos.posMedianZ1, ptrNewTouches->touch[i].x);
                                filteredYPos = CapSense_FtMedian(ptrHistory[i].yPos.posMedianZ2, ptrHistory[i].yPos.posMedianZ1, ptrNewTouches->touch[i].y);

                                /* Update history */
                                ptrHistory[i].xPos.posMedianZ2 = ptrHistory[i].xPos.posMedianZ1;
                                ptrHistory[i].xPos.posMedianZ1 = ptrNewTouches->touch[i].x;
                                ptrHistory[i].yPos.posMedianZ2 = ptrHistory[i].yPos.posMedianZ1;
                                ptrHistory[i].yPos.posMedianZ1 = ptrNewTouches->touch[i].y;   
                            }
                            #endif

                            #if (0u != CapSense_POS_AVERAGE_FILTER_EN)
                            if (0u != (filterConfig & CapSense_POS_AVERAGE_FILTER_MASK))
                            {
                                filteredXPos = (ptrNewTouches->touch[i].x + ptrWidgetRam->touch[j].x) >> 1u;
                                filteredYPos = (ptrNewTouches->touch[i].y + ptrWidgetRam->touch[j].y) >> 1u;
                            }    
                            #endif

                            #if (0u != CapSense_POS_JITTER_FILTER_EN)
                            if (0u != (filterConfig & CapSense_POS_JITTER_FILTER_MASK))
                            {
                                filteredXPos = (uint16)CapSense_FtJitter((uint32)ptrNewTouches->touch[i].x, 
                                                                                 (uint32)ptrWidgetRam->touch[j].x);
                                filteredYPos = (uint16)CapSense_FtJitter((uint32)ptrNewTouches->touch[i].y, 
                                                                                 (uint32)ptrWidgetRam->touch[j].y);
                            }
                            #endif

                            ptrWidgetRam->touch[i].x = filteredXPos;
                            ptrWidgetRam->touch[i].y = filteredYPos;
                            ptrWidgetRam->touch[i].z = ptrNewTouches->touch[i].z;
                            ptrWidgetRam->touch[i].id = ptrNewTouches->touch[i].id;
                        }
                    }
                }
            #endif
            reportedTouchNum++;
        }
        else
        {
            ptrWidgetRam->touch[i].x = CapSense_CSX_TOUCHPAD_POSITION_NONE;
            ptrWidgetRam->touch[i].y = CapSense_CSX_TOUCHPAD_POSITION_NONE;
            ptrWidgetRam->touch[i].z = LO8(CapSense_CSX_TOUCHPAD_POSITION_NONE);
            ptrWidgetRam->touch[i].id = CapSense_CSX_TOUCHPAD_UNDEFINED;
        }
    }
    return reportedTouchNum;
}

#endif /* #if CapSense_CSX_TOUCHPAD_WIDGET_EN */
    
    
/* [] END OF FILE */
