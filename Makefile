CXX = gcc
CXX_FLAGS = -Wall -Wextra -O3

rpi-i2c: rpi_i2c.c
	$(CXX) $(CXX_FLAGS) -I/opt/vc/include -L/opt/vc/lib -lbcm_host -o rpi-i2c rpi_i2c.c

.PHONY: rpi-i2c
