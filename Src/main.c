/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_ts.h"
#include "stm32746g_discovery_qspi.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* the delay before the screensaver is activated */
#define SCREENSAVER_DELAY 30000
#define REFRESH_DISP() \
    do { \
        BSP_LCD_SelectLayer(1); \
        BSP_LCD_Clear(0x00000000); \
        BSP_LCD_FillRect(395, channelVolume[channelSelect], 45, 20); \
        BSP_LCD_FillRect(20, left_slider, 45, 20); \
        BSP_LCD_FillRect(135, right_slider, 45, 20);\
        } while(0)
#define SET_LEFT(x) \
    do { \
        channelVolume[0] = x; \
        channelVolume[2] = x; \
        channelVolume[4] = x; \
        channelVolume[6] = x; \
    } while(0)
#define SET_RIGHT(x) \
    do {         \
        channelVolume[1] = x; \
        channelVolume[3] = x; \
        channelVolume[5] = x;             \
        channelVolume[7] = x; \
    } while(0)


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

DMA2D_HandleTypeDef hdma2d;

I2C_HandleTypeDef hi2c1;

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_tx;

LTDC_HandleTypeDef hltdc;

QSPI_HandleTypeDef hqspi;

SAI_HandleTypeDef hsai_BlockB1;
DMA_HandleTypeDef hdma_sai1_b;

UART_HandleTypeDef huart1;

SDRAM_HandleTypeDef hsdram1;

/* USER CODE BEGIN PV */

// The buffer for the audio data
uint16_t dataToSend[200] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 123, 123};
// tick for touchscreen
uint32_t ulSysTick_Old_Time = 0;
// reset button
uint8_t ucResetButton;
//left slider
uint16_t left_slider = 100;
//right slider
uint16_t right_slider = 100;
//touch delay
uint32_t touch_delay = 1000;
//volume control
uint8_t volumeControl[16];
//channel select
uint8_t channelSelect = 2;
//channel volume
uint8_t channelVolume[8] = {20, 40, 60, 80, 100, 120, 140, 160};
//the flag for the channel lock
uint8_t lock = 0;
/* USER CODE END PV */


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_LTDC_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2S2_Init(void);
static void MX_DMA2D_Init(void);
static void MX_QUADSPI_Init(void);
static void MX_FMC_Init(void);
static void MX_SAI1_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*This function will send the characters from printf via UART1.*/
/*Modified so you don't have to type \r\n anymore, just type \n.*/
int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        if (ptr[i] == '\n') {
            HAL_UART_Transmit(&huart1, (uint8_t *) "\r", 1, HAL_MAX_DELAY);
        }
        HAL_UART_Transmit(&huart1, (uint8_t *) &ptr[i], 1, HAL_MAX_DELAY);
    }
    return len;
}


/*This function will check if the screensaver needs to be on or not.*/
/*It will disable the screen if no fingers are detected after SCREENSAVER_DELAY*/
void checkScreensaver(void) {
    static uint32_t ScreensaverStart = SCREENSAVER_DELAY + 100;
    static uint8_t screensaver_status = 0;
    if (ScreensaverStart < HAL_GetTick() && screensaver_status == 0) {
        //Screen saver on -> display Off
        HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_RESET);
        screensaver_status = 1;
        printf("Screensaver: on\n");
    }
    TS_StateTypeDef TS_State;
    BSP_TS_GetState(&TS_State);
    if (TS_State.touchDetected > 0) {
        //new start value
        ScreensaverStart = HAL_GetTick() + SCREENSAVER_DELAY;
        if (screensaver_status == 1) {
            HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_SET);
            HAL_Delay(100);
            HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_SET);
            screensaver_status = 0;
            printf("Screensaver: off\n");
        }
    }
}

