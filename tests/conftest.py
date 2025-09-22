import os, re, time, socket, subprocess, pathlib, queue, threading, glob
import pytest

# Define the root and build directories relative to the current file.
ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"

def _is_exec(p: pathlib.Path) -> bool:
    """Checks if a given path is an executable file."""
    return p.is_file() and os.access(p, os.X_OK)

def _find_bin(name: str) -> pathlib.Path:
    """
    Locates an executable binary file by its name.

    First, it checks for an environment variable like `SERVER_BIN`.
    If not found, it searches in common build directories (e.g., `build/bin`,
    `build/name`, or recursively under `build/**`).
    Raises an AssertionError if the binary cannot be found.
    """
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
        if _is_exec(p):
            return p
    raise AssertionError(f"Could not find {name} under {BUILD}. "
                         f"Override with {name.upper()}_BIN=/abs/path")

# Find the paths to the server and client executables.
SERVER_BIN = _find_bin("server")
CLIENT1_BIN = _find_bin("client_1")
CLIENT2_BIN = _find_bin("client_2")
DEFAULT_PORT = 24737

class LiveReader:
    """
    A utility class to read and capture output from a subprocess in real-time.

    It uses a separate thread to continuously pump the process's stdout into a queue,
    allowing other threads to read from the queue without blocking.
    """
    def __init__(self, proc: subprocess.Popen):
        self.proc = proc
        self.q = queue.Queue()
        self._buf = []
        # Start a daemon thread to read the output.
        self.t = threading.Thread(target=self._pump, daemon=True)
        self.t.start()

    def _pump(self):
        """Thread function to read lines from the process's stdout and put them into the queue."""
        for line in self.proc.stdout:
            self._buf.append(line)
            self.q.put(line)

    def read_until(self, pattern: re.Pattern, timeout=8.0) -> str:
        """
        Reads from the queue until a specified regex pattern is found or a timeout occurs.
        Returns the accumulated output as a string.
        """
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
            # If the process has terminated and the queue is empty, break the loop.
            if self.proc.poll() is not None and self.q.empty():
                break
        return acc

    def dump(self) -> str:
        """Returns all captured output as a single string."""
        return "".join(self._buf)

def _probe_tcp(host: str, port: int, timeout_s=6.0) -> bool:
    """
    Probes a TCP port to check if it is open and listening.
    Retries multiple times within the given timeout.
    """
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(0.25)
            # connect_ex returns 0 on success.
            if s.connect_ex((host, port)) == 0:
                return True
        time.sleep(0.05)
    return False

@pytest.fixture(scope="session")
def server(tmp_path_factory):
    """
    A Pytest fixture that starts the server process before tests run and stops it afterwards.
    The scope is "session", meaning the server is started once for all tests in the session.
    """
    proc = subprocess.Popen(
        [str(SERVER_BIN)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )
    rd = LiveReader(proc)

    # Use a regex to find the server's reported address and port from its output.
    rx = re.compile(r"Server\s+running\s+at\s+(tcp://([0-9\.]+):(\d+))", re.I)
    out = rd.read_until(rx, timeout=10)
    m = rx.search(out)
    if m:
        # Extract the endpoint, host, and port from the regex match.
        endpoint, host, port = m.group(1), m.group(2), int(m.group(3))
        # Probe the TCP port to ensure the server is ready to accept connections.
        ok = _probe_tcp(host, port, timeout_s=4.0)
        if not ok:
            proc.kill()
            raise RuntimeError(f"Parsed endpoint '{endpoint}' but port didn’t open.\n{rd.dump()}")
    else:
        # Fallback to a default host and port if the output doesn't match the regex.
        host, port = "127.0.0.1", DEFAULT_PORT
        if not _probe_tcp(host, port, timeout_s=8.0):
            proc.kill()
            raise RuntimeError(f"Server failed to report address and default port {port} not open.\n{rd.dump()}")

    # The 'yield' keyword makes this a teardown fixture.
    # The dictionary containing the process, host, port, and reader is passed to tests.
    yield {"proc": proc, "host": host, "port": port, "reader": rd}

    # Teardown logic:
    # Terminate the process gracefully, then kill it if it doesn't exit.
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

class InteractiveProc:
    """
    A class to manage and interact with a command-line process.
    It reads output until a specific prompt appears, and sends input to the process.
    """
    def __init__(self, argv, prompt_regex=r">>\s"):
        self.proc = subprocess.Popen(
            argv,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1 # Line-buffered output
        )
        self.prompt_re = re.compile(prompt_regex)
        self.q = queue.Queue()
        self.t = threading.Thread(target=self._pump, daemon=True)
        self.t.start()

    def _pump(self):
        """Thread function to pump stdout into the queue."""
        for line in self.proc.stdout:
            self.q.put(line)

    def send(self, line: str):
        """Sends a line of text to the process's stdin."""
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()

    def until_prompt(self, timeout=5.0) -> str:
        """Reads output until the command-line prompt is found."""
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
        """Reads output until a specified regex pattern is found."""
        rx = re.compile(pattern, re.I | re.M)
        end = time.time() + timeout
        acc = ""
        while time.time() < end:
            try:
                line = self.q.get(timeout=0.2)
                acc += line
                if rx.search(acc):
                    return acc
            except queue.Empty:
                pass
            if self.proc.poll() is not None and self.q.empty():
                break
        return acc

    def close(self):
        """Gracefully shuts down the process by sending 'exit' or killing it."""
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
    """
    A Pytest fixture for the first client. It starts the client process,
    connects it to the server provided by the `server` fixture, and yields
    an `InteractiveProc` object for interaction.
    """
    argv = [str(CLIENT1_BIN), "--address", server["host"], "--port", str(server["port"])]
    ip = InteractiveProc(argv)
    banner = ip.until_prompt(timeout=5)
    assert "Client started" in banner
    # Yield the interactive process object to the test.
    yield ip
    # Teardown logic: close the client process.
    ip.close()

@pytest.fixture
def client2(server):
    """
    A Pytest fixture for the second client, similar to the first one.
    This allows for testing multi-client scenarios.
    """
    argv = [str(CLIENT2_BIN), "--address", server["host"], "--port", str(server["port"])]
    ip = InteractiveProc(argv)
    banner = ip.until_prompt(timeout=5)
    assert "Client started" in banner
    yield ip
    ip.close()