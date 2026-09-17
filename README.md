# TIPC

Linux **TIPC**(Transparent Inter-Process Communication, `AF_TIPC`) 소켓 API를
사용한 C 테스트/데모 프로그램 모음. 클러스터 내 여러 프로세스가 IP
주소가 아니라 **서비스 이름**으로 서로를 찾아 통신하는 TIPC의 특징(이름 기반
라우팅, 자동 장애 조치, 그룹/부하분산)을 디렉토리별로 하나씩 실습·검증한다.

디렉토리마다 송신자/수신자 프로그램 쌍(또는 그 이상)이 들어 있으며, 각각
독립적으로 빌드·실행된다. 최상위 Makefile은 없다.

## 디렉토리 구성

| 디렉토리 | 주제 | 소켓 타입 | 자세히 |
|---|---|---|---|
| [`ASMode/`](ASMode/README.md) | Active-Standby 이중화·장애 조치 | `SOCK_SEQPACKET` (연결형) | 동일 이름으로 여러 수신자 인스턴스를 띄워두면, 그중 하나가 죽어도 송신자는 코드 변경 없이 살아있는 인스턴스로 계속 연결됨을 검증 |
| [`Group/`](Group/README.md) | TIPC 그룹 메시징(브로드캐스트/유니캐스트) + 리더 하트비트 감시 | `SOCK_RDM` (데이터그램) | `TIPC_GROUP_JOIN`으로 그룹을 이뤄 메시지를 주고받는 데모(`tipcGsnd`/`tipcGrcv`)와, 그룹과 무관한 별개의 리더 장애 감시 프로그램(`tipcGsnd2`) |
| [`RDM/`](RDM/README.md) | `SOCK_RDM` bind 방식 비교(단일 인스턴스 vs 범위) + 토폴로지 이벤트 트레이스 | `SOCK_RDM` | `TIPC_SERVICE_ADDR`(단일 이름) vs `TIPC_SERVICE_RANGE`(instance 범위) bind를 비교하고, 커널 토폴로지 서버를 구독해 이름 등록/해제 이벤트를 실시간 관찰 |
| [`RRMode/`](RRMode/README.md) | Round-Robin 부하분산 | `SOCK_RDM` | 동일 서비스 이름에 여러 수신자가 bind되어 있을 때 커널이 메시지를 순환 분배하는 동작을 검증 |

각 디렉토리의 `README.md`에 해당 프로그램의 상세한 용도·사전 준비·빌드·
실행 방법과, 실제 실행하여 확인한 동작/이슈가 정리되어 있다. 소스 코드에도
동일한 내용을 근거로 한 한글 주석이 추가되어 있다.

## 공통 사전 준비 (Prerequisites)

모든 디렉토리에 공통으로 필요하다.

1. **TIPC 커널 모듈**
   ```sh
   sudo modprobe tipc
   lsmod | grep tipc
   ```
   단일 호스트에서 프로세스 여러 개로 테스트할 경우 이것만으로 충분하다.
   여러 물리 노드로 실제 클러스터를 구성하려면 `iproute2-tipc` 패키지의
   `tipc` 명령으로 노드 identity/bearer를 추가로 설정해야 한다.
2. **빌드 의존성**: 각 디렉토리의 `Makefile`(`genmake`로 생성된 동일 양식)은
   `$(HOME)/Project/{include,cominclude,lib,comlib}` 를 `-I`/`-L` 경로로
   참조한다. 소스 자체는 시스템 헤더(`linux/tipc.h` 등)만 사용하므로, 위
   디렉토리가 없어도 컴파일 자체는 보통 성공한다.

## 공통 빌드/실행 패턴

```sh
cd <ASMode|Group|RDM|RRMode>
make            # 해당 디렉토리의 프로그램 빌드
make clean      # 오브젝트/바이너리 정리
make install    # strip 후 $(HOME)/Project/bin 으로 복사
```

실행은 디렉토리별로 터미널 2개 이상이 필요하다(수신자 먼저 실행 → 송신자
실행). 정확한 인자와 예상 출력은 각 디렉토리 `README.md` 참고.

## 참고(이 저장소에서 git으로 추적되지 않는 자료)

`tipcutils/`(자체 git 저장소)와 `tipcutil.tar`는 업스트림 `tipcutils`
프로젝트(`libtipc`, `demos/`, `test/`, `utils/`)의 사본으로, API 참고용으로만
로컬에 두고 있으며 위 4개 디렉토리의 빌드와는 무관하다.
