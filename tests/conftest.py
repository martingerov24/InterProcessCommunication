import os, re, time, socket, subprocess, pathlib, queue, threading, glob
import pytest

ROOT  = pathlib.Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"

def _is_exec(p: pathlib.Path) -> bool:
    return p.is_file() and os.access(p, os.X_OK)

def _find_bin(name: str) -> pathlib.Path:
    env = os.environ.get(f"{name.upper()}_BIN")
    if env:
        p = pathlib.Path(env)
        assert _is_exec(p), f"{name} not executable at {p}"
        return p
    candidates = [
        BUILD / "bin" / name,
        BUILD / name,
        *[pathlib.Path(p) for p in glob.glob(str(BUILD / "**" / name), recursive=True)],
    ]
    for p in candidates:
        if _is_exec(p): return p
    raise AssertionError(f"Could not find {name} under {BUILD}. "
                         f"Override with {name.upper()}_BIN=/abs/path")

SERVER_BIN  = _find_bin("server")
CLIENT1_BIN = _find_bin("client_1")
CLIENT2_BIN = _find_bin("client_2")
DEFAULT_PORT = 24737

class LiveReader:
    def __init__(self, proc: subprocess.Popen):
        self.proc = proc
        self.q = queue.Queue()
        self._buf = []
        self.t = threading.Thread(target=self._pump, daemon=True)
        self.t.start()
    def _pump(self):
        for line in self.proc.stdout:
            self._buf.append(line)
            self.q.put(line)
    def read_until(self, pattern: re.Pattern, timeout=8.0) -> str:
        end = time.time() + timeout
        acc = ""
        while time.time() < end:
            try:
                line = self.q.get(timeout=0.2)
                acc += line
                if pattern.search(acc):
                    return acc
            except queue.Empty:
                pass
            if self.proc.poll() is not None and self.q.empty():
                break
        return acc
    def dump(self) -> str:
        return "".join(self._buf)

def _probe_tcp(host: str, port: int, timeout_s=6.0) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(0.25)
            if s.connect_ex((host, port)) == 0:
                return True
        time.sleep(0.05)
    return False

@pytest.fixture(scope="session")
def server(tmp_path_factory):
    """Start server with NO ARGS. Parse 'Server running at <endpoint>'."""
    log = "./build/server_log/log.txt"
    proc = subprocess.Popen(
        [str(SERVER_BIN)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )
    rd = LiveReader(proc)

    rx = re.compile(r"Server\s+running\s+at\s+(tcp://([0-9\.]+):(\d+))", re.I)
    out = rd.read_until(rx, timeout=10)
    m = rx.search(out)
    if m:
        endpoint, host, port = m.group(1), m.group(2), int(m.group(3))
        ok = _probe_tcp(host, port, timeout_s=4.0)
        if not ok:
            proc.kill()
            raise RuntimeError(f"Parsed endpoint '{endpoint}' but port didn’t open.\n{rd.dump()}")
    else:
        host, port = "127.0.0.1", DEFAULT_PORT
        if not _probe_tcp(host, port, timeout_s=8.0):
            proc.kill()
            raise RuntimeError(f"Server failed to report address and default port {port} not open.\n{rd.dump()}")

    yield {"proc": proc, "host": host, "port": port, "reader": rd}

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

class InteractiveProc:
    def __init__(self, argv, prompt_regex=r">>\s"):
        self.proc = subprocess.Popen(
            argv,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )
        self.prompt_re = re.compile(prompt_regex)
        self.q = queue.Queue()
        self.t = threading.Thread(target=self._pump, daemon=True)
        self.t.start()
    def _pump(self):
        for line in self.proc.stdout:
            self.q.put(line)
    def send(self, line: str):
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()
    def until_prompt(self, timeout=5.0) -> str:
        end = time.time() + timeout
        acc = ""
        while time.time() < end:
            try:
                line = self.q.get(timeout=0.2)
                acc += line
                if self.prompt_re.search(line):
                    return acc
            except queue.Empty:
                pass
            if self.proc.poll() is not None and self.q.empty():
                break
        return acc
    def until_re(self, pattern, timeout=5.0) -> str:
        rx = re.compile(pattern, re.I | re.M)
        end = time.time() + timeout
        acc = ""
        while time.time() < end:
            try:
                line = self.q.get(timeout=0.2)
                acc += line
                if rx.search(acc): return acc
            except queue.Empty:
                pass
            if self.proc.poll() is not None and self.q.empty():
                break
        return acc
    def close(self):
        if self.proc.poll() is None:
            try: self.send("exit")
            except Exception: pass
            try: self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.terminate()
                try: self.proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.proc.kill()

@pytest.fixture
def client1(server):
    argv = [str(CLIENT1_BIN), "--address", server["host"], "--port", str(server["port"])]
    ip = InteractiveProc(argv)
    banner = ip.until_prompt(timeout=5)
    assert "Client started" in banner
    yield ip
    ip.close()

@pytest.fixture
def client2(server):
    argv = [str(CLIENT2_BIN), "--address", server["host"], "--port", str(server["port"])]
    ip = InteractiveProc(argv)
    banner = ip.until_prompt(timeout=5)
    assert "Client started" in banner
    yield ip
    ip.close()