/*the function to read the touch screen*/
uint8_t ReadTouch(uint8_t ucResetButton) {
    uint32_t ulSysTick_New_Time;
    TS_StateTypeDef TS_State;
    BSP_TS_GetState(&TS_State);
    if (TS_State.touchDetected == 0) {
//        printf("Touch not detected\n");
        return ucResetButton;
    }
    else {
        uint8_t usedChannel = channelVolume[channelSelect];
        while (TS_State.touchDetected == 1) {
            BSP_TS_GetState(&TS_State);
            ulSysTick_New_Time = HAL_GetTick();
            if (ulSysTick_New_Time >= (ulSysTick_Old_Time)) {
                ucResetButton = 0;
                //L/R lock
                if ((TS_State.touchX[0] <= 125) && (TS_State.touchX[0] >= 80) && (TS_State.touchY[0] <= 155) &&
                    (TS_State.touchY[0] >= 110)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("left and right channel locked\n");
                    if (lock == 0) {
                        lock = 1;
                    }
                    else {
                        lock = 0;
                    }
                    REFRESH_DISP();

                } //channel 1 select
                if ((TS_State.touchX[0] <= 315) && (TS_State.touchX[0] >= 270) && (TS_State.touchY[0] <= 85) &&
                    (TS_State.touchY[0] >= 40)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("channel 1 selected \n");
                    channelSelect = 1;
                    REFRESH_DISP();
                } //channel 2 select
                if ((TS_State.touchX[0] <= 315) && (TS_State.touchX[0] >= 270) && (TS_State.touchY[0] <= 137) &&
                    (TS_State.touchY[0] >= 92)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("channel 2 selected \n");
                    channelSelect = 2;
                    REFRESH_DISP();
                } //channel 3 select
                if ((TS_State.touchX[0] <= 315) && (TS_State.touchX[0] >= 270) && (TS_State.touchY[0] <= 189) &&
                    (TS_State.touchY[0] >= 144)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("channel 3 selected \n");
                    channelSelect = 3;
                    REFRESH_DISP();
                } //channel 4 select
                if ((TS_State.touchX[0] <= 315) && (TS_State.touchX[0] >= 270) && (TS_State.touchY[0] <= 241) &&
                    (TS_State.touchY[0] >= 196)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("channel 4 selected \n");
                    channelSelect = 4;
                    REFRESH_DISP();
                } //channel 5 select
                if ((TS_State.touchX[0] <= 385) && (TS_State.touchX[0] >= 340) && (TS_State.touchY[0] <= 85) &&
                    (TS_State.touchY[0] >= 40)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("channel 5 selected \n");
                    channelSelect = 5;
                    REFRESH_DISP();
                } //channel 6 select
                if ((TS_State.touchX[0] <= 385) && (TS_State.touchX[0] >= 340) && (TS_State.touchY[0] <= 137) &&
                    (TS_State.touchY[0] >= 92)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("channel 6 selected \n");
                    channelSelect = 6;
                    REFRESH_DISP();
                } //channel 7 select
                if ((TS_State.touchX[0] <= 385) && (TS_State.touchX[0] >= 340) && (TS_State.touchY[0] <= 189) &&
                    (TS_State.touchY[0] >= 144)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("channel 7 selected \n");
                    channelSelect = 7;
                    REFRESH_DISP();
                }  //channel 8 select
                if ((TS_State.touchX[0] <= 385) && (TS_State.touchX[0] >= 340) && (TS_State.touchY[0] <= 241) &&
                    (TS_State.touchY[0] >= 196)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("channel 8 selected \n");
                    channelSelect = 8;
                    REFRESH_DISP();
                } //left slider
                if ((TS_State.touchX[0] <= 65) && (TS_State.touchX[0] >= 20) &&
                    (TS_State.touchY[0] <= left_slider + 25) && (TS_State.touchY[0] >= left_slider - 10)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("left slider selected \n");
                    if (lock == 0) {
                        left_slider = TS_State.touchY[0];
                        SET_LEFT(left_slider);
                    }
                    if (lock == 1) {
                        left_slider = TS_State.touchY[0];
                        right_slider = TS_State.touchY[0];
                        SET_LEFT(left_slider);
                        SET_RIGHT(right_slider);
                    }
                    REFRESH_DISP();
                } //right slider
                if ((TS_State.touchX[0] <= 180) && (TS_State.touchX[0] >= 135) &&
                    (TS_State.touchY[0] <= right_slider + 25) && (TS_State.touchY[0] >= right_slider - 10)) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("right slider selected \n");
                    if (lock == 0) {
                        right_slider = TS_State.touchY[0];
                        SET_RIGHT(right_slider);
                    }
                    if (lock == 1) {
                        left_slider = TS_State.touchY[0];
                        right_slider = TS_State.touchY[0];
                        SET_LEFT(left_slider);
                        SET_RIGHT(right_slider);
                    }
                    REFRESH_DISP();
                } //individual slider
                if ((TS_State.touchX[0] <= 440) && (TS_State.touchX[0] >= 395) &&
                    (TS_State.touchY[0] <= (channelVolume[channelSelect] + 25)) &&
                    (TS_State.touchY[0] >= (channelVolume[channelSelect] - 10))) {
                    ulSysTick_Old_Time = ulSysTick_New_Time;
                    printf("individual slider selected \n");
                    channelVolume[channelSelect] = TS_State.touchY[0];
                    REFRESH_DISP();
                }
                else {
                    printf("Touch started\n");
                }
            }
            else {
                printf("Touch initialise could not be completed\n");
            }
        }
        volumeControl[channelSelect] = usedChannel;
        for (int i = 0; i < 16; ++i) {
            if (i % 2 != 0) {
                volumeControl[i] = channelVolume[i / 2];
            } else {
            }
        }
        for (int i = 0; i < sizeof(channelVolume); ++i) {
            printf("adress: %d, Channel volume: %d\n", volumeControl[i * 2], volumeControl[(i * 2) + 1]);
        }
        HAL_I2C_Master_Transmit(&hi2c1, 0x90, (uint8_t *) volumeControl, 16, HAL_MAX_DELAY);
        HAL_Delay(100);
    }
}

