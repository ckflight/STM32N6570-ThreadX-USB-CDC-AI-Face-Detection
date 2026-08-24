
#include "app_threadx.h"
#include "main.h"
#include "gpdma.h"
#include "ucpd.h"
#include "usb_otg.h"
#include "gpio.h"
#include "usbpd.h"

extern DMA_HandleTypeDef handle_GPDMA1_Channel2 ;
extern DMA_HandleTypeDef handle_GPDMA1_Channel3 ;

// *************Includes from nn project**********************
#include "stai.h"
#include "stai_network.h"
#include "app_postprocess.h"
#include "stm32n6570_discovery_xspi.h"
#include "app_postprocess.h"
#include "app_camerapipeline.h"
#include "stm32n6570_discovery.h"

stai_ptr nn_in;
stai_size number_output = 0;
stai_ptr nn_out[STAI_NETWORK_OUT_NUM] = {0};
STAI_NETWORK_CONTEXT_DECLARE(network_context, STAI_NETWORK_CONTEXT_SIZE)

fd_blazeface_pp_static_param_t pp_params;
fd_pp_out_t pp_output;

volatile int32_t cameraFrameReceived = 0;

static void NeuralNetwork_init(uint32_t *nn_in_length, stai_ptr *nn_out, stai_size *number_output, int32_t nn_out_len[]);
static void NPURam_enable(void);
static void NPUCache_config(void);
void NeuralNetwork_run(void);
static void set_clk_sleep_mode(void);

__attribute__((section(".psram_bss")))
__attribute__((aligned(32)))
static uint8_t camera_dummy_display_buffer[800 * 480 * 2];

// **********************************

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void SystemIsolation_Config(void);

// Default cod comes with while loop buffer copying which is so slow.
// To eliminate #define UX_DEVICE_ENDPOINT_BUFFER_OWNER      1 and #define UX_DEVICE_CLASS_CDC_ACM_ZERO_COPY
// is defined in ux_user.h
// With these paremeters no need to implement fast write since it already does that.

// #define UX_SLAVE_REQUEST_DATA_MAX_LENGTH 32768 is increased.
// DMA is enabled hpcd_USB_OTG_HS1.Init.dma_enable = ENABLE;

int main(void)
{
    uint32_t nn_in_len = 0;
    int32_t nn_out_len[STAI_NETWORK_OUT_NUM] = {0};

    HAL_PWREx_EnableVddA();
    HAL_PWREx_EnableVddIO2();
    HAL_PWREx_EnableVddIO3();
    HAL_PWREx_EnableVddIO4();
    HAL_PWREx_EnableVddIO5();

    MPU_Config();
    SCB_EnableICache();
    SCB_EnableDCache();

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_GPDMA1_Init();
    MX_UCPD1_Init();
    MX_USB1_OTG_HS_PCD_Init();

    SystemIsolation_Config();

    BSP_XSPI_RAM_Init(0);
    BSP_XSPI_RAM_EnableMemoryMappedMode(0);

    BSP_XSPI_NOR_Init_t NOR_Init;
    NOR_Init.InterfaceMode = BSP_XSPI_NOR_OPI_MODE;
    NOR_Init.TransferRate = BSP_XSPI_NOR_DTR_TRANSFER;
    BSP_XSPI_NOR_Init(0, &NOR_Init);
    BSP_XSPI_NOR_EnableMemoryMappedMode(0);

    NPURam_enable();
    NPUCache_config();
    set_clk_sleep_mode();

    NeuralNetwork_init(&nn_in_len, nn_out, &number_output, nn_out_len);

    stai_network_info info;
    int ret = stai_network_get_info(network_context, &info);
    assert(ret == STAI_SUCCESS);

    app_postprocess_init(&pp_params, &info);

    uint32_t camera_width = 0;
    uint32_t camera_height = 0;
    uint32_t pitch_nn = 0;

    CameraPipeline_Init(&camera_width, &camera_height, &pitch_nn);

    CameraPipeline_DisplayPipe_Start(
        camera_dummy_display_buffer,
        DCMIPP_MODE_CONTINUOUS
    );

    CameraPipeline_IspUpdate();

    CameraPipeline_NNPipe_Start(
        (uint8_t *)nn_in,
        DCMIPP_MODE_SNAPSHOT
    );

    USBPD_PreInitOs();
    MX_ThreadX_Init();

    while (1)
    {
    }
}

