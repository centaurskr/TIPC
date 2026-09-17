///
/// @brief  : TIPC Sender (Active Standby mode test)
/// @file   : tipcASsnd.c
/// @date   : 2025. 04. 21. (월) 14:02:20 KST
/// @author : Cento
///
/// 이 프로그램은 서비스 이름({SERVICE_TYPE, INSTANCE})만으로 tipcASrcv에 접속한다.
/// 실제로 어떤 물리 노드/프로세스가 그 이름을 서비스하는지는 TIPC 커널이 이름
/// 테이블(name table)을 통해 알아서 찾아준다. 동일한 이름으로 여러 개의
/// tipcASrcv 인스턴스(우선순위가 다른)를 띄워두면, 그 중 하나가 죽어도 송신자는
/// 코드 변경 없이 살아있는 다른 인스턴스로 계속 연결된다 — 이것이 Active-Standby
/// 이중화 테스트의 핵심이다.
///
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/tipc.h>
#include <errno.h>

#define SERVICE_TYPE 18888  // tipcASrcv.c 의 SERVICE_TYPE 과 반드시 동일해야 함
#define INSTANCE     1      // tipcASrcv.c 의 INSTANCE(nameseq lower/upper) 와 반드시 동일해야 함
#define BUF_SIZE     256

///
/// @brief TIPC 이름({SERVICE_TYPE, INSTANCE})으로 접속하는 SEQPACKET 소켓을 생성한다.
///        메시지 1건마다 새로 connect() 하므로, 매 전송 시점의 활성(active) 수신자를
///        다시 탐색하는 셈이다(장애조치가 일어난 직후라도 다음 전송은 새 활성 인스턴스로 감).
/// @return 성공 시 연결된 소켓 fd, 실패 시 -1 (수신 측 서비스가 하나도 떠있지 않으면 실패)
///
int Connect(){
    int sock = socket(AF_TIPC, SOCK_SEQPACKET, 0);  // Must match receiver type
    if (sock < 0) return -1;

    struct sockaddr_tipc dest;
    memset(&dest, 0, sizeof(dest));
    dest.family = AF_TIPC;
    dest.addrtype = TIPC_ADDR_NAME;             // 단일 목적지 이름으로 주소 지정
    dest.addr.name.name.type = SERVICE_TYPE;
    dest.addr.name.name.instance = INSTANCE;
    dest.addr.name.domain = 0;                  // 0 = 도메인 제한 없음(클러스터 전체 탐색)

    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) < 0)
        return -1;
    return sock;
}

///
/// @brief stdin으로 한 줄씩 메시지를 입력받아, 그때마다 새로 접속(Connect)하여
///        1건 전송 후 바로 접속을 끊는다. connect() 가 실패하면(=서비스 이름을
///        제공하는 수신자가 하나도 없으면) 루프를 종료한다.
///
int main() {
int sock;
    char msg[256];
    while (1) {
        printf("Enter message to send: ");
        if (!fgets(msg, sizeof(msg), stdin)) break;

        sock = Connect();
        if (sock < 0) break;               // 살아있는 수신자가 없음 -> 종료
        size_t len = strlen(msg);
        if (msg[len - 1] == '\n') msg[len - 1] = '\0';

        if (send(sock, msg, strlen(msg), 0) < 0){
			exit(0);
		}

        // 전송 직후 바로 close() 하기 때문에, 수신측(tipcASrcv)의 다음 recv()는
        // 정상 EOF(0) 대신 ECONNRESET("Connection reset by peer")을 받을 수 있다.
        // 이는 메시지 자체는 정상 수신된 뒤 발생하는 것으로, 이 테스트에서는 정상 동작이다.
        printf("Message sent and colse socket\n");
        close(sock);
    }

    return 0;
}
