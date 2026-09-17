# Group — TIPC 그룹 메시징 및 하트비트 감시 테스트

## 용도 및 역할

이 디렉토리에는 서로 성격이 다른 **두 가지** 데모가 섞여 있다.

### 1) 그룹 메시징 데모 — `tipcGsnd.c` / `tipcGrcv.c`

TIPC(`AF_TIPC`, `SOCK_RDM`) 소켓의 **그룹(Group) 기능**(`TIPC_GROUP_JOIN`)을
이용해, 이름 `{GROUP_TYPE=2000, MEMBER_ID}` 로 여러 프로세스가 하나의 그룹을
이루고 브로드캐스트/특정 멤버 유니캐스트를 주고받는 데모.

- `tipcGsnd.c` — 그룹에 `MEMBER_ID=10` 으로 가입한 뒤, 그룹 전체 브로드캐스트를
  3회 시도하고 마지막으로 `MEMBER_ID=1`(즉 `tipcGrcv`)에게 직접 유니캐스트
  메시지를 보낸다.
- `tipcGrcv.c` — 그룹에 `MEMBER_ID=1` 로 가입해 메시지와 멤버 JOIN/LEAVE
  이벤트를 수신 출력한다.

### 2) 하트비트 장애 감시 — `tipcGsnd2.c` (이름과 달리 그룹과 무관)

이름은 `tipcGsnd2`이지만 위 그룹 데모와 **전혀 다른 프로그램**이다. 그룹에
join하지 않고 단순히 이름 `{NODE_MST_TYPE=6000, 0}` 에 bind()한 뒤, 리더
노드가 주기적으로 보내는 `"heartbeat"` 메시지를 기다리다가 `TIMEOUT_SEC`(10초)
동안 수신이 끊기면 "리더 사망"으로 판단해 로그를 남기는 **감시자(watcher)**
프로그램이다. 이 저장소에는 실제로 heartbeat를 보내는 리더/마스터 측
프로그램이 포함되어 있지 않다 — 테스트 방법은 아래 참고.

## ⚠️ 확인된 이슈 (실제 실행하여 검증함)

### (A) `tipcGsnd.c` 의 그룹 브로드캐스트가 현재 실패한다

`tipcGsnd.c`는 브로드캐스트를 `sendto(sock, msg, ..., &dest, ...)` 로
구현했는데, `dest`의 `addrtype = TIPC_ADDR_NAME`, `instance = 0` 은
"그룹 전체"가 아니라 **인스턴스 번호가 정확히 0인 멤버**를 향한 유니캐스트
주소다. 그런 멤버는 존재하지 않으므로 실제 실행 시 첫 반복에서 바로 실패한다:
```
Send failed: No route to host
```
(직후의 `MEMBER_ID=1` 직접 유니캐스트 코드는 실제 멤버를 지정하므로 정상
동작함을 확인했다.)

TIPC 그룹에서 "전체 브로드캐스트"를 보내는 올바른 방법은 목적지 주소 없이
**`send(sock, msg, len, 0)`** 을 호출하는 것이다(참고: 벤더링된
`tipcutils/test/group_test/worker.c`의 BROADCAST 케이스). 실제로 위
`sendto()` 를 `send()` 로 바꿔서 테스트해보면 3건의 브로드캐스트와 마지막
유니캐스트까지 모두 정상 전송됨을 확인했다. 소스 코드 자체는 수정하지
않았으며(주석으로만 표시), 직접 실행해보고 싶다면 로컬에서 위와 같이
`sendto()` 호출부만 `send()` 로 바꿔 재빌드하면 된다.

### (B) `tipcGrcv.c` 의 이벤트 판별/발신자 주소 해석이 실제로는 맞지 않는다

