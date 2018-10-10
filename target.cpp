#include <stdio.h>
#include <unistd.h>

void trap() {
  printf("This message is trapped\n");
}

int main() {
  printf("HELLO TARGET IS STARTED\n");
  printf("the function address is %p\n", &trap);
  printf("3\n");
  sleep(1);
  printf("2\n");
  sleep(1);
  printf("1\n");
  sleep(1);

  trap();

  sleep(1);
  printf("GOODBYE TARGET WILL EXIT\n");
  fflush(stdout);

  return 0;
}
