#!/bin/sh

if [ "$REQUEST_METHOD" = POST ]; then
    input=$(head -c "${CONTENT_LENGTH:-0}")
else
    input=$QUERY_STRING
fi

name=$(printf '%s' "$input" | sed -n 's/^\(.*&\)\{0,1\}name=\([^&]*\).*$/\2/p' | tr '+' ' ' | tr -cd 'A-Za-z0-9 _-' | cut -c1-32)

if [ -z "$name" ]; then
    name=stranger
fi

printf 'Content-Type: text/html; charset=utf-8\r\n\r\n'

cat <<EOF
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <title>cat.sh</title>
    <style>
        body {
            min-height: 100vh;
            margin: 0;
            display: grid;
            place-items: center;
            font-family: monospace;
            background: #fafafa;
        }

        main {
            text-align: center;
        }

        .cat {
            display: inline-block;
            font-size: 64px;
            background: linear-gradient(90deg, #8b5cf6, #ec4899, #f59e0b, #10b981, #3b82f6, #8b5cf6);
            background-size: 200% 100%;
            -webkit-background-clip: text;
            background-clip: text;
            color: transparent;
            transform-origin: 50% 100%;
            animation: shimmer 3s linear infinite, spring 1.8s cubic-bezier(0.28, 0.84, 0.42, 1) infinite;
        }

        @keyframes shimmer {
            from { background-position: 0% 0; }
            to { background-position: 200% 0; }
        }

        @keyframes spring {
            0% { transform: translateY(0) scale(1, 1); }
            12% { transform: translateY(0) scale(1.18, 0.82); }
            32% { transform: translateY(-28px) scale(0.9, 1.12); }
            50% { transform: translateY(0) scale(1.08, 0.92); }
            60% { transform: translateY(-8px) scale(0.98, 1.03); }
            70% { transform: translateY(0) scale(1, 1); }
            100% { transform: translateY(0) scale(1, 1); }
        }

        h1 {
            margin: 24px 0 16px;
            font-size: 24px;
            font-weight: 400;
        }

        table {
            margin: 0 auto 24px;
            border-collapse: collapse;
            text-align: left;
        }

        td {
            padding: 2px 12px;
        }

        td:first-child {
            color: #8b5cf6;
        }
    </style>
</head>
<body>
    <main>
        <div class="cat">₍^. .^₎⟆</div>
        <h1>hello, $name</h1>
        <table>
            <tr><td>REQUEST_METHOD</td><td>$REQUEST_METHOD</td></tr>
            <tr><td>QUERY_STRING</td><td>$(printf '%s' "$QUERY_STRING" | tr -cd 'A-Za-z0-9=&+_-')</td></tr>
            <tr><td>CONTENT_LENGTH</td><td>${CONTENT_LENGTH:-}</td></tr>
            <tr><td>SCRIPT_NAME</td><td>$SCRIPT_NAME</td></tr>
            <tr><td>SERVER_PROTOCOL</td><td>$SERVER_PROTOCOL</td></tr>
        </table>
        <form method="post" action="cat.sh">
            <input name="name" placeholder="your name">
            <button>POST</button>
        </form>
    </main>
</body>
</html>
EOF
