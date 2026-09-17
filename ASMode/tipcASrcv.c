///
/// @brief  : TIPC Receiver (Active Standbt Test)
/// @file   : tipcASrcv.c
/// @date   : 2025. 04. 21. (월) 14:03:40 KST
/// @author : Cento
///
/// 동일한 {SERVICE_TYPE, INSTANCE} 이름으로 이 프로그램을 여러 인스턴스(우선순위만
/// 다르게) 실행해두면 Active-Standby 구성이 된다. tipcASsnd는 그중 살아있는
/// 인스턴스에 연결되며, 어떤 인스턴스가 응답할지는 프로그램이 직접 정하는 것이
/// 아니라 TIPC 커널의 이름 테이블 조회 결과에 따른다. --priority 인자는
/// TIPC_IMPORTANCE 소켓 옵션으로 전달되는데, 이는 실제로는 "연결 우선순위"가
/// 아니라 링크 혼잡(congestion) 시 이 소켓이 보내는 메시지를 얼마나 먼저
/// 버릴지를 정하는 값(낮을수록 먼저 버려짐)이다. 즉 이 데모에서 --priority는
/// 인스턴스를 구분하기 위한 라벨 값으로 쓰이고 있으며, TIPC가 접속 요청을 어느
/// 인스턴스로 보낼지 선택하는 데는 관여하지 않는다.
///
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/tipc.h>

#define SERVICE_TYPE 18888  // tipcASsnd.c 의 SERVICE_TYPE 과 반드시 동일해야 함
#define INSTANCE     1      // tipcASsnd.c 의 INSTANCE 와 반드시 동일해야 함
#define BUF_SIZE     256

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <priority: 0-3>\n", argv[0]);
        return 1;
    }

    int priority = atoi(argv[1]); // 0 = highest, 3 = lowest (인스턴스 구분용 라벨)
    if (priority < 0 || priority > 3) {
        fprintf(stderr, "Priority must be between 0 and 3\n");
        return 1;
    }

    int listen_sock = socket(AF_TIPC, SOCK_SEQPACKET, 0);
    if (listen_sock < 0) die("socket");

    // TIPC_IMPORTANCE: 이 소켓에서 보내는 메시지의 혼잡 시 폐기 우선순위를 설정.
    // (TIPC_LOW_IMPORTANCE=0 ~ TIPC_CRITICAL_IMPORTANCE=3)
    if (setsockopt(listen_sock, SOL_TIPC, TIPC_IMPORTANCE, &priority, sizeof(priority)) < 0)
        die("setsockopt");

    // TIPC_ADDR_NAMESEQ + lower==upper==INSTANCE : 단일 인스턴스 번호를 범위(range)
    // 형태로 바인딩. 여러 프로세스가 동일한 {type, lower, upper} 로 bind() 할 수 있고,
    // 이 경우 TIPC 커널의 이름 테이블에 다중 바인딩(멀티캐스트 그룹과 유사)으로 등록되어
    // Active-Standby/부하분산 구성이 가능해진다.
    struct sockaddr_tipc addr = {
        .family = AF_TIPC,
        .addrtype = TIPC_ADDR_NAMESEQ,
        .addr.nameseq.type = SERVICE_TYPE,
        .addr.nameseq.lower = INSTANCE,
        .addr.nameseq.upper = INSTANCE
    };

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("bind");

    if (listen(listen_sock, 5) < 0)
        die("listen");

    printf("Receiver started with priority %d\n", priority);

    // 접속 수락 -> 수신 루프 -> 접속 종료 를 반복하는 서버 루프.
    // 한 번에 하나의 접속만 처리한다(accept()가 블로킹이므로 동시 다중 접속은
    // 처리하지 않음 — listen() 백로그에만 대기).
    while (1) {
        int client_sock = accept(listen_sock, NULL, 0);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        printf("Connection accepted\n");

        char buf[BUF_SIZE];
        ssize_t len;
        while ((len = recv(client_sock, buf, BUF_SIZE - 1, 0)) > 0) {
            buf[len] = '\0';
            printf("Received: %s\n", buf);
        }

        // len == 0 : 송신측이 정상 종료(EOF).
        // len <  0 : tipcASsnd가 send() 직후 곧바로 close()하기 때문에 흔히
        //            ECONNRESET("Connection reset by peer")이 발생한다.
        //            메시지 자체는 이미 위 루프에서 정상 수신된 뒤이므로 무시해도 된다.
        if (len < 0)
            perror("recv");

        close(client_sock);
        printf("Connection closed\n");
    }

    close(listen_sock);
    return 0;
}

