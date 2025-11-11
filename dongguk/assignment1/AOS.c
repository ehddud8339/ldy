#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

int global_data = 24;

static volatile sig_atomic_t exiting = 0;
static void sig_int(int signo) {
  exiting = 1;
}

// Stack 주소가 변하는 것을 보여주기 위한 재귀 함수
void interactive_stack(int depth) {
    volatile char local[4096];

    while (!exiting) { 
        void *ptr = malloc(4096);
        if (!ptr) {
            fprintf(stderr, "failed to malloc\n");
            return;
        }
        printf("location of stack : %p\n", (void*)&local);
        printf("location of heap  : %p\n", ptr);

        int ch = getchar();
        if (ch == EOF) return;
        if (ch != '\n') continue;

        interactive_stack(depth + 1);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT,  sig_int);
    signal(SIGTERM, sig_int);

    // main(code)과 글로벌 데이터(data)의 주소 출력
    printf("location of code  : %p\n", (void *)&main);
    printf("location of data  : %p\n", (void *)&global_data);

    interactive_stack(0); 
    return 0;
}
