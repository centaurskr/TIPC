# RDM — TIPC 데이터그램(SOCK_RDM) 바인딩 방식 비교 + 토폴로지 트레이스

## 용도 및 역할

TIPC `SOCK_RDM`(신뢰성 있는 데이터그램) 소켓의 **두 가지 bind 방식**을
비교하고, 동시에 커널의 **토폴로지 서버(Topology Server)** 를 이용해 이름
등록/해제(publish/withdraw) 이벤트를 실시간으로 관찰하는 도구 모음이다.
CLI 인자로 서비스 이름을 직접 지정하므로(하드코딩 아님), 하나의 빌드로
다양한 조합을 테스트할 수 있다.

- `tipc_rdm_snd.c` — **송신자**. `{service, instance}` 로 1초 간격 최대
  1000건을 `sendto()`. 동시에 백그라운드 스레드로 해당 `service` type
  전체의 이름 등록/해제 이벤트를 구독해 실시간 출력한다.
- `tipc_sa_recv.c` — **수신자 (단일 인스턴스)**. `TIPC_SERVICE_ADDR` 로
  정확히 하나의 `{type, instance}` 에만 bind.
- `tipc_sr_recv.c` — **수신자 (범위)**. `TIPC_SERVICE_RANGE` 로
  `[lower, upper]` instance 범위 전체를 하나의 bind로 수신.
- `tipcdump.c` — TIPC 주소/이벤트 구조체를 보기 좋게 출력하는 Dump* 함수
  모음(위 세 프로그램이 공유, 링크 타임에만 결합되고 별도 헤더는 없음 —
  각 파일에서 `extern` 선언으로 시그니처만 가져다 씀).
- `toptrace.c` — `TipcServiceTrace()`: 커널 내장 토폴로지 서버에 접속해
  특정 service type의 이름 등록/해제 이벤트를 구독하는 fd를 만들어 준다.

## 핵심 개념: 데이터 채널 + 이벤트 채널 동시 감시

세 프로그램(`tipc_rdm_snd`, `tipc_sa_recv`, `tipc_sr_recv`) 모두 소켓을
**두 개** 연다 — 하나는 실제 메시지를 주고받는 데이터 소켓, 다른 하나는
`TipcServiceTrace()`가 만들어주는 토폴로지 이벤트 구독 소켓. 두 fd를 하나의
`poll()`로 함께 감시하다가, 상대측이 서비스 이름을 bind/close 할 때마다
`TIPC_PUBLISHED`/`TIPC_WITHDRAWN` 이벤트가 실시간으로 출력된다 —
Active-Standby/RRMode 등 다른 디렉토리의 프로그램을 실행하는 동안 이름
테이블이 실제로 어떻게 변하는지 관찰하는 디버깅 도구로도 쓸 수 있다.

## ⚠️ 확인된 이슈 (실제 실행하여 검증함)

- `tipc_rdm_snd.c`: `int sz;` (지역 변수)가 **초기화되지 않은 채**
  `getsockname(sock, (struct sockaddr *)&tipcaddr, &sz)` 의 in/out 길이
  인자로 쓰인다(정의되지 않은 동작). 실제 테스트에서는 우연히 문제없이
  동작했지만, 정석대로면 `int sz = sizeof(tipcaddr);` 로 초기화해야 한다.
- `tipc_rdm_snd.c`/`tipcdump.c`: `tipc_rdm_snd`는 `bind()`를 하지 않으므로
  `getsockname()` 결과는 `ID`(node/ref)만 유효한데, `DumpSockAddrTipc()`는
  `addrtype`을 확인하지 않고 `ID`/`NAMESEQ`/`NAME` 세 블록을 무조건 전부
  출력한다. 따라서 실행 로그의 `NAMESEQ`/`NAME` 블록 값은 같은 raw 바이트를
  다른 필드로 잘못 재해석한 의미 없는 숫자다(실제 확인됨, 아래 실행 예시
  참고).
- `tipc_sa_recv.c`/`tipc_sr_recv.c`: `#define SERVICE_TYPE 30000` 은
  사용되지 않는 죽은 코드다(실제 서비스 type은 `argv[1]`로 받음).

## 사전 준비 (Prerequisites)

```sh
sudo modprobe tipc
lsmod | grep tipc
```
단일 호스트 내 여러 프로세스 테스트는 이것만으로 충분하다. `RDM/Makefile`은
다른 디렉토리와 달리 `-lpthread`를 링크한다(`tipc_rdm_snd`의 모니터 스레드 때문).

