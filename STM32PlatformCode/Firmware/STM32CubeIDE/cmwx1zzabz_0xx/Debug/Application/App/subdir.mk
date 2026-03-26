################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
$(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/app_subghz_phy.c \
$(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/subghz_phy_app.c \
$(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_telemetry.c \
$(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_config.c \
$(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_radio.c \
$(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_utils.c \
$(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/cmox_low_level.c \

OBJS += \
./Application/App/app_subghz_phy.o \
./Application/App/subghz_phy_app.o \
./Application/App/physec_telemetry.o \
./Application/App/physec_config.o \
./Application/App/physec_radio.o \
./Application/App/physec_utils.o \
./Application/App/cmox_low_level.o \

C_DEPS += \
./Application/App/app_subghz_phy.d \
./Application/App/subghz_phy_app.d \
./Application/App/physec_telemetry.d \
./Application/App/physec_config.d \
./Application/App/physec_radio.d \
./Application/App/physec_utils.d \
./Application/App/cmox_low_level.d \

CRYPTO_LIB_INCLUDE=-I$(PROJECT_ROOT)/STM32PlatformCode/Middlewares/ST/STM32_Cryptographic/include/

# Each subdirectory must supply rules for building sources it contributes
Application/App/app_subghz_phy.o: $(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/app_subghz_phy.c Application/App/subdir.mk
	arm-none-eabi-gcc "$<" $(CRYPTO_LIB_INCLUDE) -mcpu=cortex-m0plus -std=gnu11 -g3 -DSTM32L072xx -DCMWX1ZZABZ0XX -c -I../../../SubGHz_Phy/App -I../../../SubGHz_Phy/Target -I../../../Core/Inc -I../../../../Utilities/misc -I../../../../Utilities/timer -I../../../../Utilities/trace/adv_trace -I../../../../Utilities/lpm/tiny_lpm -I../../../../Utilities/sequencer -I../../../../Drivers/BSP/B-L072Z-LRWAN1 -I../../../../Drivers/BSP/CMWX1ZZABZ_0xx -I../../../../Drivers/STM32L0xx_HAL_Driver/Inc -I../../../../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../../../../Drivers/CMSIS/Include -I../../../../Middlewares/Third_Party/SubGHz_Phy -I../../../../Middlewares/Third_Party/SubGHz_Phy/sx1276 -I../../../SubGHz_Phy/App/libphysec -Os -ffunction-sections -Wall -Wextra -Wpedantic -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"
Application/App/subghz_phy_app.o: $(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/subghz_phy_app.c Application/App/subdir.mk
	arm-none-eabi-gcc "$<" $(CRYPTO_LIB_INCLUDE)  -mcpu=cortex-m0plus -std=gnu11 -g3 -DSTM32L072xx -DCMWX1ZZABZ0XX -c -I../../../SubGHz_Phy/App -I../../../SubGHz_Phy/Target -I../../../Core/Inc -I../../../../Utilities/misc -I../../../../Utilities/timer -I../../../../Utilities/trace/adv_trace -I../../../../Utilities/lpm/tiny_lpm -I../../../../Utilities/sequencer -I../../../../Drivers/BSP/B-L072Z-LRWAN1 -I../../../../Drivers/BSP/CMWX1ZZABZ_0xx -I../../../../Drivers/STM32L0xx_HAL_Driver/Inc -I../../../../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../../../../Drivers/CMSIS/Include -I../../../../Middlewares/Third_Party/SubGHz_Phy -I../../../../Middlewares/Third_Party/SubGHz_Phy/sx1276 -I../../../SubGHz_Phy/App/libphysec -Os -ffunction-sections -Wall -Wextra -Wpedantic -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"
Application/App/physec_telemetry.o: $(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_telemetry.c Application/App/subdir.mk
	arm-none-eabi-gcc "$<" $(CRYPTO_LIB_INCLUDE) -mcpu=cortex-m0plus -std=gnu11 -g3 -DSTM32L072xx -DCMWX1ZZABZ0XX -c -I../../../SubGHz_Phy/App -I../../../SubGHz_Phy/Target -I../../../Core/Inc -I../../../../Utilities/misc -I../../../../Utilities/timer -I../../../../Utilities/trace/adv_trace -I../../../../Utilities/lpm/tiny_lpm -I../../../../Utilities/sequencer -I../../../../Drivers/BSP/B-L072Z-LRWAN1 -I../../../../Drivers/BSP/CMWX1ZZABZ_0xx -I../../../../Drivers/STM32L0xx_HAL_Driver/Inc -I../../../../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../../../../Drivers/CMSIS/Include -I../../../../Middlewares/Third_Party/SubGHz_Phy -I../../../../Middlewares/Third_Party/SubGHz_Phy/sx1276 -I../../../SubGHz_Phy/App/libphysec -Os -ffunction-sections -Wall -Wextra -Wpedantic -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"
Application/App/physec_config.o: $(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_config.c Application/App/subdir.mk
	arm-none-eabi-gcc "$<" $(CRYPTO_LIB_INCLUDE) -mcpu=cortex-m0plus -std=gnu11 -g3 -DSTM32L072xx -DCMWX1ZZABZ0XX -c -I../../../SubGHz_Phy/App -I../../../SubGHz_Phy/Target -I../../../Core/Inc -I../../../../Utilities/misc -I../../../../Utilities/timer -I../../../../Utilities/trace/adv_trace -I../../../../Utilities/lpm/tiny_lpm -I../../../../Utilities/sequencer -I../../../../Drivers/BSP/B-L072Z-LRWAN1 -I../../../../Drivers/BSP/CMWX1ZZABZ_0xx -I../../../../Drivers/STM32L0xx_HAL_Driver/Inc -I../../../../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../../../../Drivers/CMSIS/Include -I../../../../Middlewares/Third_Party/SubGHz_Phy -I../../../../Middlewares/Third_Party/SubGHz_Phy/sx1276 -I../../../SubGHz_Phy/App/libphysec -Os -ffunction-sections -Wall -Wextra -Wpedantic -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"
Application/App/physec_radio.o: $(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_radio.c Application/App/subdir.mk
	arm-none-eabi-gcc "$<" $(CRYPTO_LIB_INCLUDE) -mcpu=cortex-m0plus -std=gnu11 -g3 -DSTM32L072xx -DCMWX1ZZABZ0XX -c -I../../../SubGHz_Phy/App -I../../../SubGHz_Phy/Target -I../../../Core/Inc -I../../../../Utilities/misc -I../../../../Utilities/timer -I../../../../Utilities/trace/adv_trace -I../../../../Utilities/lpm/tiny_lpm -I../../../../Utilities/sequencer -I../../../../Drivers/BSP/B-L072Z-LRWAN1 -I../../../../Drivers/BSP/CMWX1ZZABZ_0xx -I../../../../Drivers/STM32L0xx_HAL_Driver/Inc -I../../../../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../../../../Drivers/CMSIS/Include -I../../../../Middlewares/Third_Party/SubGHz_Phy -I../../../../Middlewares/Third_Party/SubGHz_Phy/sx1276 -I../../../SubGHz_Phy/App/libphysec -Os -ffunction-sections -Wall -Wextra -Wpedantic -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"
Application/App/physec_utils.o: $(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/physec_utils.c Application/App/subdir.mk
	arm-none-eabi-gcc "$<" $(CRYPTO_LIB_INCLUDE) -mcpu=cortex-m0plus -std=gnu11 -g3 -DSTM32L072xx -DCMWX1ZZABZ0XX -c -I../../../SubGHz_Phy/App -I../../../SubGHz_Phy/Target -I../../../Core/Inc -I../../../../Utilities/misc -I../../../../Utilities/timer -I../../../../Utilities/trace/adv_trace -I../../../../Utilities/lpm/tiny_lpm -I../../../../Utilities/sequencer -I../../../../Drivers/BSP/B-L072Z-LRWAN1 -I../../../../Drivers/BSP/CMWX1ZZABZ_0xx -I../../../../Drivers/STM32L0xx_HAL_Driver/Inc -I../../../../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../../../../Drivers/CMSIS/Include -I../../../../Middlewares/Third_Party/SubGHz_Phy -I../../../../Middlewares/Third_Party/SubGHz_Phy/sx1276 -I../../../SubGHz_Phy/App/libphysec -Os -ffunction-sections -Wall -Wextra -Wpedantic -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"
Application/App/cmox_low_level.o: $(PROJECT_ROOT)/STM32PlatformCode/Firmware/SubGHz_Phy/App/cmox_low_level.c Application/App/subdir.mk
	arm-none-eabi-gcc "$<" $(CRYPTO_LIB_INCLUDE) -mcpu=cortex-m0plus -std=gnu11 -g3 -DSTM32L072xx -DCMWX1ZZABZ0XX -c -I../../../SubGHz_Phy/App -I../../../SubGHz_Phy/Target -I../../../Core/Inc -I../../../../Utilities/misc -I../../../../Utilities/timer -I../../../../Utilities/trace/adv_trace -I../../../../Utilities/lpm/tiny_lpm -I../../../../Utilities/sequencer -I../../../../Drivers/BSP/B-L072Z-LRWAN1 -I../../../../Drivers/BSP/CMWX1ZZABZ_0xx -I../../../../Drivers/STM32L0xx_HAL_Driver/Inc -I../../../../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../../../../Drivers/CMSIS/Include -I../../../../Middlewares/Third_Party/SubGHz_Phy -I../../../../Middlewares/Third_Party/SubGHz_Phy/sx1276 -I../../../SubGHz_Phy/App/libphysec -Os -ffunction-sections -Wall -Wextra -Wpedantic -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"



clean: clean-Application-2f-App

clean-Application-2f-App:
	-$(RM) ./Application/App/app_subghz_phy.cyclo ./Application/App/app_subghz_phy.d ./Application/App/app_subghz_phy.o ./Application/App/app_subghz_phy.su ./Application/App/subghz_phy_app.cyclo ./Application/App/subghz_phy_app.d ./Application/App/subghz_phy_app.o ./Application/App/subghz_phy_app.su

.PHONY: clean-Application-2f-App

