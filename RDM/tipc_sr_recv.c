//
// TIPC RDM Receiver (Service Range)
//
// tipc_sa_recv.c 와 거의 동일한 구조이지만, bind()에 TIPC_SERVICE_ADDR(단일
// 인스턴스) 대신 TIPC_SERVICE_RANGE(lower~upper 범위)를 사용한다는 점이
// 다르다. 이 범위 안의 어떤 instance로 보낸 메시지든 이 소켓 하나가 받는다
// (RRMode의 "동일 이름에 여러 인스턴스가 bind"와는 다른 개념 — 여기서는
// 한 프로세스가 한 번의 bind로 넓은 instance 대역 전체를 수신하는 것).
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
void SignalHandler(int signum)
{
	printf("signal : %d\n", signum);
	RUN = 0;
}
void Using(char *pname){
	printf("TIPC RDM.Service Range Mesasge receiver\n");
	printf("%s <service type> <lower> <upper> <top type>\n",pname);
	printf("\tservice type : (0~63 예약)\n");
	printf("\tintance      : 숫자\n");
	printf("\ttop type     :\n");
	printf("\t\t1 : TIPC_SUB_PORT(all event)\n");
	printf("\t\t2 : TIPC_SUB_SERVICE\n");
	printf("\t\t3 : TIPC_SUB_CANCEL\n");
}
int main(int argc, char **argv)
{
int sock, sfd, rtn, top, service, lower, upper;
struct sockaddr_tipc addr;
char buff[1024];
	if (argc != 5){
		Using(argv[0]);
		exit(0);
	}
	top = atoi(argv[4]);	
	if (top < 1 || top > 3){
		Using(argv[0]);
		exit(0);
	}
	service  = atoi(argv[1]);
	lower    = atoi(argv[2]);
	upper    = atoi(argv[3]);

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
	// bind() : [lower, upper] instance 범위 전체를 하나의 소켓으로 수신.
	addr.family             = AF_TIPC          ;
	addr.addrtype           = TIPC_SERVICE_RANGE;
	addr.addr.nameseq.type  = service          ;
	addr.addr.nameseq.lower = lower            ;
	addr.addr.nameseq.upper = upper            ;

	rtn = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (rtn ){
		perror("bind() : ");
		exit(0);
	}

	// Service Trace : 데이터 채널(sock)과 별개로 이름 등록/해제 이벤트를
	// 받을 두 번째 fd를 연다.
	sfd = TipcServiceTrace(service, top);
	if (sfd < 0){
		perror("TipcServiceTrace() :");
		exit(0);
	}
	printf("START--ServiceRange-Lower(%d) upper(%d)-\n", lower, upper);
	// poll : 데이터 소켓(pfd[0])과 토폴로지 이벤트 소켓(pfd[1])을 함께 감시.
	struct pollfd pfd[3];
	pfd[0].fd = sock;
	pfd[0].events = POLLIN | POLLHUP;

	pfd[1].fd = sfd;
	pfd[1].events = POLLIN | POLLHUP;
	while(RUN){
		rtn = poll(pfd, 2, 30000);
		if (rtn < 0) break;
		if (!rtn ) continue;             // 30초 타임아웃 -> 다시 poll
		if (pfd[1].revents){
			struct tipc_event ent;
			rtn = recv(pfd[1].fd, &ent, sizeof(ent), 0);
			if (rtn != sizeof(ent)) break;
			//DumpTipcEvent(0, "TIPC_EVENT", &ent);
			DumpSimpleTipcEvent(1, "TIPC_EVENT", &ent);
		}
		if (pfd[0].revents){
			struct sockaddr_tipc cli;
			socklen_t cli_len = sizeof(cli);
			int len = recvfrom(pfd[0].fd, buff, 1024, 0,
					(struct sockaddr *)&cli, &cli_len);
			if (len < 0){
				printf("Receive fail\n");
				continue;
			}
			buff[len] = 0x00;
			// ref 값의 상위/하위 16비트를 각각 remote/local bearer id로
			// 나눠 출력한다(tipcdump.c의 DumpTipcSocketAddr과 동일한 해석).
			printf("from node(%u)ref(%u):상16(%d,0x%x)하16(%d,0x%x)\n\t%s\n",
				cli.addr.id.node, cli.addr.id.ref,
				(cli.addr.id.ref >> 16) & 0xffff,
				(cli.addr.id.ref >> 16) & 0xffff,
				cli.addr.id.ref & 0xffff,
				cli.addr.id.ref & 0xffff,
				buff);
		}
	}
	shutdown(sfd, TIPC_CONN_SHUTDOWN);
	shutdown(sock, TIPC_CONN_SHUTDOWN);
	close(sfd);
	close(sock);
}
