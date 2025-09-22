import re, pytest
pytestmark = pytest.mark.timeout(30)

def send_and_capture(cli, line, expect=None, timeout=5):
    cli.send(line)
    out = cli.until_prompt(timeout=timeout)
    if expect:
        assert re.search(expect, out, re.I | re.M), f"Missing {expect!r} in:\n{out}"
    return out

def test_block_sub_and_findstart(client2):
    send_and_capture(client2, "block sub 10 3", r"Result:\s*Int=7")
    send_and_capture(client2, "block find abracadabra bra", r"Result:\s*(Pos|Int)=\d+")

def test_div_by_zero_error(client2):
    send_and_capture(client2, "block div 6 0", r"(ERROR_DIV_BY_ZERO|div\s*by\s*0|invalid)")