void refreshDisp() {
    BSP_LCD_SelectLayer(1);
    BSP_LCD_Clear(0x00000000);
    //individual slidyboy
    BSP_LCD_FillRect(395, channelVolume[channelSelect], 45, 20);
    //left slidyboy
    BSP_LCD_FillRect(20, left_slider, 45, 20);
    //right slidyboy
    BSP_LCD_FillRect(135, right_slider, 45, 20);
    HAL_Delay(touch_delay);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

/* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_LTDC_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_I2S2_Init();
  MX_USB_DEVICE_Init();
  MX_DMA2D_Init();
  MX_QUADSPI_Init();
  MX_FMC_Init();
  MX_SAI1_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();


  /* USER CODE BEGIN 2 */

  //initialing fase
    printf("Running PE2 by IEBE NIJS\n");
    printf("Audio controller v1\n");

    /* Init the SDRAM */
    BSP_QSPI_Init();
    BSP_QSPI_MemoryMappedMode();
    WRITE_REG(QUADSPI->LPTR, 0xFFF);

    /* Init the LCD */
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(1, LCD_FB_START_ADDRESS);
    BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS + (480 * 272 * 4));
    BSP_LCD_DisplayOn();
    //clear the display
    BSP_LCD_SelectLayer(0);
    BSP_LCD_Clear(LCD_COLOR_WHITE);
    BSP_LCD_SelectLayer(1);
    BSP_LCD_Clear(0x00000000);

    /* Init the touchscreen */
    BSP_TS_Init(480, 272);

    /* Backlight */
    HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port,LCD_BL_CTRL_Pin,GPIO_PIN_SET);

    /* Assert display enable LCD_DISP pin */
    HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_SET);

    /*------------------------------------------------------------------------------------------------------------------------*/

    /*draw UI*/
    //select the first layer
    BSP_LCD_SelectLayer(0);
    //set color to black
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    //left slider
    BSP_LCD_FillRect(35, 40, 15, 201);
    //right slider
    BSP_LCD_FillRect(150, 40, 15, 201);
    //individual slider
    BSP_LCD_FillRect(410, 40, 15, 201);
    //set color to red
    BSP_LCD_SetTextColor(LCD_COLOR_RED);
    //lock button
    BSP_LCD_FillRect(80, 110, 45, 45);
    //channel 1
    BSP_LCD_FillRect(270, 40, 45, 45);
    //channel 2
    BSP_LCD_FillRect(270, 92, 45, 45);
    //channel 3
    BSP_LCD_FillRect(270, 144, 45, 45);
    //channel 4
    BSP_LCD_FillRect(270, 196, 45, 45);
    //channel 5
    BSP_LCD_FillRect(340, 40, 45, 45);
    //channel 6
    BSP_LCD_FillRect(340, 92, 45, 45);
    //channel 7
    BSP_LCD_FillRect(340, 144, 45, 45);
    //channel 8
    BSP_LCD_FillRect(340, 196, 45, 45);

    //select the second layer
    BSP_LCD_SelectLayer(1);
    BSP_LCD_SetTextColor(LCD_COLOR_RED);
    //draw the sliders
    //left slidyboy
    BSP_LCD_FillRect(20, left_slider, 45, 20);
    //right slidyboy
    BSP_LCD_FillRect(135, right_slider, 45, 20);
    //individual slidyboy
    BSP_LCD_FillRect(395, channelVolume[channelSelect], 45, 20);

    /*------------------------------------------------------------------------------------------------------------------------*/

    /* Init the I2C */
    HAL_I2C_Master_Transmit(&hi2c1, 0x34, (uint8_t *) dataToSend, 400, HAL_MAX_DELAY);

    /*------------------------------------------------------------------------------------------------------------------------*/

    /* Init the SAI */
    HAL_SAI_Transmit(&hsai_BlockB1, (uint8_t *) dataToSend, 400, HAL_MAX_DELAY);
    /*------------------------------------------------------------------------------------------------------------------------*/

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      //run the screensaver
      checkScreensaver();
      //read touch input
      ucResetButton = ReadTouch(ucResetButton);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 384;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_PLLCLK, RCC_MCODIV_5);
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC|RCC_PERIPHCLK_SAI1
                              |RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV2;
  PeriphClkInitStruct.PLLSAIDivQ = 1;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_8;
  PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLLSAI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* PVD_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(PVD_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(PVD_IRQn);
  /* FLASH_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(FLASH_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(FLASH_IRQn);
  /* RCC_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(RCC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(RCC_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* I2C1_EV_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(I2C1_EV_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
  /* I2C1_ER_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(I2C1_ER_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
  /* SPI2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SPI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(SPI2_IRQn);
  /* USART1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* FMC_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(FMC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(FMC_IRQn);
  /* OTG_FS_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(OTG_FS_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
  /* FPU_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(FPU_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(FPU_IRQn);
  /* LTDC_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(LTDC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(LTDC_IRQn);
  /* LTDC_ER_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(LTDC_ER_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(LTDC_ER_IRQn);
  /* DMA2D_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2D_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2D_IRQn);
  /* QUADSPI_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(QUADSPI_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(QUADSPI_IRQn);
}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x20303E5D;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2S2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_32B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_96K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */

  /* USER CODE END I2S2_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};
  LTDC_LayerCfgTypeDef pLayerCfg1 = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 40;
  hltdc.Init.VerticalSync = 9;
  hltdc.Init.AccumulatedHBP = 53;
  hltdc.Init.AccumulatedVBP = 11;
  hltdc.Init.AccumulatedActiveW = 533;
  hltdc.Init.AccumulatedActiveH = 283;
  hltdc.Init.TotalWidth = 565;
  hltdc.Init.TotalHeigh = 285;
  hltdc.Init.Backcolor.Blue = 255;
  hltdc.Init.Backcolor.Green = 255;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 480;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 272;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_ARGB1555;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
  pLayerCfg.FBStartAdress = 0;
  pLayerCfg.ImageWidth = 480;
  pLayerCfg.ImageHeight = 272;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg1.WindowX0 = 0;
  pLayerCfg1.WindowX1 = 480;
  pLayerCfg1.WindowY0 = 0;
  pLayerCfg1.WindowY1 = 272;
  pLayerCfg1.PixelFormat = LTDC_PIXEL_FORMAT_ARGB1555;
  pLayerCfg1.Alpha = 255;
  pLayerCfg1.Alpha0 = 0;
  pLayerCfg1.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  pLayerCfg1.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
  pLayerCfg1.FBStartAdress = 0;
  pLayerCfg1.ImageWidth = 480;
  pLayerCfg1.ImageHeight = 272;
  pLayerCfg1.Backcolor.Blue = 0;
  pLayerCfg1.Backcolor.Green = 0;
  pLayerCfg1.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg1, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */

  /* USER CODE END LTDC_Init 2 */

}

