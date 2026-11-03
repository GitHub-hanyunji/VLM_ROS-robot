// ip_sender.cpp
// WSL(우분투 22.04)에서 실행.
// 터미널에 "호출"을 입력하거나 브라우저 화면의 [호출] 버튼을 누르면
// 내 IP를 Jetson으로 UDP 전송한다.
// Jetson의 class_coordinate_publisher가 받은 IP를 db.json의 키로 조회해 좌표를 publish한다.
// ROS2와 무관한 일반 소켓 프로그램.
//
// 빌드: g++ -std=c++17 -o ip_sender ip_sender.cpp
// 실행: ./ip_sender <Jetson IP> <포트> [웹 화면 포트, 기본 8080]
// 예:  ./ip_sender 192.168.0.50 6001   (class_coordinate_publisher 기본 udp_port = 6001)
// 화면: Windows 브라우저에서 http://localhost:8080

#include <iostream>
#include <string>
#include <cstring>
#include <ctime>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <poll.h>
#include <csignal>

// ===== [추가] 브라우저에 보여줄 화면 =====
static const char* INDEX_HTML = R"HTML(<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Jetson IP 호출</title>
<style>
  :root {
    --bg:#f4f5f7; --card:#fff; --text:#1b1f24; --muted:#667080; --line:#e2e5ea;
    --accent:#2563eb; --accent2:#1d4ed8; --ok:#16a34a; --okbg:#e9f8ef; --err:#dc2626; --errbg:#fdecec;
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --bg:#0f1216; --card:#171b21; --text:#e7eaee; --muted:#8b95a3; --line:#262c35;
      --accent:#3b82f6; --accent2:#2563eb; --ok:#34d399; --okbg:#0f2a20; --err:#f87171; --errbg:#2c1515;
    }
  }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--text);
         font:15px/1.5 -apple-system,"Segoe UI","Malgun Gothic",sans-serif; }
  .mono { font-family:ui-monospace,Consolas,monospace; }
  .wrap { max-width:520px; margin:0 auto; padding:28px 16px 48px; }
  header { display:flex; justify-content:space-between; align-items:center; margin-bottom:20px; }
  h1 { font-size:20px; margin:0; }
  .status { display:flex; align-items:center; gap:6px; font-size:13px; color:var(--muted); }
  .dot { width:8px; height:8px; border-radius:50%; background:var(--muted); }
  .on .dot { background:var(--ok); box-shadow:0 0 0 3px var(--okbg); }
  .off .dot { background:var(--err); box-shadow:0 0 0 3px var(--errbg); }
  .card { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:16px 18px; margin-bottom:14px; }
  .item { display:flex; justify-content:space-between; align-items:center; padding:6px 0; }
  .item + .item { border-top:1px solid var(--line); }
  .label { color:var(--muted); font-size:13px; }
  .value { font-size:16px; font-weight:600; }
  .send { width:100%; padding:22px; border:0; border-radius:14px; background:var(--accent); color:#fff;
          font:600 20px/1 inherit; font-family:inherit; cursor:pointer; transition:background .15s, transform .05s; }
  .send:hover { background:var(--accent2); }
  .send:active { transform:scale(.99); }
  .send:disabled { opacity:.45; cursor:not-allowed; }
  .tip { text-align:center; font-size:12px; color:var(--muted); margin:8px 0 18px; }
  kbd { font:11px ui-monospace,monospace; border:1px solid var(--line); border-bottom-width:2px; border-radius:4px; padding:1px 5px; }
  .flash { display:none; border-radius:10px; padding:10px 12px; margin-bottom:14px; font-size:14px; }
  .flash.ok { display:block; background:var(--okbg); color:var(--ok); }
  .flash.err { display:block; background:var(--errbg); color:var(--err); }
  .loghead { display:flex; justify-content:space-between; margin-bottom:6px; }
  .loghead b { font-size:13px; color:var(--muted); font-weight:600; }
  .link { background:none; border:0; color:var(--muted); cursor:pointer; font:inherit; font-size:13px; padding:0; }
  ul { list-style:none; margin:0; padding:0; max-height:260px; overflow-y:auto; }
  li { display:flex; gap:10px; padding:7px 0; border-top:1px solid var(--line); font-size:13px; }
  li:first-child { border-top:0; }
  .t { color:var(--muted); } .g { color:var(--ok); } .r { color:var(--err); }
  .empty { color:var(--muted); font-size:13px; }
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>Jetson IP 호출</h1>
    <span id="status" class="status"><span class="dot"></span><span id="statusText">연결 확인 중…</span></span>
  </header>

  <div class="card">
    <div class="item"><span class="label">보낼 곳 (Jetson)</span><span id="target" class="value mono">-</span></div>
    <div class="item"><span class="label">보낼 내 IP</span><span id="myip" class="value mono">-</span></div>
  </div>

  <div id="flash" class="flash"></div>
  <button id="send" class="send" disabled>호출</button>
  <div class="tip"><kbd>Enter</kbd> 로도 호출할 수 있습니다</div>

  <div class="card">
    <div class="loghead"><b>전송 기록 <span id="count"></span></b><button id="clear" class="link">지우기</button></div>
    <ul id="log"></ul>
    <div id="empty" class="empty">아직 전송한 기록이 없습니다.</div>
  </div>
