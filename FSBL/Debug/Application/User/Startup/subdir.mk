################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Application/User/Startup/startup_stm32n657x0hxq_fsbl.s 

OBJS += \
./Application/User/Startup/startup_stm32n657x0hxq_fsbl.o 

S_DEPS += \
./Application/User/Startup/startup_stm32n657x0hxq_fsbl.d 


# Each subdirectory must supply rules for building sources it contributes
Application/User/Startup/%.o: ../Application/User/Startup/%.s Application/User/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m55 -g3 -DDEBUG -DTX_INCLUDE_USER_DEFINE_FILE -DTX_SINGLE_MODE_SECURE=1 -c -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/Application/User/Core/Inc" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/Application/User/AZURE_RTOS/App" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/Application/User/USBPD/App" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/Application/User/USBPD/Target" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/Application/User/USBX/App" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/Application/User/USBX/Target" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Drivers/BSP/STM32N6570-DK" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Drivers/BSP/Components" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Drivers/BSP/Components/tcpp0203" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Drivers/STM32N6xx_HAL_Driver/Inc" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Drivers/CMSIS/Device/ST/STM32N6xx/Include" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Drivers/CMSIS/Include" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Middlewares/ST/threadx/common/inc" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Middlewares/ST/threadx/ports/cortex_m55/gnu/inc" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Middlewares/ST/STM32_USBPD_Library/Core/inc" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Middlewares/ST/STM32_USBPD_Library/Devices/STM32N6XX/inc" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Middlewares/ST/usbx/common/core/inc" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Middlewares/ST/usbx/ports/generic/inc" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers" -I"/home/ck/Desktop/Workspace/STM32CubeIDE Workspace/STM32N6570_USBD_CDC_FACE_DETECT/FSBL/../Middlewares/ST/usbx/common/usbx_device_classes/inc" -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Application-2f-User-2f-Startup

clean-Application-2f-User-2f-Startup:
	-$(RM) ./Application/User/Startup/startup_stm32n657x0hxq_fsbl.d ./Application/User/Startup/startup_stm32n657x0hxq_fsbl.o

.PHONY: clean-Application-2f-User-2f-Startup

