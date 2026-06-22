#!/usr/bin/env bash
# Headless graphics E2E for the Linux GUI build.
#
# Starts `axiom -gr -ht -ihere` under a pty (sman does isatty/termios setup, so a
# bare pipe is not enough -- a pipe leaves the interpreter without a controlling
# tty and no viewport is ever mapped), issues draw(sin(x), x=0..2*%pi), and
# asserts the X11 viewport window appears -- i.e. the interpreter reached viewman
# over the socket FFI and viewman mapped a window.  Run under a live DISPLAY
# (Xvfb in CI).
#
# This is the one graphics test the macOS CI cannot run: the macOS runner has no
# X server, so mac-gui-build only proves the GUI compiles + links.  Xvfb gives the
# Linux runner a real framebuffer, so we exercise the whole draw pipeline.
#
# The pty driver is Python (heredoc below) only because pty allocation + timed
# read is a few clean lines there; it is data to this shell script, so it does
# not need an owning literate source.
set -eu

python3 - <<'PYEOF'
import os
import pty
import select
import subprocess
import sys
import time

AXIOM = os.environ.get("AXIOM", os.path.join(os.getcwd(), "mnt/LINUX"))
WRAPPER = os.path.join(AXIOM, "bin", "axiom")

master, slave = pty.openpty()
proc = subprocess.Popen(
    [WRAPPER, "-gr", "-ht", "-ihere", "-noclef"],
    stdin=slave, stdout=slave, stderr=slave, env=dict(os.environ), close_fds=True,
)
os.close(slave)

buf = b""


def pump(seconds):
    """Drain the pty for SECONDS, accumulating output into BUF."""
    global buf
    deadline = time.time() + seconds
    while time.time() < deadline:
        ready, _, _ = select.select([master], [], [], 0.5)
        if ready:
            try:
                buf += os.read(master, 8192)
            except OSError:
                break


pump(40)                                   # banner + viewman/hypertex startup + prompt
os.write(master, b"draw(sin(x),x=0..2*%pi)\n")
pump(15)                                    # let viewman map + draw the viewport

windows = subprocess.run(
    ["xwininfo", "-root", "-tree"], env=dict(os.environ),
    capture_output=True, text=True,
).stdout

os.write(master, b")quit\n")
time.sleep(2)
try:
    proc.terminate()
except Exception:
    pass

transcript = buf.decode(errors="replace")
have_window = ('"Axiom 2D"' in windows) or ('"2D Viewport"' in windows)
have_transmit = "transmitted to the viewport manager" in transcript

print("viewport window present :", have_window)
print("viewport transmit logged:", have_transmit)

if not (have_window and have_transmit):
    print("---- xwininfo -root -tree ----")
    print(windows)
    print("---- transcript tail ----")
    print(transcript[-2000:])
    sys.exit(1)

print("PASS: draw(sin(x)) created an X11 viewport under Xvfb")
PYEOF