</div>
<script>
const $ = id => document.getElementById(id);
let online = false, busy = false, history = [];
try { history = JSON.parse(localStorage.getItem('history') || '[]'); } catch {}

function setOnline(on) {
  online = on;
  $('status').className = 'status ' + (on ? 'on' : 'off');
  $('statusText').textContent = on ? '연결됨' : '프로그램 꺼짐';
  $('send').disabled = !on || busy;
}

async function refresh() {
  try {
    const j = await (await fetch('/api/info', { cache: 'no-store' })).json();
    $('target').textContent = j.jetson + ':' + j.port;
    $('myip').textContent = j.myIP || '확인 실패';
    setOnline(true);
  } catch { setOnline(false); }
}

async function call() {
  if (!online || busy) return;
  busy = true; $('send').disabled = true; $('send').textContent = '전송 중…';
  let j;
  try { j = await (await fetch('/api/call', { method: 'POST' })).json(); }
  catch { j = { ok: false, error: '프로그램에 연결할 수 없습니다' }; setOnline(false); }
  busy = false; $('send').textContent = '호출'; $('send').disabled = !online;
  const time = j.time || new Date().toTimeString().slice(0, 8);
  const msg = j.ok ? `${j.payload} → ${j.jetson}:${j.port}` : j.error;
  flash(j.ok ? 'ok' : 'err', j.ok ? '전송됨: ' + msg : msg);
  history.unshift({ ok: j.ok, time, msg });
  history = history.slice(0, 50);
  save(); render();
  refresh();
}

let ft;
function flash(kind, text) {
  $('flash').className = 'flash ' + kind; $('flash').textContent = text;
  clearTimeout(ft); ft = setTimeout(() => $('flash').className = 'flash', 4000);
}
function save() { try { localStorage.setItem('history', JSON.stringify(history)); } catch {} }
function render() {
  $('log').innerHTML = '';
  for (const e of history) {
    const li = document.createElement('li');
    li.innerHTML = `<span class="t mono"></span><span class="${e.ok ? 'g' : 'r'}">${e.ok ? '✓' : '✕'}</span><span class="mono m"></span>`;
    li.querySelector('.t').textContent = e.time;
    li.querySelector('.m').textContent = e.msg;
    $('log').appendChild(li);
  }
  $('empty').style.display = history.length ? 'none' : '';
  $('count').textContent = history.length ? `(${history.length})` : '';
}

$('send').onclick = call;
$('clear').onclick = () => { history = []; save(); render(); };
document.addEventListener('keydown', e => { if (e.key === 'Enter' && !e.isComposing) { e.preventDefault(); call(); } });
render(); refresh(); setInterval(refresh, 5000);
</script>
</body>
</html>
)HTML";
// ===== [추가 끝] =====

// loopback을 제외한 첫 번째 IPv4 주소를 찾는다.
std::string getLocalIP()
{
    struct ifaddrs *ifaddr, *ifa;
    std::string result;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        std::string name(ifa->ifa_name);
        if (name == "lo") continue;

        void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, addr, buf, sizeof(buf));
        result = buf;
        break; // 첫 번째로 찾은 non-loopback IPv4 사용
    }

    freeifaddrs(ifaddr);
    return result;
}

