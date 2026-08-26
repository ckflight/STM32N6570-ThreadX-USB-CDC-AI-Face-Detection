#include "app_threadx.h"
#include <stdint.h>
#include "ux_device_cdc_acm.h"
#include <stdio.h>
#include <string.h>
//****************** USB TASK *************
// USB 2.0 TX speed is 43 MB. Increasing buffer size decreases task loop time so dont go below 8192*2 which has 2.63k taks loop time
// 8192*4 buffer size makes task loop time 1.3khz with 43Mb throughput
#define USB_TX_BUFFER_SIZE 8192*4

__attribute__((aligned(32)))
static UCHAR usb_tx_buffer[USB_TX_BUFFER_SIZE];

static TX_THREAD usb_tx_thread;
static UCHAR usb_tx_stack[2048];
static VOID USB_TX_Task(ULONG arg);
static VOID MATH_Task(ULONG arg);

extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm;

volatile uint32_t usb_task_counter = 0;
volatile ULONG usb_actual_length = 0;
volatile UINT usb_write_status = 0;

//****************** LED 1 TASK *************
static TX_THREAD led1_thread;
static UCHAR led1_stack[1024];
static VOID LED1_Task(ULONG arg);
volatile uint32_t led1_task_counter = 0;

//****************** LED 2 TASK *************
static TX_THREAD led2_thread;
static UCHAR led2_stack[1024];
static VOID LED2_Task(ULONG arg);
volatile uint32_t led2_task_counter = 0;

//****************** MATH TASK *************
static TX_THREAD math_thread;
static UCHAR math_stack[2048];
volatile uint32_t math_task_counter = 0;
volatile float math_result = 0.0f;

//****************** AI TASK *************
static TX_THREAD ai_thread;
static UCHAR ai_stack[4096];
static VOID AI_Task(ULONG arg);

//****************** AI TASK *************
static TX_THREAD lcd_text_thread;
static UCHAR lcd_text_stack[2048];
static VOID LCD_Text_Task(ULONG arg);

#include "stai.h"
#include "stai_network.h"
#include "app_postprocess.h"
#include "app_camerapipeline.h"
#include "stm32_lcd.h"

extern stai_size number_output;
extern stai_ptr nn_out[STAI_NETWORK_OUT_NUM];
extern fd_blazeface_pp_static_param_t pp_params;
extern fd_pp_out_t pp_output;

extern volatile int32_t cameraFrameReceived;
volatile uint32_t ai_task_counter = 0;

extern stai_ptr nn_in;

__attribute__((aligned(32)))
static UCHAR usb_msg[64];

volatile int32_t ai_face_count = 0;
volatile uint32_t ai_result_ready = 0;
volatile uint32_t lcd_result_ready = 0;

#define LCD_FG_WIDTH  SCREEN_WIDTH
#define LCD_FG_HEIGHT SCREEN_HEIGHT
#define LCD_FG_FRAMEBUFFER_SIZE  (LCD_FG_WIDTH * LCD_FG_HEIGHT * 2)
extern uint8_t lcd_fg_buffer[2][LCD_FG_WIDTH * LCD_FG_HEIGHT * 2];

extern void NeuralNetwork_run(void);

UINT App_ThreadX_Init(VOID *memory_ptr)
{
    UINT ret;

    UX_PARAMETER_NOT_USED(memory_ptr);

    for (uint32_t i = 0; i < USB_TX_BUFFER_SIZE; i++)
    {
        usb_tx_buffer[i] = (UCHAR)i;
    }

    SCB_CleanDCache_by_Addr(
        (uint32_t *)usb_tx_buffer,
        USB_TX_BUFFER_SIZE
    );

    // CPU wrote the data so it is in cache. Clean cache -> ram so usb dma can transfer the correct data
    SCB_CleanDCache_by_Addr((uint32_t *)usb_tx_buffer, USB_TX_BUFFER_SIZE);

    ret = tx_thread_create(&usb_tx_thread, "USB TX", USB_TX_Task, 0, usb_tx_stack, sizeof(usb_tx_stack), 15, 15, 1, TX_AUTO_START);
    if (ret != TX_SUCCESS) return ret;

    ret = tx_thread_create(&led1_thread, "LED1", LED1_Task, 0, led1_stack, sizeof(led1_stack), 20, 20, 1, TX_AUTO_START);
    if (ret != TX_SUCCESS) return ret;

    ret = tx_thread_create(&led2_thread, "LED2", LED2_Task, 0, led2_stack, sizeof(led2_stack), 20, 20, 1, TX_AUTO_START);
    if (ret != TX_SUCCESS) return ret;

    ret = tx_thread_create(&math_thread, "MATH", MATH_Task, 0, math_stack, sizeof(math_stack), 20, 20, 1, TX_AUTO_START);
    if (ret != TX_SUCCESS) return ret;

    ret = tx_thread_create(&ai_thread, "AI", AI_Task, 0, ai_stack, sizeof(ai_stack), 10, 10, 1, TX_AUTO_START);
    if (ret != TX_SUCCESS) return ret;

    ret = tx_thread_create(&lcd_text_thread, "LCD TEXT", LCD_Text_Task, 0, lcd_text_stack, sizeof(lcd_text_stack), 20, 20, 1, TX_AUTO_START);
    if (ret != TX_SUCCESS) return ret;

    return TX_SUCCESS;
}

