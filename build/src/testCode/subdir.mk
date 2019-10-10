# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/testCode/loggerTest.c 

OBJS += \
./src/testCode/loggerTest.o 

C_DEPS += \
./src/testCode/loggerTest.d 


# Each subdirectory must supply rules for building sources it contributes
src/testCode/%.o: ../src/testCode/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I"../src/logger" -O3 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