static void set_clk_sleep_mode(void)
{
    __HAL_RCC_XSPI1_CLK_SLEEP_ENABLE();
    __HAL_RCC_XSPI2_CLK_SLEEP_ENABLE();

    __HAL_RCC_NPU_CLK_SLEEP_ENABLE();
    __HAL_RCC_CACHEAXI_CLK_SLEEP_ENABLE();

    __HAL_RCC_DCMIPP_CLK_SLEEP_ENABLE();
    __HAL_RCC_CSI_CLK_SLEEP_ENABLE();

    __HAL_RCC_FLEXRAM_MEM_CLK_SLEEP_ENABLE();

    __HAL_RCC_AXISRAM1_MEM_CLK_SLEEP_ENABLE();
    __HAL_RCC_AXISRAM2_MEM_CLK_SLEEP_ENABLE();
    __HAL_RCC_AXISRAM3_MEM_CLK_SLEEP_ENABLE();
    __HAL_RCC_AXISRAM4_MEM_CLK_SLEEP_ENABLE();
    __HAL_RCC_AXISRAM5_MEM_CLK_SLEEP_ENABLE();
    __HAL_RCC_AXISRAM6_MEM_CLK_SLEEP_ENABLE();
}

void npu_cache_enable_clocks_and_reset(void)
{
    __HAL_RCC_CACHEAXIRAM_MEM_CLK_ENABLE();
    __HAL_RCC_CACHEAXI_CLK_ENABLE();

    __HAL_RCC_CACHEAXI_FORCE_RESET();
    __HAL_RCC_CACHEAXI_RELEASE_RESET();
}

void npu_cache_disable_clocks_and_reset(void)
{
    __HAL_RCC_CACHEAXIRAM_MEM_CLK_DISABLE();
    __HAL_RCC_CACHEAXI_CLK_DISABLE();

    __HAL_RCC_CACHEAXI_FORCE_RESET();
}

HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp)
{
    RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};
    HAL_StatusTypeDef ret = HAL_OK;

    RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
    RCC_PeriphCLKInitStruct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
    RCC_PeriphCLKInitStruct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL2;
    RCC_PeriphCLKInitStruct.ICSelection[RCC_IC17].ClockDivider = 3;

    ret = HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);
    if (ret)
    {
        return ret;
    }

    RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CSI;
    RCC_PeriphCLKInitStruct.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_PeriphCLKInitStruct.ICSelection[RCC_IC18].ClockDivider = 40;

    ret = HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);

    return ret;
}

static void NeuralNetwork_init(uint32_t *nn_in_length, stai_ptr *nn_out, stai_size *number_output, int32_t nn_out_len[])
{
    stai_network_info info;
    int ret;

    ret = stai_runtime_init();
    assert(ret == STAI_SUCCESS);

    ret = stai_network_init(network_context);
    assert(ret == STAI_SUCCESS);

    ret = stai_network_get_info(network_context, &info);
    assert(ret == STAI_SUCCESS);

    assert(info.n_inputs == 1);

    *number_output = STAI_NETWORK_OUT_NUM;
    *nn_in_length = info.inputs[0].size_bytes;

    ret = stai_network_get_inputs(network_context, &nn_in, (stai_size *)&info.n_inputs);
    assert(ret == STAI_SUCCESS);

    ret = stai_network_get_outputs(network_context, nn_out, number_output);
    assert(ret == STAI_SUCCESS);

    for (int i = 0; i < *number_output; i++)
    {
        nn_out_len[i] = info.outputs[i].size_bytes;
    }
}

static void NPURam_enable(void)
{
    __HAL_RCC_NPU_CLK_ENABLE();
    __HAL_RCC_NPU_FORCE_RESET();
    __HAL_RCC_NPU_RELEASE_RESET();

    __HAL_RCC_AXISRAM3_MEM_CLK_ENABLE();
    __HAL_RCC_AXISRAM4_MEM_CLK_ENABLE();
    __HAL_RCC_AXISRAM5_MEM_CLK_ENABLE();
    __HAL_RCC_AXISRAM6_MEM_CLK_ENABLE();
    __HAL_RCC_RAMCFG_CLK_ENABLE();

    RAMCFG_HandleTypeDef hramcfg = {0};

    hramcfg.Instance = RAMCFG_SRAM3_AXI;
    HAL_RAMCFG_EnableAXISRAM(&hramcfg);

    hramcfg.Instance = RAMCFG_SRAM4_AXI;
    HAL_RAMCFG_EnableAXISRAM(&hramcfg);

    hramcfg.Instance = RAMCFG_SRAM5_AXI;
    HAL_RAMCFG_EnableAXISRAM(&hramcfg);

    hramcfg.Instance = RAMCFG_SRAM6_AXI;
    HAL_RAMCFG_EnableAXISRAM(&hramcfg);
}

