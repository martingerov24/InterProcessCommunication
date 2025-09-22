import re, pytest
pytestmark = pytest.mark.timeout(30)

def send_and_capture(cli, line, expect=None, timeout=5):
    cli.send(line)
    out = cli.until_prompt(timeout=timeout)
    if expect:
        assert re.search(expect, out, re.I | re.M), f"Missing {expect!r} in:\n{out}"
    return out

def test_help_and_usage(client1):
    out = send_and_capture(client1, "help", r"Commands:")
    out = send_and_capture(client1, "block add a b", r"Usage:\s*block\s+add\s+a\s+b")

def test_block_add_and_mult(client1):
    send_and_capture(client1, "block add 50 50",  r"Result:\s*Int=100")
    send_and_capture(client1, "block mult 7 6",   r"Result:\s*Int=42")

def test_nonblock_add_get_nowait_and_wait(client1):
    client1.send("non-block add 50 50")
    out = client1.until_re(r"IPC\s+Info:\s*\[NOT_FINISHED\].*ticket=(\d+)", timeout=5)
    m = re.search(r"ticket=(\d+)", out, re.I)
    assert m, f"no ticket in:\n{out}"
    ticket = m.group(1)

    send_and_capture(client1, f"get {ticket} nowair", r"Invalid\s+wait\s+mode")
    send_and_capture(client1, f"get {ticket} nowait", r"Result:\s*Int=100")

    client1.send("non-block add 50 50")
    out2 = client1.until_re(r"ticket=(\d+)", timeout=5)
    ticket2 = re.search(r"ticket=(\d+)", out2).group(1)
    send_and_capture(client1, f"get {ticket2} wait 500", r"Result:\s*Int=100")

def test_concat_limit_ok_and_error(client1):
    send_and_capture(client1, "block concat hello world", r"Result:\s*Str=hello\s*world")

    long1 = "X"*20
    long2 = "Y"*20
    out = send_and_capture(client1, f"block concat {long1} {long2}", r"(error|invalid|too\s*long)")
