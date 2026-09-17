///
/// @brief  : TIPC ROUND-ROBIN Sender
/// @file   : tipc_rdm_snd.c
/// @date   : 2025. 04. 30. (수) 13:57:57 KST
/// @author : Cento
///
/// tipc_sa_recv.c / tipc_sr_recv.c 의 짝이 되는 송신자. 두 개의 동작을 동시에
/// 수행한다.
///   1) 메인 스레드: {service, instance} 로 1초 간격 최대 1000건 sendto().
///      여러 수신자가 같은 이름(또는 같은 범위)에 bind되어 있으면 RRMode와
///      마찬가지로 커널이 그중 하나에 순환/부하분산 배분한다.
///   2) 백그라운드 스레드(MonitorThread): 같은 service type 전체에 대한
///      토폴로지 이벤트(누가 그 type으로 bind/close 하는지)를 실시간으로
///      구독해 화면에 출력한다 — 테스트 중 수신자를 껐다 켜는 것을 눈으로
///      확인하기 위한 디버깅 보조 기능.
///
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <linux/tipc.h>
#include <sys/socket.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

extern void DumpTipcSocketAddr  (int , char *, struct tipc_socket_addr *  );
extern void DumpTipcServiceAddr (int , char *, struct tipc_service_addr * );
extern void DumpTipcServiceRange(int , char *, struct tipc_service_range *);
extern void DumpTipcSubscr      (int , char *, struct tipc_subscr *       );
extern void DumpSockAddrTipc    (int , char *, struct sockaddr_tipc *     );
extern void DumpTipcEvent       (int , char *, struct tipc_event *        );
extern void DumpSimpleTipcEvent (int , char *, struct tipc_event *        );
extern int  TipcServiceTrace    (uint32_t , int                           );

// 별도 스레드에서 실행되어, main()이 메시지를 보내는 동안 병행으로 해당
// service type의 이름 등록/해제 이벤트를 계속 출력한다. poll() 타임아웃
// 3,000,000ms(=3000초, 약 50분)는 사실상 "거의 무한정 대기"에 가깝게
// 설정해둔 값이다.
void *MonitorThread(void *arg){
int sfd, rtn, service;
struct pollfd pfd[2];
struct tipc_event ent;

	service = *((int *)arg);
	sfd = TipcServiceTrace((uint32_t)service , 1); // all
	printf("TIPC Monitor(all) : service (%d), socket fd(%d)\n", service, sfd);
	pfd[0].fd     = sfd;
	pfd[0].events = POLLIN | POLLHUP;
	while(1){
		rtn = poll(pfd, 1, 3000000);
		if (rtn <= 0){
			printf("Poll rtn(%d), continue \n", rtn);
			continue;
		}
		rtn = recv(pfd[0].fd, &ent, sizeof(ent), 0);
		DumpSimpleTipcEvent(1, "TIPC_EVENT", &ent);
	}
	shutdown(sfd, TIPC_CONN_SHUTDOWN);
	close(sfd);
	pthread_exit(0);
}

void Using(char *pname){
    printf("TIPC RDM Mesasge sender\n");
    printf("%s <service type> <instance>\n",pname);
    printf("\tservice type : (0~63 예약)\n");
    printf("\tintance      : 숫자\n");
}
int main(int argc, char **argv) {
	if(argc != 3){
		Using(argv[0]);
		exit(0);
	}

	int service, instance;

	service = atoi(argv[1]);
	instance = atoi(argv[2]);

    int sock = socket(AF_TIPC, SOCK_RDM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_tipc addr = {
        .family                  = AF_TIPC,
        .addrtype                = TIPC_SERVICE_ADDR ,
        .addr.name.name.type     = service,
        .addr.name.name.instance = instance,
        .addr.name.domain        = 0
    };
	struct sockaddr_tipc tipcaddr;
	// !! 확인된 이슈 !!: sz가 초기화되지 않은 채 getsockname()의 in/out
	// addrlen 인자로 쓰이고 있다(정의되지 않은 동작). 실제로는 매번
	// sizeof(tipcaddr) 이상의 우연한 스택 값이 들어있어 지금까지는 정상
	// 동작한 것으로 보이나, 정석대로라면 `int sz = sizeof(tipcaddr);` 로
	// 초기화해야 한다.
	int    sz;
	getsockname(sock, (struct sockaddr *)&tipcaddr, &sz);
	// 이 소켓은 bind()를 하지 않았으므로 tipcaddr에는 커널이 부여한
	// ID(node/ref)만 유효하다. DumpSockAddrTipc가 함께 찍는 NAMESEQ/NAME
	// 블록은 같은 raw 바이트를 다른 필드로 잘못 재해석한 무의미한 값이다.
	DumpSockAddrTipc(0, "Sock name", &tipcaddr);

	pthread_t threads;

	// service 변수는 스택에 있으므로 스레드 인자로 그 주소를 넘길 때 각별히
	// 주의해야 하지만, MonitorThread가 즉시 값만 복사해 쓰고 main()도 이후
	// service를 변경하지 않으므로 이 경우엔 안전하다.
	pthread_create(&threads, NULL, MonitorThread, (void *)&service);


	for (int i = 0; i < 1000; i++){
    	char msg[1024];
		memset(msg, 0x00, 1024);
 		sprintf(msg, "Hello from Node A - %3d {%d,%d}", i, service, instance);
    	if (sendto(sock, msg, strlen(msg) + 1, 0,
               (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        	perror("Send failed");
			break;
    	}

    	printf("Sent: %s\n", msg);
		//usleep(5000);
		sleep(1);
	}
	shutdown(sock, TIPC_CONN_SHUTDOWN);
    close(sock);
    return 0;
}
