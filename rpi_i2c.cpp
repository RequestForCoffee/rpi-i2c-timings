#include <bcm_host.h>
#include <iomanip>
#include <iostream>

int main() {
  unsigned int peripheral_addr_base =
    bcm_host_get_peripheral_address();

  std::cout << "ARM Linux physical address base of peripherals: "
            << std::hex << peripheral_addr_base << std::endl;

  
}