static void NPUCache_config(void)
{
    npu_cache_enable();
}

void NeuralNetwork_run(void)
{
    int ret;

    ret = stai_network_run(network_context, STAI_MODE_SYNC);

    assert(ret == STAI_SUCCESS);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};

    /*
     * Same power configuration as original Face Detection project.
     * Required for high frequency operation.
     */
    BSP_SMPS_Init(SMPS_VOLTAGE_OVERDRIVE);

    /*
     * Keep HSE enabled for USB:
     *
     * USB PHY = HSE / 2
     * USB OTG = HSE direct
     *
     * PLL1/2/3/4 remain identical to Face Detection.
     */
    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI |
        RCC_OSCILLATORTYPE_HSE;

    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DIGITAL;

    /*
     * PLL1 = 64 MHz * 25 / 2 = 800 MHz
     */
    RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL1.PLLM = 2;
    RCC_OscInitStruct.PLL1.PLLN = 25;
    RCC_OscInitStruct.PLL1.PLLFractional = 0;
    RCC_OscInitStruct.PLL1.PLLP1 = 1;
    RCC_OscInitStruct.PLL1.PLLP2 = 1;

    /*
     * PLL2 = 64 MHz * 125 / 8 = 1000 MHz
     * NPU clock
     */
    RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL2.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL2.PLLM = 8;
    RCC_OscInitStruct.PLL2.PLLN = 125;
    RCC_OscInitStruct.PLL2.PLLFractional = 0;
    RCC_OscInitStruct.PLL2.PLLP1 = 1;
    RCC_OscInitStruct.PLL2.PLLP2 = 1;

    /*
     * PLL3 = 900 MHz
     * AXISRAM3/4/5/6
     */
    RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL3.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL3.PLLM = 8;
    RCC_OscInitStruct.PLL3.PLLN = 225;
    RCC_OscInitStruct.PLL3.PLLFractional = 0;
    RCC_OscInitStruct.PLL3.PLLP1 = 1;
    RCC_OscInitStruct.PLL3.PLLP2 = 2;

    /*
     * PLL4 = 50 MHz
     * Keep it exactly as Face Detection because LCD will be added later.
     */
    RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL4.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL4.PLLM = 8;
    RCC_OscInitStruct.PLL4.PLLN = 225;
    RCC_OscInitStruct.PLL4.PLLFractional = 0;
    RCC_OscInitStruct.PLL4.PLLP1 = 6;
    RCC_OscInitStruct.PLL4.PLLP2 = 6;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Face Detection clock tree
     */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_CPUCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2 |
        RCC_CLOCKTYPE_PCLK4 |
        RCC_CLOCKTYPE_PCLK5;

    /* CPU = PLL1 / 1 = 800 MHz */
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
    RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC1Selection.ClockDivider = 1;

    /* AXI SYS = PLL1 / 2 = 400 MHz */
    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_IC2_IC6_IC11;

    RCC_ClkInitStruct.IC2Selection.ClockSelection =
        RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC2Selection.ClockDivider = 2;

    /* NPU = PLL2 / 1 = 1000 MHz */
    RCC_ClkInitStruct.IC6Selection.ClockSelection =
        RCC_ICCLKSOURCE_PLL2;
    RCC_ClkInitStruct.IC6Selection.ClockDivider = 1;

    /* AXISRAM = PLL3 / 1 = 900 MHz */
    RCC_ClkInitStruct.IC11Selection.ClockSelection =
        RCC_ICCLKSOURCE_PLL3;
    RCC_ClkInitStruct.IC11Selection.ClockDivider = 1;

    /* HCLK = 400 / 2 = 200 MHz */
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;

    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
    RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * XSPI clocks identical to Face Detection.
     */
    RCC_PeriphCLKInitStruct.PeriphClockSelection =
        RCC_PERIPHCLK_XSPI1 |
        RCC_PERIPHCLK_XSPI2;

    RCC_PeriphCLKInitStruct.Xspi1ClockSelection =
        RCC_XSPI1CLKSOURCE_HCLK;

    RCC_PeriphCLKInitStruct.Xspi2ClockSelection =
        RCC_XSPI2CLKSOURCE_HCLK;

    if (HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
}
//void SystemClock_Config(void)
//{
//  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
//  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
//  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
//
//  /** Configure the System Power Supply
//  */
//  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
//  {
//    Error_Handler();
//  }
//
//  /** Configure the main internal regulator output voltage
//  */
//  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
//  {
//    Error_Handler();
//  }
//
//  /* Enable HSI */
//  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
//  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
//  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
//  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
//  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_NONE;
//  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
//  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
//  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
//  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
//  {
//    Error_Handler();
//  }
//
//  /* Wait HSE stabilization time before its selection as PLL source. */
//  HAL_Delay(HSE_STARTUP_TIMEOUT);
//
//  /** Initializes TIMPRE when TIM is used as Systick Clock Source
//  */
//  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_TIM;
//  PeriphClkInitStruct.TIMPresSelection = RCC_TIMPRES_DIV1;
//  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
//  {
//    Error_Handler();
//  }
//
//  /** Get current CPU/System buses clocks configuration and if necessary switch
// to intermediate HSI clock to ensure target clock can be set
//  */
//  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
//  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
//     (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
//  {
//    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
//    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
//    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
//    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
//    {
//      /* Initialization Error */
//      Error_Handler();
//    }
//  }
//
//  /** Initializes the RCC Oscillators according to the specified parameters
//  * in the RCC_OscInitTypeDef structure.
//  */
//  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
//  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DIGITAL;
//  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
//  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSE;
//  RCC_OscInitStruct.PLL1.PLLM = 3;
//  RCC_OscInitStruct.PLL1.PLLN = 75;
//  RCC_OscInitStruct.PLL1.PLLFractional = 0;
//  RCC_OscInitStruct.PLL1.PLLP1 = 1;
//  RCC_OscInitStruct.PLL1.PLLP2 = 1;
//  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
//  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
//  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
//  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
//  {
//    Error_Handler();
//  }
//
//  /** Initializes the CPU, AHB and APB buses clocks
//  */
//  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK|RCC_CLOCKTYPE_HCLK
//                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
//                              |RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_PCLK5
//                              |RCC_CLOCKTYPE_PCLK4;
//  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
//  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
//  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV4;
//  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
//  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
//  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
//  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
//  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
//  RCC_ClkInitStruct.IC1Selection.ClockDivider = 2;
//  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
//  RCC_ClkInitStruct.IC2Selection.ClockDivider = 3;
//  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
//  RCC_ClkInitStruct.IC6Selection.ClockDivider = 3;
//  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
//  RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;
//
//  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
//  {
//    Error_Handler();
//  }
//}

