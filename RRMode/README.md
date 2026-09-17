# RRMode — TIPC Round-Robin 부하분산 테스트

## 용도 및 역할

TIPC(`AF_TIPC`, `SOCK_RDM`) 데이터그램 소켓에서, **동일한 서비스 이름에 여러
수신자가 bind()** 되어 있을 때 TIPC 커널이 `sendto()`를 인스턴스들에
순환(round-robin) 분배하는 동작을 확인하는 송신자/수신자 테스트 쌍이다.

- `tipcRRsnd.c` — **송신자**. `bind()` 없이(익명 소켓) 5초 간격으로 최대 100건의
  메시지를 하나의 서비스 이름으로 `sendto()` 한다.
- `tipcRRrcv.c` — **수신자**. `SOCK_RDM` 소켓을 서비스 이름 `{SERVICE_TYPE,
  INSTANCE}` 로 bind하고 `recvfrom()` 루프를 돈다.

라운드로빈 분배 로직은 송신자/수신자 어느 쪽 코드에도 없다 — **같은 이름으로
`tipcRRrcv`를 여러 인스턴스(A, B, ...) 띄워두면** TIPC 이름 테이블이 해당
이름으로 들어오는 각 `sendto()` 호출을 인스턴스들에 번갈아 배분해 주는 것이
테스트 대상이다.

## ⚠️ 현재 소스의 서비스 이름 불일치 (중요)

두 파일의 서비스 이름 상수가 **서로 다르게** 박혀 있다.

| 파일 | type | instance |
|---|---|---|
| `tipcRRsnd.c` (송신 목적지) | `30000` | `1` |
| `tipcRRrcv.c` (수신 bind) | `18888` (`SERVICE_TYPE`) | `50` (`INSTANCE`) |

이 상태로 그대로 실행하면 **송신자가 즉시 실패한다**(실제 확인 결과):
```
Send failed: No route to host
```
테스트하려면 아래 중 하나로 두 값을 일치시켜야 한다.
- `tipcRRsnd.c` 25~26번째 줄의 `.addr.name.name.type` / `.instance` 값을
  `tipcRRrcv.c` 의 `SERVICE_TYPE`(18888) / `INSTANCE`(50) 로 수정, 또는
- 반대로 `tipcRRrcv.c` 의 `SERVICE_TYPE`/`INSTANCE` 를 `30000`/`1` 로 수정

수정 후 `make clean && make` 로 다시 빌드한다.

## 사전 준비 (Prerequisites)

1. TIPC 커널 모듈 로드:
   ```sh
   sudo modprobe tipc
   lsmod | grep tipc
   ```
   단일 호스트에서 여러 프로세스로 테스트할 경우 이것만으로 충분하다.
2. 빌드 의존성: `Makefile`이 `$(HOME)/Project/{include,cominclude,lib,comlib}` 를
   참조한다. 소스는 시스템 헤더만 사용하므로 위 디렉토리가 없어도 컴파일은 보통 성공한다.

## 빌드 방법

```sh
cd RRMode
make            # tipcRRsnd, tipcRRrcv 생성
make clean
make install    # strip 후 $(HOME)/Project/bin 으로 복사
```

## 실행 방법

위의 **서비스 이름 불일치를 먼저 해결**한 뒤 진행한다.

**단일 수신자로 기본 동작 확인**
```sh
# 터미널 1
./tipcRRrcv
# listening on service type 18888 Instance 50

# 터미널 2
./tipcRRsnd
# Sent: Hello from Node A -   0
# Sent: Hello from Node A -   1
# ... (5초 간격, 최대 100건)
```
수신자 쪽 출력 예:
```
Received message: Hello from Node A -   0
	from <2184815973,287446801,0>
```
> `from <...>` 의 세 숫자는 실제 서비스 이름이 아니다. 송신자가 `bind()` 없이
> 보내는 익명 소켓이라, 커널이 부여한 임시 포트 참조값이 `type`/`instance`
> 필드 형태로 재해석되어 출력되는 것뿐이다(정상 동작, 실제 확인됨).

**라운드로빈 분배 확인 (핵심 시나리오)**

1. 동일한 이름으로 수신자를 두 개(A, B) 띄운다.
   ```sh
   ./tipcRRrcv &     # 인스턴스 A
   ./tipcRRrcv &     # 인스턴스 B
   ```
2. 송신자를 실행한다.
   ```sh
   ./tipcRRsnd
   ```
3. 두 수신자의 출력을 비교하면, 메시지 0/1/2/3...이 A와 B에 **번갈아**
   도착하는 것을 확인할 수 있다(실제 확인됨: `0→B, 1→A, 2→B, ...` 순서로
   교차 분배됨). 수신자 인스턴스 수를 늘리면 그만큼 더 잘게 순환 분배된다.

## 파일 구성

| 파일 | 설명 |
|---|---|
| `tipcRRsnd.c` | 송신자 — 5초 간격으로 최대 100건 전송 (bind 없는 익명 소켓) |
| `tipcRRrcv.c` | 수신자 — 서비스 이름에 bind 후 `recvfrom()` 반복 |
| `Makefile` | `genmake`로 생성된 표준 빌드 파일 |
