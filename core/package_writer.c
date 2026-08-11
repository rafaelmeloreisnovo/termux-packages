#include "manifest.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <inttypes.h>

/* Simple tar header (USTAR format, 512 bytes aligned) */
typedef struct {
  char filename[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char filename_prefix[155];
  char padding[12];
} tar_header_t;

static void fill_tar_octal(char *field, size_t len, uint64_t value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%0*lo", (int)len - 1, (unsigned long)value);
  memcpy(field, buf, strlen(buf));
}

static void create_tar_header(tar_header_t *header, const char *filename,
                             uint64_t filesize, time_t mtime) {
  memset(header, 0, sizeof(*header));

  strncpy(header->filename, filename, sizeof(header->filename) - 1);
  strcpy(header->mode, "0644   ");
  strcpy(header->uid, "0     ");
  strcpy(header->gid, "0     ");

  fill_tar_octal(header->size, sizeof(header->size), filesize);
  fill_tar_octal(header->mtime, sizeof(header->mtime), mtime);

  memset(header->chksum, ' ', sizeof(header->chksum));
  header->typeflag = '0';
  strcpy(header->magic, "ustar");
  header->version[0] = '0';
  header->version[1] = '0';
  strcpy(header->uname, "root");
  strcpy(header->gname, "root");

  /* Calculate checksum */
  unsigned int chksum = 0;
  unsigned char *ptr = (unsigned char *)header;
  for (size_t i = 0; i < sizeof(*header); i++) {
    chksum += ptr[i];
  }

  snprintf(header->chksum, sizeof(header->chksum), "%06o ", chksum);
}

int termux_create_tar(const char *output_path, const char *pkg_name,
                      const char *version, const char *arch,
                      const char *build_output) {
  if (!output_path || !pkg_name || !version || !build_output) {
    return -1;
  }

  int fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  time_t now = time(NULL);

  /* Create metadata header */
  tar_header_t metadata;
  char metadata_name[100];
  snprintf(metadata_name, sizeof(metadata_name), "%s-%s.json", pkg_name, version);

  char metadata_content[512];
  snprintf(metadata_content, sizeof(metadata_content),
           "{\"name\":\"%s\",\"version\":\"%s\",\"arch\":\"%s\",\"timestamp\":%" PRIdMAX "}\n",
           pkg_name, version, arch, (intmax_t)now);

  size_t metadata_size = strlen(metadata_content);
  create_tar_header(&metadata, metadata_name, metadata_size, now);

  if (write(fd, &metadata, sizeof(metadata)) != sizeof(metadata)) {
    perror("write header");
    close(fd);
    return -1;
  }

  if (write(fd, metadata_content, metadata_size) != (ssize_t)metadata_size) {
    perror("write metadata");
    close(fd);
    return -1;
  }

  size_t padding = 512 - (metadata_size % 512);
  if (padding < 512) {
    char zeros[512] = {0};
    if (write(fd, zeros, padding) != (ssize_t)padding) {
      perror("write padding");
      close(fd);
      return -1;
    }
  }

  /* Create build output entry */
  char output_name[100];
  snprintf(output_name, sizeof(output_name), "%s-%s.build.log", pkg_name, version);

  size_t output_size = strlen(build_output);
  create_tar_header(&metadata, output_name, output_size, now);

  if (write(fd, &metadata, sizeof(metadata)) != sizeof(metadata)) {
    perror("write output header");
    close(fd);
    return -1;
  }

  if (write(fd, build_output, output_size) != (ssize_t)output_size) {
    perror("write output");
    close(fd);
    return -1;
  }

  padding = 512 - (output_size % 512);
  if (padding < 512) {
    char zeros[512] = {0};
    if (write(fd, zeros, padding) != (ssize_t)padding) {
      perror("write padding");
      close(fd);
      return -1;
    }
  }

  /* Write tar end-of-archive marker (two 512-byte blocks of zeros) */
  char zeros[1024] = {0};
  if (write(fd, zeros, sizeof(zeros)) != sizeof(zeros)) {
    perror("write EOF marker");
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}
