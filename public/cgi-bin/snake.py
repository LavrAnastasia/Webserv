#!/usr/bin/env python3
import os
import sys
import time
from urllib.parse import parse_qs

WIDTH = 24
HEIGHT = 8
DELAY = 0.06

query = parse_qs(os.environ.get("QUERY_STRING", ""))

try:
    frames = int(query.get("frames", ["120"])[0])
except ValueError:
    frames = 120

frames = max(1, min(frames, 1000))

html = "text/html" in os.environ.get("HTTP_ACCEPT", "")

path = [(x if y % 2 == 0 else WIDTH - 1 - x, y) for y in range(HEIGHT) for x in range(WIDTH)]

if html:
    sys.stdout.write("Content-Type: text/html; charset=utf-8\n\n")
    sys.stdout.write(
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>snake.py</title><style>"
        "body { margin: 0; background: #fafafa; }"
        "pre { position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); margin: 0;"
        " font-size: 24px; line-height: 1.1; color: #8b5cf6; background: #fafafa; }"
        "</style></head><body>\n"
    )
else:
    sys.stdout.write("Content-Type: text/plain; charset=utf-8\n\n")

length = 4
food = 15

for step in range(frames):
    head = step % len(path)

    if head == food:
        length = min(length + 3, len(path) - 1)
        food = (food + 37) % len(path)

    body = {path[(head - i) % len(path)] for i in range(1, length)}

    lines = ["+" + "-" * WIDTH + "+"]

    for y in range(HEIGHT):
        row = ""

        for x in range(WIDTH):
            if (x, y) == path[head]:
                row += "@"
            elif (x, y) in body:
                row += "o"
            elif (x, y) == path[food]:
                row += "*"
            else:
                row += " "

        lines.append("|" + row + "|")

    lines.append("+" + "-" * WIDTH + "+")
    lines.append(f"length {length}   frame {step + 1}/{frames}".ljust(WIDTH + 2))

    if html:
        sys.stdout.write("<pre>" + "\n".join(lines) + "</pre>\n")
    else:
        sys.stdout.write("\033[H\033[2J" + "\n".join(lines) + "\n")

    sys.stdout.flush()
    time.sleep(DELAY)

if html:
    sys.stdout.write("</body></html>\n")