void MX_ThreadX_Init(void)
{
    tx_kernel_enter();
}


static VOID LCD_Text_Task(ULONG arg){

	UX_PARAMETER_NOT_USED(arg);

	char text[32];

	int32_t face_count;

	while(1){

		if(lcd_result_ready == 0){
			tx_thread_sleep(1);
			continue;
		}

		face_count = ai_face_count;

	    UTIL_LCD_Clear(0x00000000);

	    UTIL_LCD_SetFont(&Font20);
	    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_LIGHTGREEN);

	    snprintf(text, sizeof(text), "FACE DETECTED: %ld", (long)face_count);

	    UTIL_LCD_DisplayStringAt(10, 10, (uint8_t *)text, LEFT_MODE);

	    /* CPU cache -> PSRAM, so LTDC sees updated pixels */
	    SCB_CleanDCache_by_Addr((uint32_t *)lcd_fg_buffer[0], LCD_FG_FRAMEBUFFER_SIZE);

	    lcd_result_ready = 0;
	}


}

static VOID AI_Task(ULONG arg)
{
    UX_PARAMETER_NOT_USED(arg);

    while (1)
    {
        if (cameraFrameReceived == 0)
        {
            tx_thread_sleep(1);
            continue;
        }

        cameraFrameReceived = 0;

        /* Kameradan alınan görüntüyü modelden geçir */
        NeuralNetwork_run();

        /* Ham NN çıktısını gerçek yüz detection sonucuna çevir */
        app_postprocess_run((void **)nn_out, number_output, &pp_output, &pp_params);

        ai_task_counter++;

        /* Sonucu USB task'a bırak */
        ai_face_count = pp_output.nb_detect;

        ai_result_ready = 1;
        lcd_result_ready = 1;

        /* Sonraki kamera snapshot'ını başlat */
        CameraPipeline_IspUpdate();

        CameraPipeline_NNPipe_Start((uint8_t *)nn_in, DCMIPP_MODE_SNAPSHOT);

        HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);

    }
}

static VOID USB_TX_Task(ULONG arg)
{
    ULONG actual_length;
    UINT status;

    UX_PARAMETER_NOT_USED(arg);

    while (1)
    {
        if (cdc_acm == UX_NULL)
        {
            tx_thread_sleep(1);
            continue;
        }

        if (ai_result_ready)
        {
            ai_result_ready = 0;

            int len = snprintf((char *)usb_msg, sizeof(usb_msg), "AI=%lu Faces=%ld\r\n", (unsigned long)ai_task_counter, (long)ai_face_count);

            /*
             * CPU snprintf ile cache'e yazdı.
             * USB DMA RAM'den okuyacağı için cache -> RAM clean gerekiyor.
             * 32-byte cache line'a yuvarlıyoruz.
             */
            uint32_t clean_len = ((uint32_t)len + 31U) & ~31U;

            SCB_CleanDCache_by_Addr((uint32_t *)usb_msg, clean_len);

            status = ux_device_class_cdc_acm_write(cdc_acm, usb_msg, (ULONG)len, &actual_length);

            if (status == UX_SUCCESS)
            {
                usb_task_counter++;
            }
        }
        else
        {
            tx_thread_sleep(1);
        }
    }
}

// USB Throughput test function
//static VOID USB_TX_Task(ULONG arg)
//{
//    ULONG actual_length;
//    UINT status;
//
//    UX_PARAMETER_NOT_USED(arg);
//
//    while (1)
//    {
//        if (cdc_acm == UX_NULL)
//        {
//            tx_thread_sleep(1);
//            continue;
//        }
//
//        status = ux_device_class_cdc_acm_write(cdc_acm, usb_tx_buffer, USB_TX_BUFFER_SIZE, &actual_length);
//
//        if (status == UX_SUCCESS)
//        {
//            usb_task_counter++;
//            HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_7);
//        }
//        else
//        {
//            tx_thread_sleep(1);
//        }
//    }
//}

static VOID MATH_Task(ULONG arg)
{
    float x = 1.2345f;

    UX_PARAMETER_NOT_USED(arg);

    while (1)
    {
        /* Example math workload */
        x = x * 1.00001f + 0.0001f;
        x = x * x;
        x = x / 1.0001f;

        if (x > 100.0f)
            x = 1.2345f;

        math_result = x;
        math_task_counter++;

        for(int i = 0; i < 1000; i++){
        	math_result++;
        }

        HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_14);
        tx_thread_relinquish();
    }
}

static VOID LED1_Task(ULONG arg)
{
    UX_PARAMETER_NOT_USED(arg);

    while (1)
    {
        led1_task_counter++;
        //HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
        tx_thread_sleep(25);

    }
}

static VOID LED2_Task(ULONG arg)
{
    UX_PARAMETER_NOT_USED(arg);

    while (1)
    {
        led2_task_counter++;
        //HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
        tx_thread_sleep(50);
    }
}
