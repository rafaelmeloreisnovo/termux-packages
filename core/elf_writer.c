#include "elf_writer.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static uint16_t get_machine_type(uint8_t arch) {
  switch (arch) {
    case 0: return ELF_MACHINE_AARCH64;  /* aarch64 */
    case 1: return ELF_MACHINE_ARM;       /* arm (armv7a) */
    case 2: return ELF_MACHINE_X86_64;   /* x86_64 */
    case 3: return ELF_MACHINE_X86;      /* i686 */
    default: return ELF_MACHINE_ARM;
  }
}

static uint8_t get_elf_class(uint8_t arch) {
  switch (arch) {
    case 0:  /* aarch64 */
    case 2:  /* x86_64 */
      return ELF_CLASS_64;
    case 1:  /* arm */
    case 3:  /* i686 */
      return ELF_CLASS_32;
    default:
      return ELF_CLASS_32;
  }
}

int termux_create_elf_executable(const char *output_path, uint8_t arch,
                                  const uint8_t *code, size_t code_size,
                                  uint64_t entry_point) {
  (void)entry_point;  /* Used for future dynamic linking support */

  if (!output_path || !code || code_size == 0) {
    return -1;
  }

  int fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
  if (fd < 0) {
    return -1;
  }

  uint8_t elf_class = get_elf_class(arch);
  uint16_t machine = get_machine_type(arch);

  if (elf_class == ELF_CLASS_64) {
    elf_header_t header = {
      .magic = {0x7f, 'E', 'L', 'F'},
      .class = ELF_CLASS_64,
      .data = ELF_DATA_LITTLE_ENDIAN,
      .version = ELF_VERSION_CURRENT,
      .osabi = ELF_OSABI_ANDROID,
      .abi_version = 0,
      .type = ELF_TYPE_EXECUTABLE,
      .machine = machine,
      .version2 = 1,
    };

    if (write(fd, &header, sizeof(header)) != sizeof(header)) {
      close(fd);
      return -1;
    }

    uint64_t program_header_offset = sizeof(elf_header_t);
    uint64_t code_offset = program_header_offset + sizeof(elf_program_header_64_t);
    uint64_t base_address = 0x400000;

    elf_program_header_64_t phdr = {
      .p_type = PT_LOAD,
      .p_flags = 5,  /* PF_R | PF_X */
      .p_offset = code_offset,
      .p_vaddr = base_address,
      .p_paddr = base_address,
      .p_filesz = code_size,
      .p_memsz = code_size,
      .p_align = 0x1000,
    };

    if (write(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
      close(fd);
      return -1;
    }

    if (write(fd, code, code_size) != (ssize_t)code_size) {
      close(fd);
      return -1;
    }
  } else {
    elf_header_t header = {
      .magic = {0x7f, 'E', 'L', 'F'},
      .class = ELF_CLASS_32,
      .data = ELF_DATA_LITTLE_ENDIAN,
      .version = ELF_VERSION_CURRENT,
      .osabi = ELF_OSABI_ANDROID,
      .abi_version = 0,
      .type = ELF_TYPE_EXECUTABLE,
      .machine = machine,
      .version2 = 1,
    };

    if (write(fd, &header, sizeof(header)) != sizeof(header)) {
      close(fd);
      return -1;
    }

    uint32_t program_header_offset = sizeof(elf_header_t);
    uint32_t code_offset = program_header_offset + sizeof(elf_program_header_32_t);
    uint32_t base_address = 0x8000;

    elf_program_header_32_t phdr = {
      .p_type = PT_LOAD,
      .p_offset = code_offset,
      .p_vaddr = base_address,
      .p_paddr = base_address,
      .p_filesz = code_size,
      .p_memsz = code_size,
      .p_flags = 5,  /* PF_R | PF_X */
      .p_align = 0x1000,
    };

    if (write(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
      close(fd);
      return -1;
    }

    if (write(fd, code, code_size) != (ssize_t)code_size) {
      close(fd);
      return -1;
    }
  }

  close(fd);
  return 0;
}
