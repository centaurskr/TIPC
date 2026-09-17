///
/// @brief  : Create TIPC Event Trace fd
/// @file   : toptrace.c
/// @date   : 2025. 05. 01. (목) 10:37:06 KST
/// @author : Cento
///
/// TIPC 커널이 내장 제공하는 "토폴로지 서버(Topology Server, TIPC_TOP_SRV)"에
/// 접속하여 특정 서비스 type(0~~2^32-1 instance 전체 범위)에 대한 PUBLISH/
/// WITHDRAW(=이름 등록/해제, 즉 어떤 프로세스가 그 이름으로 bind/close 하는
/// 시점) 이벤트를 구독(subscribe)한다. 반환되는 fd를 recv()하면 매 이벤트마다
/// struct tipc_event 1건이 도착한다 — tipc_sa_recv.c/tipc_sr_recv.c/
/// tipc_rdm_snd.c 가 이 fd를 poll()로 감시해 실시간으로 이름 등록/해제를
/// 관찰하는 데 사용한다(tipcdump.c 의 Dump*TipcEvent 로 출력).
///
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <linux/tipc.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
///
/// TIPC Service trade fd
///
/// @param service 감시할 서비스 이름의 type. instance 는 0~~2^32-1(=~0) 전체
///                범위로 구독하므로, 이 type 아래의 모든 instance에 대한
///                이벤트를 다 받는다(특정 instance만 골라 받는 필터는 없음).
/// @param top     1=TIPC_SUB_PORTS(모든 이벤트), 2=TIPC_SUB_SERVICE(등록/해제만),
///                그 외=TIPC_SUB_CANCEL(구독 취소)
/// @return 이벤트를 recv()로 읽어들일 소켓 fd, 실패 시 -1(토폴로지 서버 접속
///         실패) 또는 -2(구독 메시지 전송 실패)
///
int TipcServiceTrace(uint32_t service , int top){
int sfd, rtn;
struct sockaddr_tipc addr;
	sfd = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	addr.family                  = AF_TIPC;
    addr.addrtype                = TIPC_SERVICE_ADDR;
    addr.addr.name.name.type     = TIPC_TOP_SRV;      // 커널이 항상 제공하는 예약 서비스
    addr.addr.name.name.instance = TIPC_TOP_SRV;
    addr.addr.name.domain        = 0;
    rtn = connect(sfd, (struct sockaddr*)&addr, sizeof(addr));
	if (rtn) return -1;

	struct tipc_subscr subscr;

	subscr.seq.type  = service;
	subscr.seq.lower = 0;
	subscr.seq.upper = ~0;
	subscr.timeout   = TIPC_WAIT_FOREVER;
	if (top == 1){
		// 모든 event 감시
		subscr.filter    = TIPC_SUB_PORTS;
	}else if (top == 2){
		// TIPC_PUBLISH or TIPC_WITHDRAW event 감시
		subscr.filter    = TIPC_SUB_SERVICE;
	}else{
		subscr.filter    = TIPC_SUB_CANCEL;
	}

	// 구독 요청(struct tipc_subscr)을 토폴로지 서버로 전송. 이후 이 fd에서
	// recv()하면 매칭되는 이벤트가 struct tipc_event 형태로 도착한다.
	if(send(sfd, &subscr, sizeof(subscr), 0) != sizeof(subscr))
		return -2;

	return sfd;
}