## 빌드 방법

```sh
cd RDM
make            # tipc_rdm_snd, tipc_sa_recv, tipc_sr_recv 생성
make clean
make install
```

## 실행 방법

### 사용법(인자 없이 실행하면 출력됨)

```
$ ./tipc_rdm_snd
TIPC RDM Mesasge sender
./tipc_rdm_snd <service type> <instance>

$ ./tipc_sa_recv
TIPC RDM Mesasge receiver
./tipc_sa_recv <service type> <instance> <top type>
	top type : 1=TIPC_SUB_PORTS(모든 이벤트) 2=TIPC_SUB_SERVICE(등록/해제만) 3=TIPC_SUB_CANCEL

$ ./tipc_sr_recv
TIPC RDM.Service Range Mesasge receiver
./tipc_sr_recv <service type> <lower> <upper> <top type>
```

### 1) 단일 인스턴스 bind (`tipc_sa_recv`)

```sh
# 터미널 1 — service type 40000, instance 1로 bind, top=1(모든 이벤트)
./tipc_sa_recv 40000 1 1

# 터미널 2 — 같은 이름으로 전송
./tipc_rdm_snd 40000 1
```
수신 측 출력 예(실제 확인됨):
```
START----------------
	-TIPC_EVENT:TIPC_PUBLISHED , node(287446801) ref(2988103280)
MESSAGE from node(287446801): Hello from Node A -   0 {40000,1}
MESSAGE from node(287446801): Hello from Node A -   1 {40000,1}
...
```
(`TIPC_PUBLISHED` 이벤트는 송신자의 모니터 스레드가 같은 service type을
구독하며 자기 자신의 이름 변화를 감지한 것이 아니라, 수신자가 이 소켓을
bind 하는 시점에 발생한 이벤트다.)

### 2) 범위 bind (`tipc_sr_recv`)

```sh
# 터미널 1 — service type 41000, instance 1~5 범위 전체 수신
./tipc_sr_recv 41000 1 5 1

# 터미널 2 — 범위 안의 instance(예: 3)로 전송
./tipc_rdm_snd 41000 3
```
수신 측 출력 예(실제 확인됨, ref의 상위/하위 16비트를 bearer id로 함께 표시):
```
START--ServiceRange-Lower(1) upper(5)-
	-TIPC_EVENT:TIPC_PUBLISHED , node(287446801) ref(992839686)
from node(287446801)ref(3501198091):상16(53424,0xd0b0)하16(2827,0xb0b)
	Hello from Node A -   0 {41000,3}
...
```
송신 측(`tipc_rdm_snd`)에는 자신의 소켓 정보(위 "확인된 이슈" 참고 —
`NAMESEQ`/`NAME` 블록은 무시)와 모니터 스레드가 잡아낸 이벤트가 함께
찍힌다:
```
Sock name-sockaddr_tipc---------------------
	family(30) addrtype(3) scope(0)
		ID tipc_socket_addr------
		 ref(3501198091), node(287446801)
		...
Sent: Hello from Node A -   0 {41000,3}
TIPC Monitor(all) : service (41000), socket fd(4)
	-TIPC_EVENT:TIPC_PUBLISHED , node(287446801) ref(992839686)
Sent: Hello from Node A -   1 {41000,3}
...
```

### top type(이벤트 필터) 바꿔보기

`tipc_sa_recv`/`tipc_sr_recv`의 마지막 인자를 `2`로 바꾸면 개별 포트가
아니라 서비스 등록/해제(publish/withdraw)만 구독한다. 다른 값(3)은 구독
취소(`TIPC_SUB_CANCEL`) 요청을 보낸다.

## 파일 구성

| 파일 | 설명 |
|---|---|
| `tipc_rdm_snd.c` | 송신자 — 반복 전송 + 백그라운드 토폴로지 모니터 스레드 |
| `tipc_sa_recv.c` | 수신자 — 단일 인스턴스(`TIPC_SERVICE_ADDR`) bind |
| `tipc_sr_recv.c` | 수신자 — instance 범위(`TIPC_SERVICE_RANGE`) bind |
| `tipcdump.c` | TIPC 주소/이벤트 구조체 출력용 Dump* 헬퍼 (공용, 헤더 없이 extern 선언으로 사용) |
| `toptrace.c` | `TipcServiceTrace()` — 토폴로지 서버 구독 fd 생성 |
| `Makefile` | `genmake`로 생성된 표준 빌드 파일 (`-lpthread` 링크) |
