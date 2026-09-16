#!/usr/bin/env python3
"""Mac/Linux USB serial console using only the Python standard library."""
import argparse
import glob
import os
import select
import sys
import termios
import time
import tty


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='Serial port; autodetects a single USB port on macOS')
    parser.add_argument('--command', '-c', action='append', help='Send a command (repeatable)')
    parser.add_argument('--watch', type=float, default=20, help='Seconds to read after commands; default 20')
    args = parser.parse_args()
    ports = sorted(glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/cu.usbserial*'))
    if not args.port:
        if len(ports) != 1:
            parser.error('Specify --port; detected: ' + (', '.join(ports) or 'none'))
        args.port = ports[0]
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    previous = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)
        attrs[4] = attrs[5] = termios.B115200
        attrs[2] |= termios.CLOCAL | termios.CREAD
        attrs[2] &= ~termios.HUPCL
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        pending = bytearray()
        if args.command:
            pending.extend(('\n'.join(args.command) + '\n').encode('ascii'))
        else:
            print('Connected. Type help then Enter; Ctrl-C exits. Close this console before uploading.', file=sys.stderr)
        deadline = time.monotonic() + args.watch if args.command else None
        while deadline is None or time.monotonic() < deadline:
            inputs = [fd] if args.command else [fd, sys.stdin]
            readable, writable, _ = select.select(inputs, [fd] if pending else [], [], 0.1)
            if fd in writable:
                try:
                    count = os.write(fd, pending)
                    del pending[:count]
                except BlockingIOError:
                    pass
            if fd in readable:
                try:
                    data = os.read(fd, 4096)
                except BlockingIOError:
                    continue
                if not data:
                    break
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            if sys.stdin in readable:
                line = sys.stdin.readline()
                if not line:
                    break
                pending.extend(line.encode('ascii', errors='replace'))
    finally:
        try:
            termios.tcsetattr(fd, termios.TCSANOW, previous)
        except termios.error:
            pass  # USB may disconnect after a reboot/sleep command.
        os.close(fd)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        pass
    except OSError as exc:
        sys.exit(f'USB connection failed: {exc}')
