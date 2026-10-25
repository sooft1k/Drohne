# Host-Tests, laufen auf dem PC (nicht auf dem STM32):
#   make -C tests
# Anderer Compiler: make -C tests CC=clang

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -Istub -I../Core/Inc -I../Sensors
BUILD_DIR = build

test: $(BUILD_DIR)/test_arming
	./$(BUILD_DIR)/test_arming
	@# batbypass darf nur ueber das Terminal einschaltbar sein (nicht ueber rc.c/Funk)
	@users=$$(grep -l "Arming_SetBatBypass" ../Core/Src/*.c ../Sensors/*.c | sort | tr '\n' ' '); \
	if [ "$$users" != "../Sensors/arming.c ../Sensors/command.c " ]; then \
		echo "FEHLER: Arming_SetBatBypass ausserhalb von command.c benutzt: $$users"; exit 1; \
	else echo "batbypass nur ueber das Terminal: OK"; fi

$(BUILD_DIR)/test_arming: test_arming.c ../Sensors/arming.c ../Sensors/arming.h stub/stm32f4xx_hal.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) test_arming.c ../Sensors/arming.c -lm -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: test clean
