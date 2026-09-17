///
/// @brief : TIPC 주소/이벤트 구조체를 사람이 읽기 좋은 형태로 출력하는
///          디버그용 Dump* 함수 모음. tipc_sa_recv.c/tipc_sr_recv.c/
///          tipc_rdm_snd.c 가 extern 선언으로 가져다 쓴다(별도 헤더 없음).
/// @file  : tipcdump.c
///
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/tipc.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>

// level 만큼 탭 문자를 채운 문자열을 만든다(중첩 Dump 출력 들여쓰기용).
void _makeTabStr(int level, char *tabstr)
{
	memset(tabstr, 0x00, 32);
	for(int i =0 ; i < level; i++)
		strcat(tabstr, "\t");
}
void DumpTipcSocketAddr(int lv, char *msg, struct tipc_socket_addr *ts)
{
char tab[32];
	_makeTabStr(lv, tab);
	printf("%s%s tipc_socket_addr------\n", tab, msg);
	printf("%s ref(%u), node(%u)\n", tab, ts->ref, ts->node);
	printf("%s\tlocal_bearerid(%d) remote_bearerid(%d)\n",tab,
		ts->ref & 0xffff, (ts->ref >> 16) & 0xffff);
	printf("%s-%s-end------------------\n", tab, msg);
}
void DumpTipcServiceAddr(int lv, char *msg, struct tipc_service_addr *ts)
{
char tab[32];
	_makeTabStr(lv, tab);
	printf("%s%s tipc_service_addr------\n", tab, msg);
	printf("%s type(%u), instance(%u)\n", tab, ts->type, ts->instance);
	printf("%s-%s-end-----------------\n",tab, msg);
}
void DumpTipcServiceRange(int lv, char *msg, struct tipc_service_range *ts)
{
char tab[32];
	_makeTabStr(lv, tab);
	printf("%s%s tipc_service_range-------\n", tab, msg);
	printf("%s\ttype(%u), lower(%u), upper(%u)\n", 
		tab, ts->type, ts->lower, ts->upper);
	printf("%s-%s-end--------------------\n", tab, msg);
}
void DumpTipcSubscr(int lv, char *msg, struct tipc_subscr *ts)
{
char tab[32];
	_makeTabStr(lv, tab);

	printf("%s%s-tipc_subscr------------\n", tab, msg);
	DumpTipcServiceRange(lv+1, "SEQ", &ts->seq);
	printf("%s\ttimeout(%u), filter(%u)\n", tab, ts->timeout, ts->filter);
	if(ts->filter & TIPC_SUB_PORTS) printf("%s\t\tTIPC_SUB_PORT\n", tab);
	if(ts->filter & TIPC_SUB_SERVICE) printf("%s\t\tTIPC_SUB_SERVICE\n", tab);
	if(ts->filter & TIPC_SUB_CANCEL) printf("%s\t\tTIPC_SUB_CANCEL\n", tab);

	printf("%s\tusr_handle :\n", tab);
	for(int i =0; i < 8; i++){
		printf("%s\t\t[%d]0x%02x\n", tab, i,ts->usr_handle[i]);
	}
	printf("%s-%s-end--------------\n", tab, msg);
}
// !! 주의 !!: ts->addr 은 union(id / nameseq / name)이므로, 실제로 유효한 필드는
// ts->addrtype(TIPC_ADDR_ID/TIPC_ADDR_NAMESEQ/TIPC_ADDR_NAME) 값 하나에
// 대응되는 것 하나뿐이다. 이 함수는 addrtype을 검사하지 않고 ID/NAMESEQ/NAME
// 세 가지를 전부 출력하므로, 실제로 유효하지 않은 두 블록은 같은 raw 바이트를
// 다른 필드로 재해석한 의미 없는 값이 찍힌다(예: bind()를 하지 않은 소켓의
// getsockname() 결과는 ID만 유효하고 NAMESEQ/NAME 블록은 무시해야 함).
void DumpSockAddrTipc(int lv, char *msg, struct sockaddr_tipc *ts)
{
char tab[32];
	_makeTabStr(lv, tab);
	printf("%s%s-sockaddr_tipc---------------------\n", tab, msg);
	printf("%s\tfamily(%d) addrtype(%d) scope(%d)\n",tab,
		ts->family, ts->addrtype, ts->scope);
	DumpTipcSocketAddr(lv + 2, "ID" , &ts->addr.id);
	DumpTipcServiceRange(lv +2, "NAMESEQ", &ts->addr.nameseq);
	DumpTipcServiceAddr(lv+2, "NAME", &ts->addr.name.name);
	printf("%s\t\tname.domain(%d)\n", tab, ts->addr.name.domain);


}
// 피어(peer)의 tipc_socket_addr(node + ref)로부터, 두 노드를 잇는 TIPC
// 링크(bearer)의 이름 문자열을 SIOCGETLINKNAME ioctl로 조회한다.
// ref의 상위 16비트를 local bearer id로 사용(DumpTipcSocketAddr 참고).
char *TipcLinkname(struct tipc_socket_addr *addr, char *name){
struct tipc_sioc_ln_req req;
int    rtn, fd;
	memset((char *)&req, 0x00, sizeof(req));
	req.peer      = addr->node;
	req.bearer_id = (addr->ref >> 16) & 0xffff; // local
	//req.bearer_id = addr->ref & 0xffff; // local

printf("\t\tTipcLinkname() peer(%u) bearer_id(%u)\n", 
req.peer, req.bearer_id);
	fd = socket(AF_TIPC, SOCK_RDM, 0);
	if (fd < 0 ){
		perror("socket : ");
		return NULL;
	}
	name[0] = 0x00;
	rtn = ioctl(fd, SIOCGETLINKNAME, &req);
	if (rtn < 0){ 
		perror("ioctl");
		close(fd);
		return name;
	}
	strncpy(name, req.linkname, strlen(req.linkname));
	close(fd);
	return name;
}
// toptrace.c(TipcServiceTrace)가 반환한 fd에서 읽은 struct tipc_event 1건을
// 필드별로 상세 출력(내부적으로 TipcLinkname()으로 ioctl까지 수행하므로
// DumpSimpleTipcEvent보다 무겁다).
void DumpTipcEvent(int lv, char *msg, struct tipc_event *ent)
{
char tab[32];
	_makeTabStr(lv, tab);
	printf("%s%s-tipc_event----------------\n", tab, msg);
	printf("%sevent(%u), found_lower or neigh_node(%u), found_upper(%u)\n",
		tab, ent->event, ent->found_lower, ent->found_upper);
	if (ent->event == TIPC_PUBLISHED)
		printf("%s\tevent is : TIPC_PUBLISHED\n", tab);
	if (ent->event == TIPC_WITHDRAWN)
		printf("%s\t event is : TIPC_WITHDRAWN\n", tab);
	if (ent->event == TIPC_SUBSCR_TIMEOUT)
		printf("%s\tevent is : TIPC_SUBSCR_TIMEOUT\n", tab);
	DumpTipcSocketAddr(lv +1, "PORT ", &ent->port);
	char name[40];
	memset(name, 0x00, 40);
	TipcLinkname(&ent->port, name);
	printf("%sLinkName is : %s\n", tab, name);

	DumpTipcSubscr(lv+1, "S ", &ent->s);
	printf("%s-%s-end-----------------------\n", tab,msg);
}
// DumpTipcEvent의 한 줄 요약 버전. tipc_sa_recv.c/tipc_sr_recv.c/
// tipc_rdm_snd.c 는 모두 이쪽을 사용한다(ioctl 호출 없이 이벤트 종류와
// 발생 노드/포트 ref만 표시).
void DumpSimpleTipcEvent(int lv, char *msg, struct tipc_event *ent)
{
char tab[32], strevent[32];
	_makeTabStr(lv, tab);
	switch(ent->event){
	case TIPC_PUBLISHED :      strcpy(strevent, "TIPC_PUBLISHED");break;
	case TIPC_WITHDRAWN :      strcpy(strevent, "TIPC_WITHDRAWN");break;
	case TIPC_SUBSCR_TIMEOUT : strcpy(strevent, "TIPC_SUBSCR_TIMEOUT");break;
	}
	printf("%s-%s:%s , node(%u) ref(%u)\n", tab, msg, strevent, 
		ent->port.node, ent->port.ref);
}
