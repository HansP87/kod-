/**
  ******************************************************************************
  * @file    stm32h7xx_hal_ltdc.h
  * @author  MCD Application Team
  * @brief   Header file of LTDC HAL module.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STM32H7xx_HAL_LTDC_H
#define STM32H7xx_HAL_LTDC_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal_def.h"

/** @addtogroup STM32H7xx_HAL_Driver
  * @{
  */

#if defined (LTDC)

/** @addtogroup LTDC
  * @{
  */

/* Exported types ------------------------------------------------------------*/

/** @defgroup LTDC_Exported_Types LTDC Exported Types
  * @{
  */

/**
  * @brief  LTDC Color structure definition
  */
typedef struct
{
  uint32_t Blue;   /*!< Blue value */
  uint32_t Green;  /*!< Green value */
  uint32_t Red;    /*!< Red value */
} LTDC_ColorTypeDef;

/**
  * @brief  LTDC Initialization Structure definition
  */
typedef struct
{
  uint32_t HSPolarity;      /*!< Horizontal synchronization polarity. */
  uint32_t VSPolarity;      /*!< Vertical synchronization polarity. */
  uint32_t DEPolarity;      /*!< Data enable polarity. */
  uint32_t PCPolarity;      /*!< Pixel clock polarity. */
  uint32_t HorizontalSync;  /*!< Horizontal sync width. */
  uint32_t VerticalSync;    /*!< Vertical sync height. */
  uint32_t AccumulatedHBP;  /*!< Accumulated horizontal back porch. */
  uint32_t AccumulatedVBP;  /*!< Accumulated vertical back porch. */
  uint32_t AccumulatedActiveW; /*!< Accumulated active width. */
  uint32_t AccumulatedActiveH; /*!< Accumulated active height. */
  uint32_t TotalWidth;      /*!< Total width. */
  uint32_t TotalHeigh;      /*!< Total height. */
  LTDC_ColorTypeDef Backcolor; /*!< Background color. */
} LTDC_InitTypeDef;

/**
  * @brief  LTDC Layer configuration structure definition
  */
typedef struct
{
  uint32_t WindowX0;        /*!< Window Horizontal Start Position. */
  uint32_t WindowX1;        /*!< Window Horizontal Stop Position. */
  uint32_t WindowY0;        /*!< Window Vertical Start Position. */
  uint32_t WindowY1;        /*!< Window Vertical Stop Position. */
  uint32_t PixelFormat;     /*!< Pixel format. */
  uint32_t Alpha;           /*!< Alpha value. */
  uint32_t ConstantAlpha;   /*!< Constant alpha value. */
  LTDC_ColorTypeDef DefaultColor; /*!< Default color for blending. */
  uint32_t BlendingFactor1; /*!< Blending factor 1. */
  uint32_t BlendingFactor2; /*!< Blending factor 2. */
  uint32_t FBStartAdress;   /*!< Frame buffer start address. */
  uint32_t FBLineLength;    /*!< Frame buffer line length in bytes. */
  uint32_t FBPitch;         /*!< Frame buffer pitch in bytes. */
  uint32_t FBLineNumber;    /*!< Frame buffer line number. */
  uint32_t ImageWidth;      /*!< Image width. */
  uint32_t ImageHeight;     /*!< Image height. */
} LTDC_LayerCfgTypeDef;

/**
  * @brief  HAL LTDC State structures definition
  */
typedef enum
{
  HAL_LTDC_STATE_RESET           = 0x00U,
  HAL_LTDC_STATE_READY           = 0x01U,
  HAL_LTDC_STATE_BUSY            = 0x02U,
  HAL_LTDC_STATE_TIMEOUT         = 0x03U,
  HAL_LTDC_STATE_ERROR           = 0x04U
} HAL_LTDC_StateTypeDef;

/**
  * @brief  LTDC handle Structure definition
  */
