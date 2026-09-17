///
/// @brief   TEST TIPC
/// @file    tipcRRrcv.c
/// @date    2025. 04. 17. (목) 16:45:20 KST
/// @author  Cento
///
/// 이 프로그램을 동일한 {SERVICE_TYPE, INSTANCE} 이름으로 여러 인스턴스 실행해
/// 두면, 이름 기반 부하분산(round-robin) 테스트가 된다. SOCK_RDM 소켓 여러 개가
/// 같은 이름에 bind() 하면 TIPC 커널이 이름 테이블에 다중 바인딩으로 등록하고,
/// 그 이름으로 들어오는 sendto()를 인스턴스들에 순환 분배한다 — 실제 확인 결과
/// (본 저장소 README.md 참고) 순서대로 A, B, A, B ... 처럼 번갈아 수신된다.
///
/// !! 주의 !!: 이 파일의 bind 값 {SERVICE_TYPE=18888, INSTANCE=50} 은
/// tipcRRsnd.c 의 목적지 값 {type=30000, instance=1} 과 일치하지 않는다. 이
/// 상태로 함께 실행하면 송신측 sendto()가 실패한다 — README.md 참고.
///
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/tipc.h>

#define BUFFER_SIZE   1024  // Message buffer size

#define SERVICE_TYPE 18888  // tipcRRsnd.c 의 목적지 type 과 맞춰야 함
#define INSTANCE     50     // tipcRRsnd.c 의 목적지 instance 와 맞춰야 함

int main() {
    int sock;
    struct sockaddr_tipc addr;
    char buffer[BUFFER_SIZE];

    // Create TIPC socket
    sock = socket(AF_TIPC, SOCK_RDM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    // Bind the socket to a service type and instance range
    addr.family                  = AF_TIPC;
    addr.addrtype                = TIPC_SERVICE_ADDR;
    addr.addr.name.name.type     = SERVICE_TYPE;  // 18888
    addr.addr.name.name.instance = INSTANCE;  // 50
	addr.addr.name.domain        = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
		close(sock);
        exit(1);
    }

    printf("listening on service type %d Instance %d\n",
		SERVICE_TYPE, INSTANCE);

    // Server loop: wait for client messages and respond
    while (1) {
        struct sockaddr_tipc client_addr;
        socklen_t client_len = sizeof(client_addr);
        int received = recvfrom(sock, buffer, BUFFER_SIZE, 0,
					(struct sockaddr *)&client_addr, &client_len);

        if (received < 0) {
            perror("Receive failed");
			continue;
        }
        buffer[received] = '\0';  // Null-terminate the message
        printf("Received message: %s\n", buffer);
        // 참고: 송신측(tipcRRsnd)이 bind() 없이 sendto()만 하는 익명 소켓이므로,
        // 아래에 찍히는 값은 실제 서비스 이름이 아니라 커널이 부여한 임시 포트
        // 참조값이 name.type/name.instance 필드 형태로 재해석된 것이다.
		printf("\tfrom <%u,%u,%u>\n\n",
				client_addr.addr.name.name.type,
				client_addr.addr.name.name.instance,
				client_addr.addr.name.domain);
    }

    close(sock);
    return 0;
}

