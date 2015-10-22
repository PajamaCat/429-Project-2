################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../Event.cc \
../Link.cc \
../Node.cc \
../RoutingProtocolImpl.cc \
../Simulator.cc 

CC_DEPS += \
./Event.d \
./Link.d \
./Node.d \
./RoutingProtocolImpl.d \
./Simulator.d 

OBJS += \
./Event.o \
./Link.o \
./Node.o \
./RoutingProtocolImpl.o \
./Simulator.o 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