/**
  * @brief QUADSPI Initialization Function
  * @param None
  * @retval None
  */
static void MX_QUADSPI_Init(void)
{

  /* USER CODE BEGIN QUADSPI_Init 0 */

  /* USER CODE END QUADSPI_Init 0 */

  /* USER CODE BEGIN QUADSPI_Init 1 */

  /* USER CODE END QUADSPI_Init 1 */
  /* QUADSPI parameter configuration*/
  hqspi.Instance = QUADSPI;
  hqspi.Init.ClockPrescaler = 255;
  hqspi.Init.FifoThreshold = 1;
  hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_NONE;
  hqspi.Init.FlashSize = 1;
  hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
  hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
  hqspi.Init.FlashID = QSPI_FLASH_ID_1;
  hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
  if (HAL_QSPI_Init(&hqspi) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN QUADSPI_Init 2 */

  /* USER CODE END QUADSPI_Init 2 */

}

/**
  * @brief SAI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SAI1_Init(void)
{

  /* USER CODE BEGIN SAI1_Init 0 */

  /* USER CODE END SAI1_Init 0 */

  /* USER CODE BEGIN SAI1_Init 1 */

  /* USER CODE END SAI1_Init 1 */
  hsai_BlockB1.Instance = SAI1_Block_B;
  hsai_BlockB1.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockB1.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockB1.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
  hsai_BlockB1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  hsai_BlockB1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_96K;
  hsai_BlockB1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  if (HAL_SAI_InitProtocol(&hsai_BlockB1, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_32BIT, 2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI1_Init 2 */

  /* USER CODE END SAI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);

}

/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM1 memory initialization sequence
  */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_1;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_DISABLE;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_DISABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 16;
  SdramTiming.ExitSelfRefreshDelay = 16;
  SdramTiming.SelfRefreshTime = 16;
  SdramTiming.RowCycleDelay = 16;
  SdramTiming.WriteRecoveryTime = 16;
  SdramTiming.RPDelay = 16;
  SdramTiming.RCDDelay = 16;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */

  /* USER CODE END FMC_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LCD_BL_CTRL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BL_CTRL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_DISP_Pin */
  GPIO_InitStruct.Pin = LCD_DISP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_DISP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Button_Pin */
  GPIO_InitStruct.Pin = Button_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Button_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    printf("I2S transmit half complete\n");
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
