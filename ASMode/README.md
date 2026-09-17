# ASMode — TIPC Active-Standby 테스트

## 용도 및 역할

TIPC(`AF_TIPC`, `SOCK_SEQPACKET`) 소켓의 **이름 기반 라우팅**을 이용해
Active-Standby(이중화) 구성에서의 장애 조치(failover) 동작을 검증하는
송신자/수신자 테스트 프로그램 쌍이다.

- `tipcASsnd.c` — **송신자**. 사용자가 입력한 메시지를 한 줄씩, 그때마다
  새로 `connect()`하여 서비스 이름 `{SERVICE_TYPE=18888, INSTANCE=1}` 로
  전송하고 즉시 연결을 끊는다. 송신자는 어떤 수신자 프로세스가 실제로
  응답하는지 알지 못하며, 오직 서비스 이름만으로 접속한다.
- `tipcASrcv.c` — **수신자**. 동일한 서비스 이름 `{18888, 1}` 을
  `TIPC_ADDR_NAMESEQ`(범위 바인딩, `lower == upper == INSTANCE`)로 bind하고
  접속을 기다린다. 실행 시 인자로 `priority`(0~3)를 받아 `TIPC_IMPORTANCE`
  소켓 옵션으로 설정한다.

같은 이름으로 `tipcASrcv`를 **여러 인스턴스** 띄워두면, TIPC 커널의 이름
테이블에 다중 바인딩으로 등록되어 그중 하나가 죽어도 송신자는 코드/설정
변경 없이 살아있는 다른 인스턴스로 계속 연결된다 — 이것이 이 테스트가
검증하는 Active-Standby 동작이다.

> **참고**: `--priority` 인자는 `TIPC_IMPORTANCE` 소켓 옵션에 그대로
> 전달되는데, 이 옵션은 실제로는 "어느 인스턴스가 접속을 우선적으로
> 받을지"를 정하는 값이 아니라, 링크 혼잡(congestion) 시 해당 소켓이 보내는
> 메시지를 얼마나 먼저 폐기할지를 정하는 값이다(낮을수록 먼저 폐기됨). 이
> 데모에서는 여러 수신자 인스턴스를 구분하기 위한 라벨 용도로 쓰인다.

## 사전 준비 (Prerequisites)

1. **TIPC 커널 모듈**이 로드되어 있어야 한다.
   ```sh
   sudo modprobe tipc
   lsmod | grep tipc      # tipc 모듈이 보이면 OK
   ```
   단일 호스트 내에서 두 프로세스만으로 테스트할 경우 이것만으로 충분하다
   (별도의 bearer/노드 설정 없이도 같은 노드 안에서는 이름 기반 통신이 된다).
   여러 물리 노드에 걸쳐 실제 failover를 구성하려면 `iproute2-tipc` 패키지의
   `tipc` 명령으로 노드 identity 및 bearer(예: UDP/이더넷)를 추가로
   설정해야 한다.
2. **빌드 의존성**: 이 디렉토리의 `Makefile`은 `$(HOME)/Project` 아래의
   `include/`, `cominclude/`, `lib/`, `comlib/` 디렉토리를 `-I`/`-L` 경로로
   참조한다. 소스 자체는 시스템 헤더(`linux/tipc.h` 등)만 사용하므로, 위
   디렉토리가 없어도 컴파일 자체는 보통 성공한다.

## 빌드 방법

```sh
cd ASMode
make            # tipcASsnd, tipcASrcv 생성
make clean      # 오브젝트 파일/바이너리 정리
make install    # strip 후 $(HOME)/Project/bin 으로 복사, 이후 make clean 수행
```

## 실행 방법

터미널 2개(또는 그 이상) 필요.

**터미널 1 — 수신자 실행** (priority는 0~3, 인스턴스 구분용 라벨):
```sh
./tipcASrcv 0
```
```
Receiver started with priority 0
```

**터미널 2 — 송신자 실행**, 프롬프트에 메시지를 입력:
```sh
./tipcASsnd
```
```
Enter message to send: hello
Message sent and colse socket
Enter message to send:
```

수신자 쪽에는 다음과 같이 출력된다:
```
Connection accepted
Received: hello
Connection closed
```

> `Connection closed` 직전에 `recv: Connection reset by peer` 라는
> `perror` 출력이 함께 보일 수 있다. 송신자가 `send()` 직후 곧바로
> `close()`하기 때문에 발생하는 것으로, 메시지 자체는 이미 정상 수신된
> 뒤이므로 이 테스트에서는 오류가 아니라 정상 동작이다.

### Active-Standby(장애 조치) 시나리오 테스트

1. `./tipcASrcv 0` (인스턴스 A), `./tipcASrcv 1` (인스턴스 B)를 각각 다른
   터미널에서 동시에 실행한다 — 둘 다 동일한 `{18888, 1}` 이름으로 bind된다.
2. `./tipcASsnd` 로 메시지를 몇 건 전송하며 어느 인스턴스가 수신하는지
   확인한다.
3. 현재 메시지를 받고 있는 인스턴스를 `Ctrl+C`로 종료한다.
4. `./tipcASsnd` 에서 계속 메시지를 입력한다 — 송신자 쪽 코드/설정 변경 없이
   남아있는 다른 인스턴스가 이어서 메시지를 수신하는지 확인한다(둘 다
   종료된 경우 `Connect()`가 실패하여 송신자가 종료됨).

## 파일 구성

| 파일 | 설명 |
|---|---|
| `tipcASsnd.c` | 송신자 — stdin 한 줄마다 재접속 후 전송 |
| `tipcASrcv.c` | 수신자 — `priority` 인자를 받아 bind/listen/accept 반복 |
| `Makefile` | `genmake`로 생성된 표준 빌드 파일 |
