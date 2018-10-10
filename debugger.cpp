// original: https://gist.github.com/pgoodman/bfc4a5e5ec8cebd219f7
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

enum {
    DR7_BREAK_ON_EXEC  = 0,
    DR7_BREAK_ON_WRITE = 1,
    // UNDEFINED = 2
    DR7_BREAK_ON_RW    = 3,
};

enum {
    DR7_LEN_1 = 0,
    DR7_LEN_2 = 1,
    DR7_LEN_8 = 2,  // when long mode 
    DR7_LEN_4 = 3,
};

// DR7 レジスタの内部構造 32bit
typedef struct {
    char l0:1;
    char g0:1;
    char l1:1;
    char g1:1;
    char l2:1;
    char g2:1;
    char l3:1;
    char g3:1;
    char le:1;
    char ge:1;
    char pad1:3;
    char gd:1;
    char pad2:2;
    char rw0:2;
    char len0:2;
    char rw1:2;
    char len1:2;
    char rw2:2;
    char len2:2;
    char rw3:2;
    char len3:2;
} dr7_t;

typedef void sigactionhandler_t(int, siginfo_t*, void*);

// 子プロセスで PTRACE_TRACEME を使用して ATTTACH できるようにしてからターゲットを exec する
// 親プロセスでは 子プロセスに PTRACE_ATTACH して PTRACE_POKEUSER を利用して ハードウェアブレークポイントを仕掛ける
// long ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data);
int main(int argc, char **argv, char **envp)
{
  unsigned int break_addr = 0x401156;
  int status;

  pid_t pid = fork();
  if (pid == 0) {
    // 子プロセス
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);  // PTRACE_TRACEME: 親プロセスによってトレースされる
    execve(argv[1], argv + 1, envp);  // デバッグ対象を起動。 execve はプロセスを置き換えるのでこれ以降は実行されない
  }

  // 親プロセス
  waitpid(pid, &status, 0);  // 子プロセスのexecve 時のSIGTRAP を呼び出しを待つ。 

  // PTRACE_POKEUSER: USER領域に書き込み
  // DR0 に addr を書き込む
  if (ptrace(PTRACE_POKEUSER, pid, offsetof(struct user, u_debugreg[0]), break_addr) != 0) {
    fprintf(stderr, "[-]PTRACE_POKEUSER: %d (%s)\n", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "[+]attached to pid(%ld)\n", pid);

  // DR7 に必要な情報を書き込む
  dr7_t dr7 = {0};
  dr7.l0 = 1;   // local enable for braekpoint in DR0
  dr7.rw0 = DR7_BREAK_ON_EXEC;
  dr7.len0 = DR7_LEN_1;   // breakpoint is 4 bytes long 
  if (ptrace(PTRACE_POKEUSER, pid, offsetof(struct user, u_debugreg[7]), dr7) != 0) {
    fprintf(stderr, "[-]PTRACE_POKEUSER2: %d (%s)\n", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Breakpoint を書き込んだので CONT
  // DETACH すると TRACEME が消える
  if (ptrace(PTRACE_CONT, pid, NULL, NULL) != 0) {
    fprintf(stderr, "[-]PTRACE_CONT: %d (%s)\n", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  waitpid(pid, &status, 0);  // 子プロセスがBREAKするのまつ
  if (!WIFSTOPPED(status)) {
    fprintf(stderr, "[-]Child process may be exited\n");
    exit(EXIT_FAILURE);
  }

  printf("[+]BREAK!\n");
  fgetc(stdin);

  // DETACH して子プロセスが終了するまで待つ
  if (ptrace(PTRACE_DETACH, pid, NULL, NULL) != 0) {
    fprintf(stderr, "[-]PTRACE_DETACH: %d (%s)\n", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }
  waitpid(pid, &status, 0);  // 子プロセスがBREAKするのまつ
  printf("[+]Debugger Exit\n");


  return 0;
}
