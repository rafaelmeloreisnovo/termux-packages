#ifndef TERMUX_ELF_WRITER_H
#define TERMUX_ELF_WRITER_H

#include <stdint.h>
#include <stddef.h>

/* ELF header constants */
#define ELF_MAGIC 0x464C457F           /* "\x7FELF" */
#define ELF_CLASS_32 1
#define ELF_CLASS_64 2
#define ELF_DATA_LITTLE_ENDIAN 1
#define ELF_VERSION_CURRENT 1
#define ELF_OSABI_ANDROID 0
#define ELF_TYPE_EXECUTABLE 2
#define ELF_TYPE_SHARED 3

/* Machine types */
#define ELF_MACHINE_ARM 40     /* ARM/Thumb Architecture */
#define ELF_MACHINE_X86 3      /* Intel 80386 */
#define ELF_MACHINE_AARCH64 183  /* ARM AARCH64 */
#define ELF_MACHINE_X86_64 62  /* AMD x86-64 */

/* Flags for different architectures */
#define ELF_FLAGS_ARM_EABI_VERSION 0x05000000

/* Section types */
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_DYNSYM 11

/* Program header types */
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_GNU_EH_FRAME 0x6474e550
#define PT_GNU_STACK 0x6474e551
#define PT_GNU_RELRO 0x6474e552

typedef struct {
  uint8_t magic[4];      /* 0x7f, 'E', 'L', 'F' */
  uint8_t class;         /* ELF_CLASS_32 or ELF_CLASS_64 */
  uint8_t data;          /* ELF_DATA_LITTLE_ENDIAN */
  uint8_t version;       /* ELF_VERSION_CURRENT */
  uint8_t osabi;         /* ELF_OSABI_* */
  uint8_t abi_version;
  uint8_t pad[7];
  uint16_t type;         /* ELF_TYPE_* */
  uint16_t machine;      /* ELF_MACHINE_* */
  uint32_t version2;
} elf_header_t;

typedef struct {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
} elf_program_header_32_t;

typedef struct {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
} elf_program_header_64_t;

typedef struct {
  uint32_t sh_name;
  uint32_t sh_type;
  uint32_t sh_flags;
  uint32_t sh_addr;
  uint32_t sh_offset;
  uint32_t sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  uint32_t sh_addralign;
  uint32_t sh_entsize;
} elf_section_header_t;

int termux_create_elf_executable(const char *output_path, uint8_t arch,
                                  const uint8_t *code, size_t code_size,
                                  uint64_t entry_point);

#endif
