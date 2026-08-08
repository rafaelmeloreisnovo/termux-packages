#include "gpu_device.h"
#include <stdlib.h>
#include <stdio.h>

void gpu_device_cleanup(void *gpu_device) {
  if (!gpu_device) {
    return;
  }

  fprintf(stdout, "Cleaning up GPU device\n");
}