/**
  * @brief RIF Initialization Function
  * @param None
  * @retval None
  */
static void SystemIsolation_Config(void)
{

/* USER CODE BEGIN RIF_Init 0 */

/* USER CODE END RIF_Init 0 */

  /* set all required IPs as secure privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /*RIMC configuration*/
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;

  HAL_RIF_RIMC_ConfigMasterAttributes(
      RIF_MASTER_INDEX_NPU,
      &RIMC_master
  );

  HAL_RIF_RISC_SetSlaveSecureAttributes(
      RIF_RISC_PERIPH_INDEX_NPU,
      RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV
  );

  HAL_RIF_RIMC_ConfigMasterAttributes(
      RIF_MASTER_INDEX_DCMIPP,
      &RIMC_master
  );

  HAL_RIF_RISC_SetSlaveSecureAttributes(
      RIF_RISC_PERIPH_INDEX_CSI,
      RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV
  );

  HAL_RIF_RISC_SetSlaveSecureAttributes(
      RIF_RISC_PERIPH_INDEX_DCMIPP,
      RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV
  );

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_OTG1, &RIMC_master);

  /*RISUP configuration*/
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG1HS , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_ADC12 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  /* RIF-Aware IPs Config */

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_12,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

/* USER CODE BEGIN RIF_Init 1 */

/* USER CODE END RIF_Init 1 */
/* USER CODE BEGIN RIF_Init 2 */

/* USER CODE END RIF_Init 2 */

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};
  uint32_t primask_bit = __get_PRIMASK();
  __disable_irq();

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region 0 and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x341F8000;
  MPU_InitStruct.LimitAddress = 0x341FFFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RW;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.DisablePrivExec = MPU_PRIV_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Attribute 0 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

  /* Exit critical section to lock the system and avoid any issue around MPU mechanism */
  __set_PRIMASK(primask_bit);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
  USBPD_DPM_TimerCounter();
#if defined(_GUI_INTERFACE)
  GUI_TimerCounter();
#endif /* _GUI_INTERFACE */
  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while (1)
  {
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
    HAL_Delay(200);
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
  /* Infinite loop */
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