`handle_event()` 호출 조건 `sender.addrtype == TIPC_ADDR_ID && sender.addr.id.node == 0`
은 실사용 환경에서 참이 되는 경우가 거의 없다(`node`는 실제 노드 해시 값이라
0이 아니기 때문). 그 결과 JOIN/LEAVE 이벤트든 일반 메시지든 **항상** else
분기(`"Message from {%u,%u}: ..."`)로 흘러간다. 그런데 TIPC 그룹 소켓은
발신자 주소를 항상 `TIPC_ADDR_ID`(노드/포트 id) 형태로 보고하기 때문에, else
분기가 이를 `sender.addr.name.name.type/instance` 로 읽는 것은 실제로는
노드/포트 id 값을 이름 필드로 잘못 재해석하는 것이다. 실행해보면 아래처럼
큰 임의의 숫자가 찍히는 것으로 확인할 수 있다:
```
Message from {3177007876,287446801}:
```
(주고받은 메시지 문자열 자체는 `buf`에 정상적으로 들어있지만, 위 두 숫자는
GROUP_TYPE/MEMBER_ID가 아니다.)

## 사전 준비 (Prerequisites)

```sh
sudo modprobe tipc
lsmod | grep tipc
```
단일 호스트 내 여러 프로세스 테스트는 이것만으로 충분하다.

## 빌드 방법

```sh
cd Group
make            # tipcGrcv, tipcGsnd, tipcGsnd2 생성
make clean
make install
```

## 실행 방법

### 그룹 메시징 데모 (`tipcGsnd` / `tipcGrcv`)

```sh
# 터미널 1
./tipcGrcv
# Joined group {2000,1}

# 터미널 2
./tipcGsnd
# Sender {2000,10} joined group
# Send failed: No route to host      <- 위 이슈(A), 프로그램은 여기서 즉시 종료됨
```
현재 소스 그대로는 브로드캐스트 3건이 전송되지 못하고 종료된다. 위 이슈(A)에서
설명한 대로 `sendto()`를 `send()`로 바꿔 재빌드하면 브로드캐스트 3건과 마지막
유니캐스트까지 모두 전송되며, 수신 측에는 다음과 같이(이슈 B로 인해 GROUP_TYPE/
MEMBER_ID 대신 임의의 노드/포트 id 숫자가 찍힌 채) 출력된다:
```
Joined group {2000,1}
Message from {3177007876,287446801}:
Message from {796608197,287446801}:
...
```

### 하트비트 감시자 (`tipcGsnd2`) 단독 테스트

이 저장소에 짝이 되는 리더 송신 프로그램이 없으므로, `python3`의 `AF_TIPC`
소켓 지원을 이용해 수동으로 하트비트를 보내 검증한다(실제 확인된 방법):

```sh
# 터미널 1
./tipcGsnd2
# Node-MST: Waiting for heartbeat...

# 터미널 2 — heartbeat 한 번 전송
python3 - <<'EOF'
import socket
s = socket.socket(socket.AF_TIPC, socket.SOCK_RDM)
dest = (socket.TIPC_ADDR_NAME, 6000, 0, 0, socket.TIPC_CLUSTER_SCOPE)
s.sendto(b"heartbeat", dest)
EOF
```
터미널 1에 `Received heartbeat.` 가 출력되면 정상. 이후 10초(`TIMEOUT_SEC`)
동안 추가 heartbeat 없이 방치하면 다음 로그가 출력되는지 확인한다:
```
No heartbeat detected! Leader is dead. Trigger failover!
```

## 파일 구성

| 파일 | 설명 |
|---|---|
| `tipcGsnd.c` | 그룹 송신자 — 브로드캐스트(현재 실패, 이슈 A) + 특정 멤버 유니캐스트 |
| `tipcGrcv.c` | 그룹 수신자 — 메시지/이벤트 수신 (이슈 B로 주소 표시가 부정확) |
| `tipcGsnd2.c` | 그룹과 무관한 독립 프로그램 — 리더 하트비트 타임아웃 감시자 |
| `Makefile` | `genmake`로 생성된 표준 빌드 파일 |
