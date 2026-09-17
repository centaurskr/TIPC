///
/// @brief : 리더 하트비트 감시자(장애 감지) — 이름은 tipcGsnd2 이지만
///          tipcGsnd.c/tipcGrcv.c 의 그룹(GROUP_TYPE=2000)과는 무관한 별개의
///          독립 프로그램이다. Group/디렉토리에 있을 뿐, 그룹에 join하지 않고
///          단순 이름 {NODE_MST_TYPE, 0} 으로 bind()만 하여 리더(마스터) 노드가
///          주기적으로 보내는 "heartbeat" 데이터그램을 기다린다.
/// @file  : tipcGsnd2.c
///
/// 이 저장소에는 "heartbeat" 문자열을 실제로 보내는 리더/마스터 측 송신
/// 프로그램이 포함되어 있지 않다. 테스트 방법은 README.md 참고(파이썬
/// socket 모듈(AF_TIPC)로 간단히 heartbeat를 보내 검증 완료).
///
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <linux/tipc.h>
#include <time.h>

#define NODE_MST_TYPE 6000  // 리더 하트비트를 받을 서비스 이름(type). instance는 0 고정.
#define TIMEOUT_SEC 10      // 이 시간(초) 동안 하트비트가 없으면 리더 사망으로 판단
#define BUF_SIZE 256

int main() {
    int sock;
    struct sockaddr_tipc addr;
    char buf[BUF_SIZE];
    time_t last_heartbeat;
    bool leader_alive = false;

    sock = socket(AF_TIPC, SOCK_RDM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.family = AF_TIPC;
    addr.addrtype = TIPC_ADDR_NAME;
    addr.addr.name.name.type = NODE_MST_TYPE;
    addr.addr.name.name.instance = 0;
    addr.addr.name.domain = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Node-MST: Waiting for heartbeat...\n");
    last_heartbeat = time(NULL);

    // 1초 주기로 select() 타임아웃을 걸어 소켓을 폴링하면서, 별도로 마지막
    // 하트비트 수신 이후 경과 시간을 직접 누적 검사한다(recv()를 블로킹으로
    // 두면 하트비트가 끊겼는지 능동적으로 검사할 수 없기 때문).
    while (1) {
        fd_set readfds;
        struct timeval timeout;
        int maxfd = sock + 1;

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(maxfd, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select");
            break;
        }

        if (activity > 0 && FD_ISSET(sock, &readfds)) {
            ssize_t len = recv(sock, buf, sizeof(buf) - 1, 0);
            if (len > 0) {
                buf[len] = '\0';
                if (strcmp(buf, "heartbeat") == 0) {
                    printf("Received heartbeat.\n");
                    last_heartbeat = time(NULL);
                    leader_alive = true;
                }
            }
        }

        // 최초 하트비트를 한 번이라도 받은 뒤(leader_alive==true)에만 타임아웃을
        // 검사한다 — 그래야 프로그램 시작 직후 아직 아무 것도 못 받은 상태를
        // "리더 사망"으로 잘못 판단하지 않는다.
        time_t now = time(NULL);
        if (leader_alive && (now - last_heartbeat) > TIMEOUT_SEC) {
            printf("No heartbeat detected! Leader is dead. Trigger failover!\n");
            leader_alive = false;
            // 여기서 새로운 리더 선출을 위한 추가 액션을 할 수 있음
        }
    }

    close(sock);
    return 0;
}

