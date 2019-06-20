################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../c2x_v0.c 

OBJS += \
./c2x_v0.o 

C_DEPS += \
./c2x_v0.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I"/home/ga53goc/contiki/cpu" -I"/home/ga53goc/contiki/core/sys" -I"/home/ga53goc/contiki/platform" -I"/home/ga53goc/contiki/platform/zoul" -I"/home/ga53goc/contiki/core" -I"/home/ga53goc/contiki" -O2 -g -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


