///
/// @brief : TIPC Group 송신자 — 그룹 브로드캐스트 + 특정 멤버 유니캐스트 데모
/// @file  : tipcGsnd.c
///
/// SOCK_RDM 소켓으로 이름 {GROUP_TYPE, MEMBER_ID}에 bind() 한 뒤
/// TIPC_GROUP_JOIN 으로 그룹에 가입한다. 같은 GROUP_TYPE 으로 bind/join한
/// 다른 프로세스(tipcGrcv 등)들과 하나의 그룹을 이룬다. 짝이 되는 파일은
/// tipcGrcv.c 이며, GROUP_TYPE(2000)은 두 파일이 반드시 동일해야 한다
/// (MEMBER_ID는 그룹 내에서 서로 달라야 하는 멤버 식별자이므로 송신자=10,
/// 수신자=1 로 의도적으로 다르게 되어 있다).
///
#include <sys/socket.h>
#include <linux/tipc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define GROUP_TYPE 2000  // tipcGrcv.c 의 GROUP_TYPE 과 반드시 동일해야 함
#define MEMBER_ID 10     // 그룹 내 이 프로세스의 멤버 인스턴스 번호 (수신자의 1과 달라야 함)
#define MSG_SIZE 64

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

    // Join group
    // TIPC_GROUP_LOOPBACK    : 자신이 보낸 메시지도 자신에게 되돌려 받음(단일 호스트에서
    //                          여러 인스턴스를 자기 자신과 테스트할 때 유용).
    // TIPC_GROUP_MEMBER_EVTS : 다른 멤버의 JOIN/LEAVE 이벤트를 이 소켓의 recv()로도 받음.
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
    printf("Sender {%u,%u} joined group\n", GROUP_TYPE, MEMBER_ID);

    // Prepare destination address for group broadcast
    // !! 확인된 이슈 !!: 아래 dest(addrtype=TIPC_ADDR_NAME, instance=0)로 sendto() 하면
    // "그룹 전체 브로드캐스트"가 아니라 인스턴스 번호가 정확히 0인 멤버에게 보내는
    // 유니캐스트 시도가 된다. 그런 멤버는 아무도 없으므로 실제로는 sendto()가
    // ENETUNREACH("No route to host")로 실패한다(실제 확인됨, README.md 참고).
    // 참고(tipcutils/test/group_test/worker.c의 BROADCAST 케이스): 그룹에 join한
    // 소켓에서 그룹 전체 브로드캐스트를 보내려면 목적지 주소 없이 send()를
    // 호출해야 한다(아래 sendto 호출을 send(sock, msg, strlen(msg)+1, 0)로 바꾸면
    // 정상 동작함을 확인함).
    struct sockaddr_tipc dest = {
        .family = AF_TIPC,
        .addrtype = TIPC_ADDR_NAME,
        .addr.name.name.type = GROUP_TYPE,
        .addr.name.name.instance = 0 // 0 for group broadcast
    };

    // Send multiple messages
    char msg[MSG_SIZE];
    for (int i = 1; i <= 3; i++) {
        snprintf(msg, MSG_SIZE, "Message %d from {%u,%u}", i, GROUP_TYPE, MEMBER_ID);
        if (sendto(sock, msg, strlen(msg) + 1, 0, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
            perror("Send failed");
            close(sock);
            return 1;
        }
        printf("Sent: %s\n", msg);
        sleep(1); // Delay between messages
    }

    // Example of sending to a specific member (e.g., instance 1)
    // 이 유니캐스트(anycast) 전송은 실제 존재하는 멤버(tipcGrcv, MEMBER_ID=1)를
    // 정확히 지정하므로 위의 브로드캐스트 시도와 달리 정상적으로 성공한다(확인됨).
    dest.addr.name.name.instance = 1; // Target specific member
    snprintf(msg, MSG_SIZE, "Direct message from {%u,%u} to {%u,1}", GROUP_TYPE, MEMBER_ID, GROUP_TYPE);
    if (sendto(sock, msg, strlen(msg) + 1, 0, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("Send to specific member failed");
    } else {
        printf("Sent: %s\n", msg);
    }

    close(sock);
    return 0;
}