// ===== [변경] 기존 "호출" 처리 부분을 함수로 묶음 (터미널/버튼 공용) =====
bool callJetson(int sockfd, const sockaddr_in& servaddr, const std::string& jetsonIP, int port,
                std::string& myIP, std::string& payload, std::string& error)
{
    myIP = getLocalIP();
    if (myIP.empty()) {
        error = "IP 확인 실패";
        std::cerr << error << "\n";
        return false;
    }
    // Jetson(class_coordinate_publisher)은 받은 문자열을 db.json의 키로 조회하므로 IP만 보낸다.
    payload = myIP;

    ssize_t sent = sendto(sockfd, payload.c_str(), payload.size(), 0,
                          (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (sent < 0) {
        error = std::string("sendto 실패: ") + strerror(errno);
        perror("sendto");
        return false;
    }
    std::cout << "[전송됨] " << payload << " -> " << jetsonIP << ":" << port << std::endl;
    return true;
}

std::string jsonEscape(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

// ===== [추가] 브라우저 요청 처리 =====
void sendHttp(int fd, const std::string& type, const std::string& body)
{
    std::string res = "HTTP/1.1 200 OK\r\nContent-Type: " + type +
                      "\r\nContent-Length: " + std::to_string(body.size()) +
                      "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + body;
    size_t off = 0;
    while (off < res.size()) {
        ssize_t n = write(fd, res.data() + off, res.size() - off);
        if (n <= 0) break;
        off += n;
    }
}

void handleWeb(int fd, int sockfd, const sockaddr_in& servaddr, const std::string& jetsonIP, int port)
{
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';
    std::string req(buf);

    std::string base = "\"jetson\":\"" + jetsonIP + "\",\"port\":" + std::to_string(port);

    if (req.rfind("POST /api/call", 0) == 0) {
        std::string myIP, payload, error;
        char t[16];
        std::time_t now = std::time(nullptr);
        std::strftime(t, sizeof(t), "%H:%M:%S", std::localtime(&now));
        bool ok = callJetson(sockfd, servaddr, jetsonIP, port, myIP, payload, error);
        sendHttp(fd, "application/json; charset=utf-8",
                 std::string("{\"ok\":") + (ok ? "true" : "false") + "," + base +
                 ",\"myIP\":\"" + myIP + "\",\"payload\":\"" + jsonEscape(payload) +
                 "\",\"error\":\"" + jsonEscape(error) + "\",\"time\":\"" + t + "\"}");
    }
    else if (req.rfind("GET /api/info", 0) == 0) {
        sendHttp(fd, "application/json; charset=utf-8",
                 "{" + base + ",\"myIP\":\"" + getLocalIP() + "\"}");
    }
    else {
        sendHttp(fd, "text/html; charset=utf-8", INDEX_HTML);
    }
}
// ===== [추가 끝] =====

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "사용법: " << argv[0] << " <Jetson IP> <포트> [웹 화면 포트]\n";
        return 1;
    }

    std::string jetsonIP = argv[1];
    int port = std::stoi(argv[2]);
    int webPort = (argc >= 4) ? std::stoi(argv[3]) : 8080; // [추가]

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, jetsonIP.c_str(), &servaddr.sin_addr) <= 0) {
        std::cerr << "잘못된 IP 주소입니다: " << jetsonIP << "\n";
        return 1;
    }

    // ===== [추가] 브라우저 화면용 웹 서버 (내 PC에서만 접속 가능) =====
    signal(SIGPIPE, SIG_IGN);
    int websock = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(websock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in webaddr{};
    webaddr.sin_family = AF_INET;
    webaddr.sin_port = htons(webPort);
    webaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(websock, (struct sockaddr*)&webaddr, sizeof(webaddr)) < 0 || listen(websock, 8) < 0) {
        perror("웹 화면 포트 열기 실패");
        return 1;
    }
    // ===== [추가 끝] =====

    std::cout << "\"호출\" 입력 시 내 IP를 Jetson(" << jetsonIP << ":" << port
              << ")으로 전송합니다. 종료: Ctrl+C\n";
    std::cout << "브라우저 화면: http://localhost:" << webPort << std::endl; // [추가]

    // [변경] 터미널 입력과 브라우저 요청을 함께 기다린다.
    struct pollfd fds[2] = { { STDIN_FILENO, POLLIN, 0 }, { websock, POLLIN, 0 } };
    std::string line;
    while (true) {
        if (poll(fds, 2, -1) < 0) continue;

        if (fds[0].revents & (POLLIN | POLLHUP)) {          // 기존: 터미널 입력
            if (!std::getline(std::cin, line)) fds[0].fd = -1; // 입력이 닫히면 터미널 감시만 중단
            else if (line == "호출") {
                std::string myIP, payload, error;
                callJetson(sockfd, servaddr, jetsonIP, port, myIP, payload, error);
            }
        }
        if (fds[1].revents & POLLIN) {                     // 추가: 브라우저 버튼
            int fd = accept(websock, nullptr, nullptr);
            if (fd >= 0) {
                handleWeb(fd, sockfd, servaddr, jetsonIP, port);
                close(fd);
            }
        }
    }

    close(websock);
    close(sockfd);
    return 0;
}