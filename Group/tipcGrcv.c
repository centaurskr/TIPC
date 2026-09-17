///
/// @brief : TIPC Group 수신자 — 그룹 메시지 및 멤버 JOIN/LEAVE 이벤트 수신 데모
/// @file  : tipcGrcv.c
///
/// tipcGsnd.c 와 짝을 이룬다. GROUP_TYPE(2000)은 두 파일이 반드시 동일해야
/// 하고, MEMBER_ID(여기서는 1)는 tipcGsnd.c의 MEMBER_ID(10)와 달라야 한다
/// (tipcGsnd.c 마지막 부분이 instance=1로 이 프로세스에 직접 유니캐스트를
/// 보내기 때문에, 이 값을 바꾸면 그 유니캐스트 데모도 함께 바뀐다).
///
#include <sys/socket.h>
#include <linux/tipc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define GROUP_TYPE 2000  // tipcGsnd.c 의 GROUP_TYPE 과 반드시 동일해야 함
#define MEMBER_ID 1      // 그룹 내 이 프로세스의 멤버 인스턴스 번호
#define MSG_SIZE 64

// TIPC_GROUP_MEMBER_EVTS 로 수신되는 JOIN/LEAVE 이벤트를 출력한다.
// !! 확인된 이슈 !!: 아래 main()의 이벤트 판별 조건
// (sender.addrtype == TIPC_ADDR_ID && sender.addr.id.node == 0) 은 실사용
// 환경에서 참이 되는 경우가 거의 없어(node는 실제 노드 해시값이라 0이 아님)
// 이 함수가 사실상 호출되지 않는다(실제 확인됨). 그룹 멤버 소켓은 일반
// 메시지의 발신자 주소도 항상 TIPC_ADDR_ID(노드/포트 id) 형태로 보고하므로,
// 아래 else 분기가 이름(name.type/instance)으로 잘못 해석해 출력하는 숫자도
// 실제로는 노드/포트 id 값이다.
void handle_event(struct tipc_event *evt) {
    printf("Event: %s {%u,%u} on node 0x%x, ref 0x%x\n",
           evt->event == TIPC_PUBLISHED ? "JOIN" : "LEAVE",
           evt->found_lower, evt->found_upper,
           evt->port.node, evt->port.ref);
}

int main() {
    // Create TIPC socket
    int sock = socket(AF_TIPC, SOCK_RDM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Bind to service address
    struct sockaddr_tipc addr = {
        .family = AF_TIPC,
        .addrtype = TIPC_ADDR_NAME,
        .addr.name.name.type = GROUP_TYPE,
        .addr.name.name.instance = MEMBER_ID,
        .scope = TIPC_CLUSTER_SCOPE
    };
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return 1;
    }

    // Join group with event subscription
    struct tipc_group_req greq = {
        .type = GROUP_TYPE,
        .instance = MEMBER_ID,
        .scope = TIPC_CLUSTER_SCOPE,
        .flags = TIPC_GROUP_LOOPBACK | TIPC_GROUP_MEMBER_EVTS
    };
    if (setsockopt(sock, SOL_TIPC, TIPC_GROUP_JOIN, &greq, sizeof(greq)) < 0) {
        perror("Failed to join group");
        close(sock);
        return 1;
    }
    printf("Joined group {%u,%u}\n", GROUP_TYPE, MEMBER_ID);

    // Main loop to receive messages and events
    char buf[MSG_SIZE];
    struct sockaddr_tipc sender;
    socklen_t alen = sizeof(sender);
    while (1) {
        ssize_t len = recvfrom(sock, buf, MSG_SIZE, 0, (struct sockaddr*)&sender, &alen);
        if (len < 0) {
            perror("Receive failed");
            continue;
        }

        // Check if it's a membership event
        // (위 handle_event() 주석 참고: 이 조건은 거의 항상 거짓이 되어 실질적으로는
        //  모든 수신 메시지가 아래 else 분기로 흘러가는 것이 실제 관찰된 동작이다.)
        if (sender.addrtype == TIPC_ADDR_ID && sender.addr.id.node == 0) {
            struct tipc_event *evt = (struct tipc_event*)buf;
            handle_event(evt);
        } else {
            // Regular group message
            // sender.addrtype 이 실제로는 TIPC_ADDR_ID 인 경우가 대부분이라, 아래
            // sender.addr.name.name.* 필드 읽기는 실제로 id.node/id.ref 값을
            // 이름 필드로 잘못 재해석한 것이다(출력되는 숫자가 비정상적으로 큰 이유).
            printf("Message from {%u,%u}: %s\n",
                   sender.addr.name.name.type, sender.addr.name.name.instance, buf);
        }
    }

    close(sock);
    return 0;
}
