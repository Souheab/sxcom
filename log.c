#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void log_fatal(char* str) {
  printf("FATAL ERROR: %s\n", str);
  exit(1);
}

void log_normal(char* str) {
  printf("LOG: %s\n", str);
}

void log_normalf(char* str, ...) {
  va_list args;
  va_start(args, str);
  printf("LOG: ");
  vprintf(str, args);
  printf("\n");
  va_end(args);
}
