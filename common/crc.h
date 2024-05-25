#ifndef STM32L4xx_HAL_CRC_PERSO_H
#define STM32L4xx_HAL_CRC_PERSO_H

#ifdef PROFFIEBOARD

    #include <stdio.h>
    #include "stm32l4xx.h"

    /**
     ******************************************************************************
    * @file    stm32l4xx_hal_crc.h
    * @author  MCD Application Team
    * @brief   Header file of CRC HAL module.
    ******************************************************************************
    * @attention
    *
    * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
    * All rights reserved.</center></h2>
    *
    * This software component is licensed by ST under BSD 3-Clause license,
    * the "License"; You may not use this file except in compliance with the
    * License. You may obtain a copy of the License at:
    *                        opensource.org/licenses/BSD-3-Clause
    *
    ******************************************************************************
    */

    /**
     * @brief  HAL Status structures definition
     */
    typedef enum
    {
    HAL_CRC_OK       = 0x00,
    HAL_CRC_ERROR    = 0x01,
    HAL_CRC_BUSY     = 0x02,
    HAL_CRC_TIMEOUT  = 0x03
    } HAL_CRCStatusTypeDef;

    /**
     * @brief  HAL Lock structures definition
     */
    typedef enum
    {
    HAL_CRC_UNLOCKED = 0x00,
    HAL_CRC_LOCKED   = 0x01
    } HAL_CRCLockTypeDef;

    #define UNUSED_CRC(X) (void)X      		/* To avoid gcc/g++ warnings */
    #define assert_param_crc(X) (void)X     /* To avoid gcc/g++ warnings */

    #define __HAL_CRC_LOCK(__HANDLE__)                                           \
                                do{                                        \
                                    if((__HANDLE__)->Lock == HAL_CRC_LOCKED)   \
                                    {                                      \
                                        return HAL_CRC_BUSY;                    \
                                    }                                      \
                                    else                                   \
                                    {                                      \
                                        (__HANDLE__)->Lock = HAL_CRC_LOCKED;    \
                                    }                                      \
                                    }while (0)

    #define __HAL_CRC_UNLOCK(__HANDLE__)                                          \
                                    do{                                       \
                                        (__HANDLE__)->Lock = HAL_CRC_UNLOCKED;    \
                                    }while (0)


    /**
     * @brief  CRC HAL State Structure definition
     */
    typedef enum
    {
    HAL_CRC_STATE_RESET     = 0x00U,  /*!< CRC not yet initialized or disabled */
    HAL_CRC_STATE_READY     = 0x01U,  /*!< CRC initialized and ready for use   */
    HAL_CRC_STATE_BUSY      = 0x02U,  /*!< CRC internal process is ongoing     */
    HAL_CRC_STATE_TIMEOUT   = 0x03U,  /*!< CRC timeout state                   */
    HAL_CRC_STATE_ERROR     = 0x04U   /*!< CRC error state                     */
    } HAL_CRC_StateTypeDef;

    /**
     * @brief CRC Init Structure definition
     */
    typedef struct
    {
    uint8_t DefaultPolynomialUse;       /*!< This parameter is a value of @ref CRC_Default_Polynomial and indicates if default polynomial is used.
                                                If set to DEFAULT_POLYNOMIAL_ENABLE, resort to default
                                                X^32 + X^26 + X^23 + X^22 + X^16 + X^12 + X^11 + X^10 +X^8 + X^7 + X^5 + X^4 + X^2+ X +1.
                                                In that case, there is no need to set GeneratingPolynomial field.
                                                If otherwise set to DEFAULT_POLYNOMIAL_DISABLE, GeneratingPolynomial and CRCLength fields must be set. */

    uint8_t DefaultInitValueUse;        /*!< This parameter is a value of @ref CRC_Default_InitValue_Use and indicates if default init value is used.
                                            If set to DEFAULT_INIT_VALUE_ENABLE, resort to default
                                            0xFFFFFFFF value. In that case, there is no need to set InitValue field.
                                            If otherwise set to DEFAULT_INIT_VALUE_DISABLE,  InitValue field must be set. */

    uint32_t GeneratingPolynomial;      /*!< Set CRC generating polynomial as a 7, 8, 16 or 32-bit long value for a polynomial degree
                                            respectively equal to 7, 8, 16 or 32. This field is written in normal representation,
                                            e.g., for a polynomial of degree 7, X^7 + X^6 + X^5 + X^2 + 1 is written 0x65.
                                            No need to specify it if DefaultPolynomialUse is set to DEFAULT_POLYNOMIAL_ENABLE.   */

    uint32_t CRCLength;                 /*!< This parameter is a value of @ref CRC_Polynomial_Sizes and indicates CRC length.
                                            Value can be either one of
                                            @arg @ref CRC_POLYLENGTH_32B                  (32-bit CRC),
                                            @arg @ref CRC_POLYLENGTH_16B                  (16-bit CRC),
                                            @arg @ref CRC_POLYLENGTH_8B                   (8-bit CRC),
                                            @arg @ref CRC_POLYLENGTH_7B                   (7-bit CRC). */

    uint32_t InitValue;                 /*!< Init value to initiate CRC computation. No need to specify it if DefaultInitValueUse
                                            is set to DEFAULT_INIT_VALUE_ENABLE.   */

    uint32_t InputDataInversionMode;    /*!< This parameter is a value of @ref CRCEx_Input_Data_Inversion and specifies input data inversion mode.
                                            Can be either one of the following values
                                            @arg @ref CRC_INPUTDATA_INVERSION_NONE       no input data inversion
                                            @arg @ref CRC_INPUTDATA_INVERSION_BYTE       byte-wise inversion, 0x1A2B3C4D becomes 0x58D43CB2
                                            @arg @ref CRC_INPUTDATA_INVERSION_HALFWORD   halfword-wise inversion, 0x1A2B3C4D becomes 0xD458B23C
                                            @arg @ref CRC_INPUTDATA_INVERSION_WORD       word-wise inversion, 0x1A2B3C4D becomes 0xB23CD458 */

    uint32_t OutputDataInversionMode;   /*!< This parameter is a value of @ref CRCEx_Output_Data_Inversion and specifies output data (i.e. CRC) inversion mode.
                                                Can be either
                                                @arg @ref CRC_OUTPUTDATA_INVERSION_DISABLE   no CRC inversion,
                                                @arg @ref CRC_OUTPUTDATA_INVERSION_ENABLE    CRC 0x11223344 is converted into 0x22CC4488 */
    } CRC_InitTypeDef;

    /**
     * @brief  CRC Handle Structure definition
     */
    typedef struct
    {
    CRC_TypeDef                 *Instance;   /*!< Register base address        */

    CRC_InitTypeDef             Init;        /*!< CRC configuration parameters */

    HAL_CRCLockTypeDef          Lock;        /*!< CRC Locking object           */

    __IO HAL_CRC_StateTypeDef   State;       /*!< CRC communication state      */

    uint32_t InputDataFormat;                /*!< This parameter is a value of @ref CRC_Input_Buffer_Format and specifies input data format.
                                                Can be either
                                                @arg @ref CRC_INPUTDATA_FORMAT_BYTES       input data is a stream of bytes (8-bit data)
                                                @arg @ref CRC_INPUTDATA_FORMAT_HALFWORDS   input data is a stream of half-words (16-bit data)
                                                @arg @ref CRC_INPUTDATA_FORMAT_WORDS       input data is a stream of words (32-bit data)

                                            Note that constant CRC_INPUT_FORMAT_UNDEFINED is defined but an initialization error
                                            must occur if InputBufferFormat is not one of the three values listed above  */
    } CRC_HandleTypeDef;
    /**
     * @}
     */

    /* Exported constants --------------------------------------------------------*/
    /** @defgroup CRC_Exported_Constants CRC Exported Constants
     * @{
     */

    /** @defgroup CRC_Default_Polynomial_Value    Default CRC generating polynomial
     * @{
     */
    #define DEFAULT_CRC32_POLY      0x04C11DB7U  /*!<  X^32 + X^26 + X^23 + X^22 + X^16 + X^12 + X^11 + X^10 +X^8 + X^7 + X^5 + X^4 + X^2+ X +1 */
    /**
     * @}
     */

    /** @defgroup CRC_Default_InitValue    Default CRC computation initialization value
     * @{
     */
    #define DEFAULT_CRC_INITVALUE   0xFFFFFFFFU  /*!< Initial CRC default value */
    /**
     * @}
     */

    /** @defgroup CRC_Default_Polynomial    Indicates whether or not default polynomial is used
     * @{
     */
    #define DEFAULT_POLYNOMIAL_ENABLE       ((uint8_t)0x00U)  /*!< Enable default generating polynomial 0x04C11DB7  */
    #define DEFAULT_POLYNOMIAL_DISABLE      ((uint8_t)0x01U)  /*!< Disable default generating polynomial 0x04C11DB7 */
    /**
     * @}
     */

    /** @defgroup CRC_Default_InitValue_Use    Indicates whether or not default init value is used
     * @{
     */
    #define DEFAULT_INIT_VALUE_ENABLE      ((uint8_t)0x00U) /*!< Enable initial CRC default value  */
    #define DEFAULT_INIT_VALUE_DISABLE     ((uint8_t)0x01U) /*!< Disable initial CRC default value */
    /**
     * @}
     */

    /** @defgroup CRC_Polynomial_Sizes Polynomial sizes to configure the peripheral
     * @{
     */
    #define CRC_POLYLENGTH_32B                  0x00000000U        /*!< Resort to a 32-bit long generating polynomial */
    #define CRC_POLYLENGTH_16B                  CRC_CR_POLYSIZE_0  /*!< Resort to a 16-bit long generating polynomial */
    #define CRC_POLYLENGTH_8B                   CRC_CR_POLYSIZE_1  /*!< Resort to a 8-bit long generating polynomial  */
    #define CRC_POLYLENGTH_7B                   CRC_CR_POLYSIZE    /*!< Resort to a 7-bit long generating polynomial  */
    /**
     * @}
     */

    /** @defgroup CRC_Polynomial_Size_Definitions CRC polynomial possible sizes actual definitions
     * @{
     */
    #define HAL_CRC_LENGTH_32B     32U          /*!< 32-bit long CRC */
    #define HAL_CRC_LENGTH_16B     16U          /*!< 16-bit long CRC */
    #define HAL_CRC_LENGTH_8B       8U          /*!< 8-bit long CRC  */
    #define HAL_CRC_LENGTH_7B       7U          /*!< 7-bit long CRC  */
    /**
     * @}
     */

    /** @defgroup CRC_Input_Buffer_Format Input Buffer Format
     * @{
     */
    /* WARNING: CRC_INPUT_FORMAT_UNDEFINED is created for reference purposes but
    * an error is triggered in HAL_CRC_Init() if InputDataFormat field is set
    * to CRC_INPUT_FORMAT_UNDEFINED: the format MUST be defined by the user for
    * the CRC APIs to provide a correct result */
    #define CRC_INPUTDATA_FORMAT_UNDEFINED             0x00000000U  /*!< Undefined input data format    */
    #define CRC_INPUTDATA_FORMAT_BYTES                 0x00000001U  /*!< Input data in byte format      */
    #define CRC_INPUTDATA_FORMAT_HALFWORDS             0x00000002U  /*!< Input data in half-word format */
    #define CRC_INPUTDATA_FORMAT_WORDS                 0x00000003U  /*!< Input data in word format      */
    /**
     * @}
     */

    /** @defgroup CRC_Aliases CRC API aliases
     * @{
     */
    #define HAL_CRC_Input_Data_Reverse   HAL_CRCEx_Input_Data_Reverse    /*!< Aliased to HAL_CRCEx_Input_Data_Reverse for inter STM32 series compatibility  */
    #define HAL_CRC_Output_Data_Reverse  HAL_CRCEx_Output_Data_Reverse   /*!< Aliased to HAL_CRCEx_Output_Data_Reverse for inter STM32 series compatibility */
    /**
     * @}
     */

    /**
     * @}
     */

    /* Exported macros -----------------------------------------------------------*/
    /** @defgroup CRC_Exported_Macros CRC Exported Macros
     * @{
     */

    /** @brief Reset CRC handle state.
     * @param  __HANDLE__ CRC handle.
     * @retval None
     */
    #define __HAL_CRC_RESET_HANDLE_STATE(__HANDLE__) ((__HANDLE__)->State = HAL_CRC_STATE_RESET)

    /**
     * @brief  Reset CRC Data Register.
     * @param  __HANDLE__ CRC handle
     * @retval None
     */
    #define __HAL_CRC_DR_RESET(__HANDLE__) ((__HANDLE__)->Instance->CR |= CRC_CR_RESET)

    /**
     * @brief  Set CRC INIT non-default value
     * @param  __HANDLE__ CRC handle
     * @param  __INIT__ 32-bit initial value
     * @retval None
     */
    #define __HAL_CRC_INITIALCRCVALUE_CONFIG(__HANDLE__, __INIT__) ((__HANDLE__)->Instance->INIT = (__INIT__))

    /**
     * @brief Store data in the Independent Data (ID) register.
     * @param __HANDLE__ CRC handle
     * @param __VALUE__  Value to be stored in the ID register
     * @note  Refer to the Reference Manual to get the authorized __VALUE__ length in bits
     * @retval None
     */
    #define __HAL_CRC_SET_IDR(__HANDLE__, __VALUE__) (WRITE_REG((__HANDLE__)->Instance->IDR, (__VALUE__)))

    /**
     * @brief Return the data stored in the Independent Data (ID) register.
     * @param __HANDLE__ CRC handle
     * @note  Refer to the Reference Manual to get the authorized __VALUE__ length in bits
     * @retval Value of the ID register
     */
    #define __HAL_CRC_GET_IDR(__HANDLE__) (((__HANDLE__)->Instance->IDR) & CRC_IDR_IDR)
    /**
     * @}
     */


    /* Private macros --------------------------------------------------------*/
    /** @defgroup  CRC_Private_Macros CRC Private Macros
     * @{
     */

    #define IS_DEFAULT_POLYNOMIAL(DEFAULT) (((DEFAULT) == DEFAULT_POLYNOMIAL_ENABLE) || \
                                            ((DEFAULT) == DEFAULT_POLYNOMIAL_DISABLE))


    #define IS_DEFAULT_INIT_VALUE(VALUE)  (((VALUE) == DEFAULT_INIT_VALUE_ENABLE) || \
                                        ((VALUE) == DEFAULT_INIT_VALUE_DISABLE))

    #define IS_CRC_POL_LENGTH(LENGTH)     (((LENGTH) == CRC_POLYLENGTH_32B) || \
                                        ((LENGTH) == CRC_POLYLENGTH_16B) || \
                                        ((LENGTH) == CRC_POLYLENGTH_8B)  || \
                                        ((LENGTH) == CRC_POLYLENGTH_7B))

    #define IS_CRC_INPUTDATA_FORMAT(FORMAT)           (((FORMAT) == CRC_INPUTDATA_FORMAT_BYTES)     || \
                                                    ((FORMAT) == CRC_INPUTDATA_FORMAT_HALFWORDS) || \
                                                    ((FORMAT) == CRC_INPUTDATA_FORMAT_WORDS))


    /*************************************** from crc_ex *******************************************************/
    /** @defgroup CRCEx_Input_Data_Inversion Input Data Inversion Modes
     * @{
     */
    #define CRC_INPUTDATA_INVERSION_NONE               0x00000000U     /*!< No input data inversion            */
    #define CRC_INPUTDATA_INVERSION_BYTE               CRC_CR_REV_IN_0 /*!< Byte-wise input data inversion     */
    #define CRC_INPUTDATA_INVERSION_HALFWORD           CRC_CR_REV_IN_1 /*!< HalfWord-wise input data inversion */
    #define CRC_INPUTDATA_INVERSION_WORD               CRC_CR_REV_IN   /*!< Word-wise input data inversion     */
    /**
     * @}
     */

    /** @defgroup CRCEx_Output_Data_Inversion Output Data Inversion Modes
     * @{
     */
    #define CRC_OUTPUTDATA_INVERSION_DISABLE         0x00000000U       /*!< No output data inversion       */
    #define CRC_OUTPUTDATA_INVERSION_ENABLE          CRC_CR_REV_OUT    /*!< Bit-wise output data inversion */
    /**
     * @}
     */

    /**
     * @}
     */

    /* Exported macro ------------------------------------------------------------*/
    /** @defgroup CRCEx_Exported_Macros CRC Extended Exported Macros
     * @{
     */

    /**
     * @brief  Set CRC output reversal
     * @param  __HANDLE__ CRC handle
     * @retval None
     */
    #define  __HAL_CRC_OUTPUTREVERSAL_ENABLE(__HANDLE__) ((__HANDLE__)->Instance->CR |= CRC_CR_REV_OUT)

    /**
     * @brief  Unset CRC output reversal
     * @param  __HANDLE__ CRC handle
     * @retval None
     */
    #define __HAL_CRC_OUTPUTREVERSAL_DISABLE(__HANDLE__) ((__HANDLE__)->Instance->CR &= ~(CRC_CR_REV_OUT))

    /**
     * @brief  Set CRC non-default polynomial
     * @param  __HANDLE__ CRC handle
     * @param  __POLYNOMIAL__ 7, 8, 16 or 32-bit polynomial
     * @retval None
     */
    #define __HAL_CRC_POLYNOMIAL_CONFIG(__HANDLE__, __POLYNOMIAL__) ((__HANDLE__)->Instance->POL = (__POLYNOMIAL__))

    /**
     * @}
     */

    /* Private macros --------------------------------------------------------*/
    /** @defgroup CRCEx_Private_Macros CRC Extended Private Macros
     * @{
     */

    #define IS_CRC_INPUTDATA_INVERSION_MODE(MODE)     (((MODE) == CRC_INPUTDATA_INVERSION_NONE)     || \
                                                    ((MODE) == CRC_INPUTDATA_INVERSION_BYTE)     || \
                                                    ((MODE) == CRC_INPUTDATA_INVERSION_HALFWORD) || \
                                                    ((MODE) == CRC_INPUTDATA_INVERSION_WORD))

    #define IS_CRC_OUTPUTDATA_INVERSION_MODE(MODE)    (((MODE) == CRC_OUTPUTDATA_INVERSION_DISABLE) || \
                                                    ((MODE) == CRC_OUTPUTDATA_INVERSION_ENABLE))

    /**
     * @}
     */

    /* Exported functions --------------------------------------------------------*/

    /** @addtogroup CRCEx_Exported_Functions
     * @{
     */

    /** @addtogroup CRCEx_Exported_Functions_Group1
     * @{
     */
    /* Initialization and de-initialization functions  ****************************/
    HAL_CRCStatusTypeDef HAL_CRCEx_Polynomial_Set(CRC_HandleTypeDef *hcrc, uint32_t Pol, uint32_t PolyLength);
    HAL_CRCStatusTypeDef HAL_CRCEx_Input_Data_Reverse(CRC_HandleTypeDef *hcrc, uint32_t InputReverseMode);
    HAL_CRCStatusTypeDef HAL_CRCEx_Output_Data_Reverse(CRC_HandleTypeDef *hcrc, uint32_t OutputReverseMode);


    /* Exported functions --------------------------------------------------------*/
    /** @defgroup CRC_Exported_Functions CRC Exported Functions
     * @{
     */
    #define __HAL_CRC_CLK_ENABLE()             do { \
                                                    __IO uint32_t tmpreg; \
                                                    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_CRCEN); \
                                                    /* Delay after an RCC peripheral clock enabling */ \
                                                    tmpreg = READ_BIT(RCC->AHB1ENR, RCC_AHB1ENR_CRCEN); \
                                                    UNUSED_CRC(tmpreg); \
                                                } while(0)

    #define __HAL_CRC_CLK_DISABLE()            CLEAR_BIT(RCC->AHB1ENR, RCC_AHB1ENR_CRCEN)

    /* Initialization and de-initialization functions  ****************************/
    /** @defgroup CRC_Exported_Functions_Group1 Initialization and de-initialization functions
     * @{
     */
    HAL_CRCStatusTypeDef HAL_CRC_Init(CRC_HandleTypeDef *hcrc);
    HAL_CRCStatusTypeDef HAL_CRC_DeInit(CRC_HandleTypeDef *hcrc);
    void HAL_CRC_MspInit(CRC_HandleTypeDef *hcrc);
    void HAL_CRC_MspDeInit(CRC_HandleTypeDef *hcrc);
    /**
     * @}
     */

    /* Peripheral Control functions ***********************************************/
    /** @defgroup CRC_Exported_Functions_Group2 Peripheral Control functions
     * @{
     */
    uint32_t HAL_CRC_Accumulate(CRC_HandleTypeDef *hcrc, uint32_t pBuffer[], uint32_t BufferLength);
    uint32_t HAL_CRC_Calculate(CRC_HandleTypeDef *hcrc, uint32_t pBuffer[], uint32_t BufferLength);
    /**
     * @}
     */

    /* Peripheral State and Error functions ***************************************/
    /** @defgroup CRC_Exported_Functions_Group3 Peripheral State functions
     * @{
     */
    HAL_CRC_StateTypeDef HAL_CRC_GetState(CRC_HandleTypeDef *hcrc);


    /*
    ===============================================================================
                        ##### How to use this driver #####
    ===============================================================================
        [..]
            (+) Enable CRC AHB clock using __HAL_RCC_CRC_CLK_ENABLE();
            (+) Initialize CRC calculator
                (++) specify generating polynomial (peripheral default or non-default one)
                (++) specify initialization value (peripheral default or non-default one)
                (++) specify input data format
                (++) specify input or output data inversion mode if any
            (+) Use HAL_CRC_Accumulate() function to compute the CRC value of the
                input data buffer starting with the previously computed CRC as
                initialization value
            (+) Use HAL_CRC_Calculate() function to compute the CRC value of the
                input data buffer starting with the defined initialization value
                (default or non-default) to initiate CRC calculation

    @endverbatim
    ******************************************************************************
    * @attention
    *
    * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
    * All rights reserved.</center></h2>
    *
    * This software component is licensed by ST under BSD 3-Clause license,
    * the "License"; You may not use this file except in compliance with the
    * License. You may obtain a copy of the License at:
    *                        opensource.org/licenses/BSD-3-Clause
    *
    ******************************************************************************
    */

    /* Includes ------------------------------------------------------------------*/

    // #include "stm32l4_crc.h"

    /** @addtogroup STM32L4xx_HAL_Driver
     * @{
     */

    /** @defgroup CRC CRC
     * @brief CRC HAL module driver.
     * @{
     */


    /* Private typedef -----------------------------------------------------------*/
    /* Private define ------------------------------------------------------------*/
    /* Private macro -------------------------------------------------------------*/
    /* Private variables ---------------------------------------------------------*/
    /* Private function prototypes -----------------------------------------------*/
    /** @defgroup CRC_Private_Functions CRC Private Functions
     * @{
     */
    static uint32_t CRC_Handle_8(CRC_HandleTypeDef *hcrc, uint8_t pBuffer[], uint32_t BufferLength);
    static uint32_t CRC_Handle_16(CRC_HandleTypeDef *hcrc, uint16_t pBuffer[], uint32_t BufferLength);
    /**
     * @}
     */

    /* Exported functions --------------------------------------------------------*/

    /** @defgroup CRC_Exported_Functions CRC Exported Functions
     * @{
     */

    /** @defgroup CRC_Exported_Functions_Group1 Initialization and de-initialization functions
     *  @brief    Initialization and Configuration functions.
     *
    @verbatim
    ===============================================================================
                ##### Initialization and de-initialization functions #####
    ===============================================================================
        [..]  This section provides functions allowing to:
        (+) Initialize the CRC according to the specified parameters
            in the CRC_InitTypeDef and create the associated handle
        (+) DeInitialize the CRC peripheral
        (+) Initialize the CRC MSP (MCU Specific Package)
        (+) DeInitialize the CRC MSP

    @endverbatim
    * @{
    */

    /**
     * @brief  Initialize the CRC according to the specified
     *         parameters in the CRC_InitTypeDef and create the associated handle.
     * @param  hcrc CRC handle
     * @retval HAL status
     */
    HAL_CRCStatusTypeDef HAL_CRC_Init(CRC_HandleTypeDef *hcrc)
    {
    /* Check the CRC handle allocation */
    if (hcrc == NULL)
    {
        return HAL_CRC_ERROR;
    }

    /* Check the parameters */
    assert_param_crc(IS_CRC_ALL_INSTANCE(hcrc->Instance));

    if (hcrc->State == HAL_CRC_STATE_RESET)
    {
        /* Allocate lock resource and initialize it */
        hcrc->Lock = HAL_CRC_UNLOCKED;
        /* Init the low level hardware */
        HAL_CRC_MspInit(hcrc);
    }

    hcrc->State = HAL_CRC_STATE_BUSY;

    /* check whether or not non-default generating polynomial has been
    * picked up by user */
    assert_param_crc(IS_DEFAULT_POLYNOMIAL(hcrc->Init.DefaultPolynomialUse));
    if (hcrc->Init.DefaultPolynomialUse == DEFAULT_POLYNOMIAL_ENABLE)
    {
        /* initialize peripheral with default generating polynomial */
        WRITE_REG(hcrc->Instance->POL, DEFAULT_CRC32_POLY);
        MODIFY_REG(hcrc->Instance->CR, CRC_CR_POLYSIZE, CRC_POLYLENGTH_32B);
    }
    else
    {
        /* initialize CRC peripheral with generating polynomial defined by user */
        if (HAL_CRCEx_Polynomial_Set(hcrc, hcrc->Init.GeneratingPolynomial, hcrc->Init.CRCLength) != HAL_CRC_OK)
        {
        return HAL_CRC_ERROR;
        }
    }

    /* check whether or not non-default CRC initial value has been
    * picked up by user */
    assert_param_crc(IS_DEFAULT_INIT_VALUE(hcrc->Init.DefaultInitValueUse));
    if (hcrc->Init.DefaultInitValueUse == DEFAULT_INIT_VALUE_ENABLE)
    {
        WRITE_REG(hcrc->Instance->INIT, DEFAULT_CRC_INITVALUE);
    }
    else
    {
        WRITE_REG(hcrc->Instance->INIT, hcrc->Init.InitValue);
    }


    /* set input data inversion mode */
    assert_param_crc(IS_CRC_INPUTDATA_INVERSION_MODE(hcrc->Init.InputDataInversionMode));
    MODIFY_REG(hcrc->Instance->CR, CRC_CR_REV_IN, hcrc->Init.InputDataInversionMode);

    /* set output data inversion mode */
    assert_param_crc(IS_CRC_OUTPUTDATA_INVERSION_MODE(hcrc->Init.OutputDataInversionMode));
    MODIFY_REG(hcrc->Instance->CR, CRC_CR_REV_OUT, hcrc->Init.OutputDataInversionMode);

    /* makes sure the input data format (bytes, halfwords or words stream)
    * is properly specified by user */
    assert_param_crc(IS_CRC_INPUTDATA_FORMAT(hcrc->InputDataFormat));

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_READY;

    /* Return function status */
    return HAL_CRC_OK;
    }

    /**
     * @brief  DeInitialize the CRC peripheral.
     * @param  hcrc CRC handle
     * @retval HAL status
     */
    HAL_CRCStatusTypeDef HAL_CRC_DeInit(CRC_HandleTypeDef *hcrc)
    {
    /* Check the CRC handle allocation */
    if (hcrc == NULL)
    {
        return HAL_CRC_ERROR;
    }

    /* Check the parameters */
    assert_param_crc(IS_CRC_ALL_INSTANCE(hcrc->Instance));

    /* Check the CRC peripheral state */
    if (hcrc->State == HAL_CRC_STATE_BUSY)
    {
        return HAL_CRC_BUSY;
    }

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_BUSY;

    /* Reset CRC calculation unit */
    __HAL_CRC_DR_RESET(hcrc);

    /* Reset IDR register content */
    CLEAR_BIT(hcrc->Instance->IDR, CRC_IDR_IDR);

    /* DeInit the low level hardware */
    HAL_CRC_MspDeInit(hcrc);

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_RESET;

    /* Process unlocked */
    __HAL_CRC_UNLOCK(hcrc);

    /* Return function status */
    return HAL_CRC_OK;
    }

    /**
     * @brief  Initializes the CRC MSP.
     * @param  hcrc CRC handle
     * @retval None
     */

    void HAL_CRC_MspInit(CRC_HandleTypeDef* hcrc)
    {
    if(hcrc->Instance==CRC)
    {
        __HAL_CRC_CLK_ENABLE();

    }

    }



    /**
     * @brief  DeInitialize the CRC MSP.
     * @param  hcrc CRC handle
     * @retval None
     */

    void HAL_CRC_MspDeInit(CRC_HandleTypeDef* hcrc)
    {
    if(hcrc->Instance==CRC)
    {
        __HAL_CRC_CLK_DISABLE();

    }

    }

    /**
     * @}
     */

    /** @defgroup CRC_Exported_Functions_Group2 Peripheral Control functions
     *  @brief    management functions.
     *
    @verbatim
    ===============================================================================
                        ##### Peripheral Control functions #####
    ===============================================================================
        [..]  This section provides functions allowing to:
        (+) compute the 7, 8, 16 or 32-bit CRC value of an 8, 16 or 32-bit data buffer
            using combination of the previous CRC value and the new one.

        [..]  or

        (+) compute the 7, 8, 16 or 32-bit CRC value of an 8, 16 or 32-bit data buffer
            independently of the previous CRC value.

    @endverbatim
    * @{
    */

    /**
     * @brief  Compute the 7, 8, 16 or 32-bit CRC value of an 8, 16 or 32-bit data buffer
     *         starting with the previously computed CRC as initialization value.
     * @param  hcrc CRC handle
     * @param  pBuffer pointer to the input data buffer, exact input data format is
     *         provided by hcrc->InputDataFormat.
     * @param  BufferLength input data buffer length (number of bytes if pBuffer
     *         type is * uint8_t, number of half-words if pBuffer type is * uint16_t,
     *         number of words if pBuffer type is * uint32_t).
     * @note  By default, the API expects a uint32_t pointer as input buffer parameter.
     *        Input buffer pointers with other types simply need to be cast in uint32_t
     *        and the API will internally adjust its input data processing based on the
     *        handle field hcrc->InputDataFormat.
     * @retval uint32_t CRC (returned value LSBs for CRC shorter than 32 bits)
     */
    uint32_t HAL_CRC_Accumulate(CRC_HandleTypeDef *hcrc, uint32_t pBuffer[], uint32_t BufferLength)
    {
    uint32_t index;      /* CRC input data buffer index */
    uint32_t temp = 0U;  /* CRC output (read from hcrc->Instance->DR register) */

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_BUSY;

    switch (hcrc->InputDataFormat)
    {
        case CRC_INPUTDATA_FORMAT_WORDS:
        /* Enter Data to the CRC calculator */
        for (index = 0U; index < BufferLength; index++)
        {
            hcrc->Instance->DR = pBuffer[index];
        }
        temp = hcrc->Instance->DR;
        break;

        case CRC_INPUTDATA_FORMAT_BYTES:
        temp = CRC_Handle_8(hcrc, (uint8_t *)pBuffer, BufferLength);
        break;

        case CRC_INPUTDATA_FORMAT_HALFWORDS:
        temp = CRC_Handle_16(hcrc, (uint16_t *)(void *)pBuffer, BufferLength);    /* Derogation MisraC2012 R.11.5 */
        break;
        default:
        break;
    }

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_READY;

    /* Return the CRC computed value */
    return temp;
    }

    /**
     * @brief  Compute the 7, 8, 16 or 32-bit CRC value of an 8, 16 or 32-bit data buffer
     *         starting with hcrc->Instance->INIT as initialization value.
     * @param  hcrc CRC handle
     * @param  pBuffer pointer to the input data buffer, exact input data format is
     *         provided by hcrc->InputDataFormat.
     * @param  BufferLength input data buffer length (number of bytes if pBuffer
     *         type is * uint8_t, number of half-words if pBuffer type is * uint16_t,
     *         number of words if pBuffer type is * uint32_t).
     * @note  By default, the API expects a uint32_t pointer as input buffer parameter.
     *        Input buffer pointers with other types simply need to be cast in uint32_t
     *        and the API will internally adjust its input data processing based on the
     *        handle field hcrc->InputDataFormat.
     * @retval uint32_t CRC (returned value LSBs for CRC shorter than 32 bits)
     */
    uint32_t HAL_CRC_Calculate(CRC_HandleTypeDef *hcrc, uint32_t pBuffer[], uint32_t BufferLength)
    {
    uint32_t index;      /* CRC input data buffer index */
    uint32_t temp = 0U;  /* CRC output (read from hcrc->Instance->DR register) */

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_BUSY;

    /* Reset CRC Calculation Unit (hcrc->Instance->INIT is
    *  written in hcrc->Instance->DR) */
    __HAL_CRC_DR_RESET(hcrc);

    switch (hcrc->InputDataFormat)
    {
        case CRC_INPUTDATA_FORMAT_WORDS:
        /* Enter 32-bit input data to the CRC calculator */
        for (index = 0U; index < BufferLength; index++)
        {
            hcrc->Instance->DR = pBuffer[index];
        }
        temp = hcrc->Instance->DR;
        break;

        case CRC_INPUTDATA_FORMAT_BYTES:
        /* Specific 8-bit input data handling  */
        temp = CRC_Handle_8(hcrc, (uint8_t *)pBuffer, BufferLength);
        break;

        case CRC_INPUTDATA_FORMAT_HALFWORDS:
        /* Specific 16-bit input data handling  */
        temp = CRC_Handle_16(hcrc, (uint16_t *)(void *)pBuffer, BufferLength);    /* Derogation MisraC2012 R.11.5 */
        break;

        default:
        break;
    }

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_READY;

    /* Return the CRC computed value */
    return temp;
    }

    /**
     * @}
     */

    /** @defgroup CRC_Exported_Functions_Group3 Peripheral State functions
     *  @brief    Peripheral State functions.
     *
    @verbatim
    ===============================================================================
                        ##### Peripheral State functions #####
    ===============================================================================
        [..]
        This subsection permits to get in run-time the status of the peripheral.

    @endverbatim
    * @{
    */

    /**
     * @brief  Return the CRC handle state.
     * @param  hcrc CRC handle
     * @retval HAL state
     */
    HAL_CRC_StateTypeDef HAL_CRC_GetState(CRC_HandleTypeDef *hcrc)
    {
    /* Return CRC handle state */
    return hcrc->State;
    }

    /**
     * @}
     */

    /**
     * @}
     */

    /** @addtogroup CRC_Private_Functions
     * @{
     */

    /**
     * @brief  Enter 8-bit input data to the CRC calculator.
     *         Specific data handling to optimize processing time.
     * @param  hcrc CRC handle
     * @param  pBuffer pointer to the input data buffer
     * @param  BufferLength input data buffer length
     * @retval uint32_t CRC (returned value LSBs for CRC shorter than 32 bits)
     */
    static uint32_t CRC_Handle_8(CRC_HandleTypeDef *hcrc, uint8_t pBuffer[], uint32_t BufferLength)
    {
    uint32_t i; /* input data buffer index */
    // uint16_t data;
    // __IO uint16_t *pReg;

    /* Processing time optimization: 4 bytes are entered in a row with a single word write,
    * last bytes must be carefully fed to the CRC calculator to ensure a correct type
    * handling by the peripheral */
    for (i = 0U; i < (BufferLength / 4U); i++)
    {
        hcrc->Instance->DR = ((uint32_t)pBuffer[4U * i] << 24U) | \
                            ((uint32_t)pBuffer[(4U * i) + 1U] << 16U) | \
                            ((uint32_t)pBuffer[(4U * i) + 2U] << 8U)  | \
                            (uint32_t)pBuffer[(4U * i) + 3U];
    }
    /* last bytes specific handling */
    if ((BufferLength % 4U) != 0U)
    {
        if ((BufferLength % 4U) == 1U)
        {
        hcrc->Instance->DR =  (uint32_t)pBuffer[4U * i] ;  // << 24
        // *(__IO uint8_t *)(__IO void *)(&hcrc->Instance->DR) = pBuffer[4U * i];         /* Derogation MisraC2012 R.11.5 */

        }
        if ((BufferLength % 4U) == 2U)
        {
        // data = ((uint16_t)(pBuffer[4U * i]) << 8U) | (uint16_t)pBuffer[(4U * i) + 1U];
        // pReg = (__IO uint16_t *)(__IO void *)(&hcrc->Instance->DR);                    /* Derogation MisraC2012 R.11.5 */
        // *pReg = data;
        hcrc->Instance->DR = ( (uint32_t)(pBuffer[4U * i] << 8) | (uint32_t)(pBuffer[(4U * i) + 1U]) );
        }
        if ((BufferLength % 4U) == 3U)
        {
        // data = ((uint16_t)(pBuffer[4U * i]) << 8U) | (uint16_t)pBuffer[(4U * i) + 1U];
        // pReg = (__IO uint16_t *)(__IO void *)(&hcrc->Instance->DR);                    /* Derogation MisraC2012 R.11.5 */
        // *pReg = data;

        // *(__IO uint8_t *)(__IO void *)(&hcrc->Instance->DR) = pBuffer[(4U * i) + 2U];  /* Derogation MisraC2012 R.11.5 */
            hcrc->Instance->DR =  ( (uint32_t)(pBuffer[4U * i] << 16) | (uint32_t)(pBuffer[(4U * i) + 1U] << 8) | (uint32_t)(pBuffer[(4U * i) + 2U] ) );
        }
    }

    /* Return the CRC computed value */
    return hcrc->Instance->DR;
    }

    /**
     * @brief  Enter 16-bit input data to the CRC calculator.
     *         Specific data handling to optimize processing time.
     * @param  hcrc CRC handle
     * @param  pBuffer pointer to the input data buffer
     * @param  BufferLength input data buffer length
     * @retval uint32_t CRC (returned value LSBs for CRC shorter than 32 bits)
     */
    static uint32_t CRC_Handle_16(CRC_HandleTypeDef *hcrc, uint16_t pBuffer[], uint32_t BufferLength)
    {
    uint32_t i;  /* input data buffer index */
    __IO uint16_t *pReg;

    /* Processing time optimization: 2 HalfWords are entered in a row with a single word write,
    * in case of odd length, last HalfWord must be carefully fed to the CRC calculator to ensure
    * a correct type handling by the peripheral */
    for (i = 0U; i < (BufferLength / 2U); i++)
    {
        hcrc->Instance->DR = ((uint32_t)pBuffer[2U * i] << 16U) | (uint32_t)pBuffer[(2U * i) + 1U];
    }
    if ((BufferLength % 2U) != 0U)
    {
        pReg = (__IO uint16_t *)(__IO void *)(&hcrc->Instance->DR);                 /* Derogation MisraC2012 R.11.5 */
        *pReg = pBuffer[2U * i];
        // hcrc->Instance->DR = ((uint32_t)pBuffer[2U * i] << 16U) | (uint32_t)pBuffer[(2U * i) + 1U];
    }

    /* Return the CRC computed value */
    return hcrc->Instance->DR;
    }




    /**
     * @brief  Initialize the CRC polynomial if different from default one.
     * @param  hcrc CRC handle
     * @param  Pol CRC generating polynomial (7, 8, 16 or 32-bit long).
     *         This parameter is written in normal representation, e.g.
     *         @arg for a polynomial of degree 7, X^7 + X^6 + X^5 + X^2 + 1 is written 0x65
     *         @arg for a polynomial of degree 16, X^16 + X^12 + X^5 + 1 is written 0x1021
     * @param  PolyLength CRC polynomial length.
     *         This parameter can be one of the following values:
     *          @arg @ref CRC_POLYLENGTH_7B  7-bit long CRC (generating polynomial of degree 7)
     *          @arg @ref CRC_POLYLENGTH_8B  8-bit long CRC (generating polynomial of degree 8)
     *          @arg @ref CRC_POLYLENGTH_16B 16-bit long CRC (generating polynomial of degree 16)
     *          @arg @ref CRC_POLYLENGTH_32B 32-bit long CRC (generating polynomial of degree 32)
     * @retval HAL status
     */
    HAL_CRCStatusTypeDef HAL_CRCEx_Polynomial_Set(CRC_HandleTypeDef *hcrc, uint32_t Pol, uint32_t PolyLength)
    {
    HAL_CRCStatusTypeDef status = HAL_CRC_OK;
    uint32_t msb = 31U; /* polynomial degree is 32 at most, so msb is initialized to max value */

    /* Check the parameters */
    assert_param_crc(IS_CRC_POL_LENGTH(PolyLength));

    /* check polynomial definition vs polynomial size:
    * polynomial length must be aligned with polynomial
    * definition. HAL_ERROR is reported if Pol degree is
    * larger than that indicated by PolyLength.
    * Look for MSB position: msb will contain the degree of
    *  the second to the largest polynomial member. E.g., for
    *  X^7 + X^6 + X^5 + X^2 + 1, msb = 6. */
    while ((msb-- > 0U) && ((Pol & ((uint32_t)(0x1U) << (msb & 0x1FU))) == 0U))
    {
    }

    switch (PolyLength)
    {
        case CRC_POLYLENGTH_7B:
        if (msb >= HAL_CRC_LENGTH_7B)
        {
            status =   HAL_CRC_ERROR;
        }
        break;
        case CRC_POLYLENGTH_8B:
        if (msb >= HAL_CRC_LENGTH_8B)
        {
            status =   HAL_CRC_ERROR;
        }
        break;
        case CRC_POLYLENGTH_16B:
        if (msb >= HAL_CRC_LENGTH_16B)
        {
            status =   HAL_CRC_ERROR;
        }
        break;

        case CRC_POLYLENGTH_32B:
        /* no polynomial definition vs. polynomial length issue possible */
        break;
        default:
        status =  HAL_CRC_ERROR;
        break;
    }
    if (status == HAL_CRC_OK)
    {
        /* set generating polynomial */
        WRITE_REG(hcrc->Instance->POL, Pol);

        /* set generating polynomial size */
        MODIFY_REG(hcrc->Instance->CR, CRC_CR_POLYSIZE, PolyLength);
    }
    /* Return function status */
    return status;
    }

    /**
     * @brief  Set the Reverse Input data mode.
     * @param  hcrc CRC handle
     * @param  InputReverseMode Input Data inversion mode.
     *         This parameter can be one of the following values:
     *          @arg @ref CRC_INPUTDATA_INVERSION_NONE     no change in bit order (default value)
     *          @arg @ref CRC_INPUTDATA_INVERSION_BYTE     Byte-wise bit reversal
     *          @arg @ref CRC_INPUTDATA_INVERSION_HALFWORD HalfWord-wise bit reversal
     *          @arg @ref CRC_INPUTDATA_INVERSION_WORD     Word-wise bit reversal
     * @retval HAL status
     */
    HAL_CRCStatusTypeDef HAL_CRCEx_Input_Data_Reverse(CRC_HandleTypeDef *hcrc, uint32_t InputReverseMode)
    {
    /* Check the parameters */
    assert_param_crc(IS_CRC_INPUTDATA_INVERSION_MODE(InputReverseMode));

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_BUSY;

    /* set input data inversion mode */
    MODIFY_REG(hcrc->Instance->CR, CRC_CR_REV_IN, InputReverseMode);
    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_READY;

    /* Return function status */
    return HAL_CRC_OK;
    }

    /**
     * @brief  Set the Reverse Output data mode.
     * @param  hcrc CRC handle
     * @param  OutputReverseMode Output Data inversion mode.
     *         This parameter can be one of the following values:
     *          @arg @ref CRC_OUTPUTDATA_INVERSION_DISABLE no CRC inversion (default value)
     *          @arg @ref CRC_OUTPUTDATA_INVERSION_ENABLE  bit-level inversion (e.g. for a 8-bit CRC: 0xB5 becomes 0xAD)
     * @retval HAL status
     */
    HAL_CRCStatusTypeDef HAL_CRCEx_Output_Data_Reverse(CRC_HandleTypeDef *hcrc, uint32_t OutputReverseMode)
    {
    /* Check the parameters */
    assert_param_crc(IS_CRC_OUTPUTDATA_INVERSION_MODE(OutputReverseMode));

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_BUSY;

    /* set output data inversion mode */
    MODIFY_REG(hcrc->Instance->CR, CRC_CR_REV_OUT, OutputReverseMode);

    /* Change CRC peripheral state */
    hcrc->State = HAL_CRC_STATE_READY;

    /* Return function status */
    return HAL_CRC_OK;
    }

CRC_HandleTypeDef stm32l4_crc;

void ProffieBoard_InitCRC()
{
  stm32l4_crc.Instance = CRC;
  stm32l4_crc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  stm32l4_crc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  stm32l4_crc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  stm32l4_crc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  stm32l4_crc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&stm32l4_crc) != HAL_CRC_OK)
  {
	  // manage error 
  }

}

#endif
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

#endif