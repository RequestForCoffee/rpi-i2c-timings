#include <bcm_host.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#ifndef max
#define max(a,b)   (((a) > (b)) ? (a) : (b))
#endif

// Register layoout is defined in the BCM2711 ARM Peripherals Manual, section 3.2.
//
// The manual lists 0x7E804000 as the address for the BSC1 bus (I2C1). This is
// a *bus* address; the ARM mapping MMU maps it to the ARM *physical* address,
// as seen via /dev/mem.
//
// For instance, on the Raspberry Pi 4B the bus adress offset 0x7E000000 is
// mapped to ARM physical address base 0xFE000000:
// https://github.com/raspberrypi/linux/blob/17cba8a/arch/arm/boot/dts/bcm2711-rpi-4-b.dts#L46
#define I2C1_OFFSET 0x00804000

typedef struct I2CRegisterSetStruct {
  uint32_t C;
  uint32_t S;
  uint32_t DLEN;
  uint32_t A;
  uint32_t FIFO;
  uint32_t DIV;
  uint32_t DEL;
  uint32_t CLKT;
} I2CRegisterSet;

const char core_clk_debugfs_path[] = "/sys/kernel/debug/clk/vpu/clk_rate";

void print_core_clock_speed() {
  FILE* clk_fh;
  if ((clk_fh = fopen(core_clk_debugfs_path, "r")) == NULL) {
    printf("Could not open VPU core clock DebugFS path\n");
    return;
  }
  uint32_t core_clk_rate;
  if (fscanf(clk_fh, "%u", &core_clk_rate) != 1) {
    printf("Could not read VPU core clock\n");
    return;
  }

  if (fclose(clk_fh) != 0) {
    printf("Could not close VPU core clock DebugFS path\n");
    return;
  }

  printf("Core clock (Hz): %u\n", core_clk_rate);
}

int main(int argc, char** argv) {
  if (argc != 1 && argc != 3) {
    printf("Usage: rpi-i2c [<div.cdiv> <clkt.tout>]\n");
    exit(-1);
  }

  printf("Raspberry Pi I2C timing utility\n\n");
  printf("To read current timing values, run the program without arguments.\n");
  printf("To set new timing values: %s <div.cdiv> <clkt.tout>\n\n", argv[0]);

  uint16_t cdiv = 0, tout = 0;
  int set_new = 0;
  char* endptr;
  if (argc == 3) {
    long cdiv_l = strtol(argv[1], &endptr, 10);
    if (cdiv_l == LONG_MIN || cdiv_l == LONG_MAX) {
      perror("Could not parse CDIV value");
      return errno;
    }
    if (cdiv_l <= 0 || cdiv_l > 0xFFFF) {
      printf("CDIV out of bounds (0, 65535)\n");
      return ERANGE;
    }
    // CDIV is always rounded down to an even number.
    cdiv = 0xFFFE & cdiv_l;

    long tout_l = strtol(argv[2], &endptr, 10);
    if (tout_l == LONG_MIN || tout_l == LONG_MAX) {
      perror("Could not parse TOUT value");
      return errno;
    }
    if (tout_l < 0 || tout_l > 0xFFFF) {
      printf("TOUT out of bounds (0, 65535)\n");
      return ERANGE;
    }
    tout = 0xFFFF & tout_l;

    set_new = 1;
  }

  uint32_t peripheral_addr_base = bcm_host_get_peripheral_address();
  printf("ARM peripheral address base: %#010x\n",
         peripheral_addr_base);

  print_core_clock_speed();

  int devmem_fd;
  if ((devmem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0) {
    perror("Could not open /dev/mem");
    return errno;
  }

  off_t offset = peripheral_addr_base + I2C1_OFFSET;
  printf("I2C1 controller address base: %#010x\n",
         (uint32_t)offset);

  I2CRegisterSet* i2c_1 = (I2CRegisterSet*)(mmap(
    NULL,
    sizeof(I2CRegisterSet),
    PROT_READ|PROT_WRITE,
    MAP_SHARED,
    devmem_fd,
    offset)
  );

  // mmap(2): "After the mmap() call has returned, the file descriptor, fd,
  // can be closed immediately without invalidating the mapping."
  close(devmem_fd);

  if (i2c_1 == MAP_FAILED) {
    perror("Could not mmap I2C registers");
    return errno;
  }

  // CDIV and TOUT use only the lower halves of the 32-bit registers.
  printf("DIV.CDIV: %u\n", i2c_1->DIV & 0xFFFF);
  printf("CLKT.TOUT: %u\n", i2c_1->CLKT & 0xFFFF);

  if (set_new) {
    // Sanity check: delay values should not exceed CDIV/2.
    uint16_t FEDL = (i2c_1->DEL >> 16) & 0xFFFF;
    uint16_t REDL = i2c_1->DEL & 0xFFFF;

    // FEDL & REDL calculation as per the i2c-bcm2835 driver code.
    FEDL = max(cdiv / 16, 1u);
    REDL = max(cdiv / 4, 1u);
    printf("Updating delay values to: FEDL=%u, REDL=%u.\n", FEDL, REDL);

    i2c_1->DIV = (uint32_t)cdiv & 0x0000FFFF;
    i2c_1->CLKT = (uint32_t)tout & 0x0000FFFF;
    i2c_1->DEL = (uint32_t)(((uint32_t)FEDL << 16) | REDL);
    printf("Timing values updated: CDIV=%u, CLKT=%u.\n", cdiv, tout);
  }

  munmap(i2c_1, sizeof(I2CRegisterSet));
  return 0;
}
