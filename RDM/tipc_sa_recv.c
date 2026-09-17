//
// TIPC RDM Receiver
//
// SOCK_RDM 소켓을 TIPC_SERVICE_ADDR(단일 인스턴스)로 bind()하여 데이터
// 메시지를 받는 동시에, TipcServiceTrace()(toptrace.c)로 같은 서비스
// type에 대한 토폴로지 이벤트(이름 등록/해제)까지 poll() 하나로 함께
// 감시하는 데모. 짝이 되는 송신자는 RDM/tipc_rdm_snd.c 이며, 실행 시
// 인자로 준 <service type>/<instance> 를 그대로 맞춰 실행하면 된다.
// bind 범위가 정확히 인스턴스 1개뿐이라는 점이 tipc_sr_recv.c(범위 bind)
// 와의 차이다.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/tipc.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <signal.h>

int RUN = 1;

extern void DumpTipcSocketAddr  (int , char *, struct tipc_socket_addr *  );
extern void DumpTipcServiceAddr (int , char *, struct tipc_service_addr * );
extern void DumpTipcServiceRange(int , char *, struct tipc_service_range *);
extern void DumpTipcSubscr      (int , char *, struct tipc_subscr *       );
extern void DumpSockAddrTipc    (int , char *, struct sockaddr_tipc *     );
extern void DumpTipcEvent       (int , char *, struct tipc_event *        );
extern void DumpSimpleTipcEvent (int , char *, struct tipc_event *        );
extern char *TipcLinkname       (struct tipc_socket_addr *, char *        );
extern int  TipcServiceTrace    (uint32_t , int                           );

#define SERVICE_TYPE 30000  // 사용되지 않는 매크로(실제 서비스 type은 argv[1]로 받음)

// SIGINT/SIGTERM 등 수신 시 메인 poll 루프를 정상 종료시키기 위한 플래그 설정.
// (SIGKILL/SIGSEGV는 프로세스 차원에서 잡히지 않거나 핸들러 등록이 의미 없는
// 시그널이라 signal()이 실질적 효과를 내지 못할 수 있음)
void SignalHandler(int signum)
{
	printf("signal : %d\n", signum);
	RUN = 0;
}
void Using(char *pname){
	printf("TIPC RDM Mesasge receiver\n");
	printf("%s <service type> <instance> <top type>\n",pname);
	printf("\tservice type : (0~63 예약)\n");
	printf("\tintance      : 숫자\n");
	printf("\ttop type     :\n");
	printf("\t\t1 : TIPC_SUB_PORT(all event)\n");
	printf("\t\t2 : TIPC_SUB_SERVICE\n");
	printf("\t\t3 : TIPC_SUB_CANCEL\n");
}
int main(int argc, char **argv)
{
int sock, sfd, rtn, top, service, instance;
struct sockaddr_tipc addr;
char buff[1024];
	if (argc != 4){
		Using(argv[0]);
		exit(0);
	}
	top = atoi(argv[3]);	
	if (top < 1 || top > 3){
		Using(argv[0]);
		exit(0);
	}
	service  = atoi(argv[1]);
	instance = atoi(argv[2]);

	signal(SIGKILL, SignalHandler);
	signal(SIGINT, SignalHandler);
	signal(SIGSEGV, SignalHandler);
	signal(SIGTERM, SignalHandler);

	// create RDM socket 
	sock = socket(AF_TIPC, SOCK_RDM, 0);
	if (sock < 0 ){
		perror("socket : ");
		exit(0);
	}
	// bind() : 단일 인스턴스 이름 {service, instance} 에만 bind (범위 bind가
	// 아니므로 정확히 이 instance로 보낸 메시지만 이 소켓이 받는다).
	addr.family                  = AF_TIPC          ;
	addr.addrtype                = TIPC_SERVICE_ADDR;
	addr.addr.name.name.type     = service          ;
	addr.addr.name.name.instance = instance         ;
	addr.addr.name.domain        = 0                ;// "default" Cluster

	rtn = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (rtn ){
		perror("bind() : ");
		exit(0);
	}

	// Service Trace : 위에서 bind()한 것과 별개로, 같은 service type 전체에
	// 대한 이름 등록/해제 이벤트를 받을 두 번째 fd를 연다(데이터 채널과
	// 이벤트 채널이 서로 다른 소켓).
	sfd = TipcServiceTrace(service, top);
	if (sfd < 0){
		perror("TipcServiceTrace() :");
		exit(0);
	}
	printf("START----------------\n");
	// poll : 데이터 소켓(pfd[0])과 토폴로지 이벤트 소켓(pfd[1])을 하나의
	// poll()로 동시에 감시한다.
	struct pollfd pfd[3];
	pfd[0].fd = sock;
	pfd[0].events = POLLIN | POLLHUP;

	pfd[1].fd = sfd;
	pfd[1].events = POLLIN | POLLHUP;
	while(RUN){
		rtn = poll(pfd, 2, 30000);
		if (rtn < 0) break;
		if (!rtn ) continue;               // 30초 타임아웃 -> 그냥 다시 poll
		if (pfd[1].revents){
			// 토폴로지 이벤트 도착: 누군가 이 service type 이름을
			// 등록(bind)/해제(close) 했다는 알림.
			struct tipc_event ent;
			rtn = recv(pfd[1].fd, &ent, sizeof(ent), 0);
			if (rtn != sizeof(ent)) break;
			//DumpTipcEvent(0, "TIPC_EVENT", &ent);
			DumpSimpleTipcEvent(1, "TIPC_EVENT", &ent);
		}
		if (pfd[0].revents){
			// 실제 데이터 메시지 도착.
			struct sockaddr_tipc cli;
			socklen_t cli_len = sizeof(cli);
			int len = recvfrom(pfd[0].fd, buff, 1024, 0,
					(struct sockaddr *)&cli, &cli_len);
			if (len < 0){
				printf("Receive fail\n");
				continue;
			}
			buff[len] = 0x00;
			// SOCK_RDM 발신자 주소는 항상 TIPC_ADDR_ID 이므로 cli.addr.id.node
			// 를 읽는 것이 맞다(Group/tipcGrcv.c 와 달리 여기서는 올바르게
			// id 필드를 사용하고 있음).
			printf("MESSAGE from node(%u): %s\n", cli.addr.id.node, buff);

		}
	}
	shutdown(sfd, TIPC_CONN_SHUTDOWN);
	shutdown(sock, TIPC_CONN_SHUTDOWN);
	close(sfd);
	close(sock);
}
