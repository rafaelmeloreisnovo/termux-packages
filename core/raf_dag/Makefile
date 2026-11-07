CC ?= clang
CFLAGS_CORE = -std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
 -fno-unwind-tables -fno-asynchronous-unwind-tables -fdata-sections \
 -ffunction-sections -fvisibility=hidden -Wall -Wextra -Wshadow \
 -Wconversion -Werror -pedantic
.RECIPEPREFIX := >
.PHONY: verify clean
verify:
>$(CC) $(CFLAGS_CORE) -c raf_dag.c -o raf_dag.o
>@test -z "$$(nm -u raf_dag.o)" || (nm -u raf_dag.o && false)
>$(CC) -std=c11 -O2 -Wall -Wextra -Werror test.c raf_dag.o -o test-raf-dag
>./test-raf-dag
clean:
>rm -f raf_dag.o test-raf-dag