#if (USE_HAL_LTDC_REGISTER_CALLBACKS == 1)
typedef struct __LTDC_HandleTypeDef
#else
typedef struct
#endif /* USE_HAL_LTDC_REGISTER_CALLBACKS */
{
  LTDC_TypeDef                *Instance;  /*!< Register base address */
  LTDC_InitTypeDef            Init;       /*!< LTDC required parameters */
  HAL_StatusTypeDef           Status;     /*!< LTDC peripheral status */
  HAL_LockTypeDef             Lock;       /*!< LTDC locking object */
  __IO HAL_LTDC_StateTypeDef  State;      /*!< LTDC peripheral state */
#if (USE_HAL_LTDC_REGISTER_CALLBACKS == 1)
  void (* MspInitCallback)(struct __LTDC_HandleTypeDef *hltc);          /*!< LTDC Msp Init callback */
  void (* MspDeInitCallback)(struct __LTDC_HandleTypeDef *hltc);        /*!< LTDC Msp DeInit callback */
#endif /* USE_HAL_LTDC_REGISTER_CALLBACKS */
} LTDC_HandleTypeDef;

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup LTDC_Exported_Constants LTDC Exported Constants
  * @{
  */
#define LTDC_HSPOLARITY_AL                0x00000000U
#define LTDC_HSPOLARITY_AH                LTDC_GCR_HSPOL
#define LTDC_VSPOLARITY_AL                0x00000000U
#define LTDC_VSPOLARITY_AH                LTDC_GCR_VSPOL
#define LTDC_DEPOLARITY_AL                0x00000000U
#define LTDC_DEPOLARITY_AH                LTDC_GCR_DEPOL
#define LTDC_PCPOLARITY_IPC               0x00000000U
#define LTDC_PCPOLARITY_IIPC              LTDC_GCR_PCPOL

#define LTDC_PIXEL_FORMAT_ARGB8888        0x00000000U
#define LTDC_PIXEL_FORMAT_RGB888          0x00000001U
#define LTDC_PIXEL_FORMAT_RGB565          0x00000002U
#define LTDC_PIXEL_FORMAT_ARGB1555        0x00000003U
#define LTDC_PIXEL_FORMAT_ARGB4444        0x00000004U
#define LTDC_PIXEL_FORMAT_L8              0x00000005U
#define LTDC_PIXEL_FORMAT_AL44            0x00000006U
#define LTDC_PIXEL_FORMAT_AL88            0x00000007U

#define LTDC_BLENDING_FACTOR1_PAxCA       0x0600U
#define LTDC_BLENDING_FACTOR2_PAxCA       0x0006U

/**
  * @}
  */

/* Exported macro ------------------------------------------------------------*/

/**
  * @brief  Reset LTDC handle state
  * @param  __HANDLE__ LTDC handle
  * @retval None
  */
#define __HAL_LTDC_RESET_HANDLE_STATE(__HANDLE__) ((__HANDLE__)->State = HAL_LTDC_STATE_RESET)

/* Exported functions --------------------------------------------------------*/

HAL_StatusTypeDef HAL_LTDC_Init(LTDC_HandleTypeDef *hltdc);
HAL_StatusTypeDef HAL_LTDC_ConfigLayer(LTDC_HandleTypeDef *hltdc, LTDC_LayerCfgTypeDef *pLayerCfg, uint32_t LayerIndex);
void HAL_LTDC_MspInit(LTDC_HandleTypeDef *hltdc);

#if (USE_HAL_LTDC_REGISTER_CALLBACKS == 1)
HAL_StatusTypeDef HAL_LTDC_RegisterCallback(LTDC_HandleTypeDef *hltdc, HAL_LTDC_CallbackIDTypeDef CallbackID, pLTDC_CallbackTypeDef pCallback);
HAL_StatusTypeDef HAL_LTDC_UnRegisterCallback(LTDC_HandleTypeDef *hltdc, HAL_LTDC_CallbackIDTypeDef CallbackID);
#endif /* USE_HAL_LTDC_REGISTER_CALLBACKS */

/**
  * @}
  */

#endif /* defined (LTDC) */

#ifdef __cplusplus
}
#endif

#endif /* STM32H7xx_HAL_LTDC_H */
