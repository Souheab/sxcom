#include <signal.h>
#include <stdio.h>
#include<stdbool.h>

bool *global_quit;

void handle_sigint() {
  printf("SIGINT received\n");
  *global_quit = true;
}

void handle_signals(bool *quit) {
  global_quit = quit;
  signal(SIGINT, handle_sigint);
}
