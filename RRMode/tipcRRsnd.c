///
/// @brief  : TIPC ROUND-ROBIN Sender
/// @file   : tipcRRsnd.c
/// @date   : 2025. 04. 17. (목) 17:10:53 KST
/// @author : Cento
///
/// SOCK_RDM(비연결형 데이터그램) 소켓으로 같은 서비스 이름({type, instance})에
/// sendto()를 반복한다. 이 프로그램 자체는 라운드로빈 로직을 갖고 있지 않다 —
/// "라운드로빈"은 수신측(tipcRRrcv)을 동일한 이름으로 여러 인스턴스 bind() 해둘
/// 때 TIPC 커널의 이름 테이블이 매 sendto() 호출마다 그중 하나를 순환 선택해
/// 배분해 주는 동작을 가리킨다(RDM/tipc_rdm_snd.c 의 서비스 트레이스로 이 선택을
/// 직접 관찰할 수 있다).
///
/// !! 주의 !!: 이 파일의 목적지 {type=30000, instance=1} 은 tipcRRrcv.c 의
/// bind 값 {SERVICE_TYPE=18888, INSTANCE=50} 과 일치하지 않는다. 이 상태로
/// 함께 실행하면 sendto()가 "No route to host"로 실패한다(실제 확인됨). 테스트
/// 전에 두 파일 중 하나의 값을 상대와 동일하게 맞춰야 한다 — 자세한 내용은
/// README.md 참고.
///
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/tipc.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_TIPC, SOCK_RDM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // bind() 없이 sendto()만 하는 "익명" 발신 소켓 — 이 소켓에는 이름이
    // 등록되지 않으므로, 수신측이 recvfrom()으로 받는 발신자 주소는
    // 실제 서비스 이름이 아니라 커널이 부여한 임시 포트 참조값이다.
    struct sockaddr_tipc addr = {
        .family                  = AF_TIPC,
        .addrtype                = TIPC_SERVICE_ADDR ,
        .addr.name.name.type     = 30000,   // tipcRRrcv.c 의 SERVICE_TYPE 과 맞춰야 함
        .addr.name.name.instance = 1,       // tipcRRrcv.c 의 INSTANCE 와 맞춰야 함
        .addr.name.domain        = 0
    };

	for (int i = 0; i < 100; i++){
    	char msg[1024];
		memset(msg, 0x00, 1024);
 		sprintf(msg, "Hello from Node A - %3d", i);
    	if (sendto(sock, msg, strlen(msg) + 1, 0,
               (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        	perror("Send failed");
			break;
    	}

    	printf("Sent: %s\n", msg);
		//usleep(5000);
		sleep(5);
	}
    close(sock);
    return 0;
}
