# Server behavior

[Back to README](../README.md)

## Request processing

1. The local address and port of the accepted connection select a `server`.
2. The parser validates the request line and headers; the router finds a `location` and resolves its settings.
3. The dispatcher follows this order, stopping as soon as it can produce a response:

   | Order | Check or action |
   | --- | --- |
   | 1 | Check the declared body size for a matching route; reject an oversized body with `413`. |
   | 2 | Check `Expect`; reject unsupported expectations with `417`. |
   | 3 | Return `404` if no location matches. |
   | 4 | Produce the configured `return` response, if present. |
   | 5 | Check allowed methods; return `405` with `Allow` if the method is disallowed. |
   | 6 | Select CGI if the request path has a configured extension. |
   | 7 | Select the upload handler for POST when `upload_path` is configured. |
   | 8 | Select the static file handler. |

4. If a handler is selected, the session receives the body and enforces its size limit, including for chunked bodies and static requests.
5. The response is serialized and sent in parts as the socket becomes ready. Persistent connections can process subsequent requests; pipelined requests are handled in order.

Socket and pipe I/O happens only after `poll` reports readiness; regular files are read and written directly. CGI programs run as separate processes, while the server uses one event loop.

## Protocol and deliberate choices

The project uses nginx as a reference for behavior, and Apache where nginx has no equivalent, with deliberate exceptions noted below. The implemented protocol and configuration are a subset of those servers' features.

- Only HTTP/1.1 is supported. A syntactically valid HTTP/1.0 request receives `505`; tools such as `ab` that send HTTP/1.0 are therefore unsuitable for testing successful responses here.
- HEAD must be explicitly listed in `methods`; allowing GET does not enable HEAD automatically.
- A body receive timeout produces `408` and closes the connection. Incomplete request headers time out by closing the connection without an HTTP response.
- Virtual hosts based on `Host` were deliberately omitted because the subject marks them as out of scope. Servers are selected by the connection's local address and port, so `server_name` is not supported.
- A directory without an index and with autoindex disabled returns `403`. CGI `PATH_INFO` is empty because this router uses the request path as the script path and does not split out a path-info suffix. `return` responds before consuming the body. The external tester expects different behavior in these cases; see [the documented compatibility switches](testing.md#42-tester).

## Request paths and file operations

A request path must start with `/`. The parser decodes percent escapes and rejects malformed escapes, `.` and `..` path components, repeated slashes (`//`), encoded `/` or backslash, and raw or encoded control characters with `400`. These path rules apply before routing. The query string is separated from the path.

Filesystem paths are checked against the effective root. DELETE handled by the file handler can remove any allowed file under that location's root, not just uploaded files. For a symbolic link, DELETE removes the link itself rather than its target. Directory deletion returns `403`.

## Limits and timeouts

These values are fixed in the implementation unless a configuration directive is named.

| Limit | Value | Result |
| --- | --- | --- |
| Request line | 8192 bytes (8 KiB) | `414` when exceeded. |
| Request headers | 32768 bytes (32 KiB) | `431` when exceeded. |
| Request body | `client_max_body_size`, default `1m` | `413` and connection closure when enforced; see [body size handling](config.md#client_max_body_size-request-body-size). |
| Header completion | 20 seconds from the start of the request | Close without a response if headers remain incomplete. |
| Body receive inactivity | 60 seconds without incoming data | `408`, then close. A body that keeps arriving is not limited to 60 seconds total. |
| Idle keep-alive | 60 seconds | Close the idle connection. |
| Response send inactivity | 60 seconds with pending output and no progress | Close the connection. |
| CGI output inactivity | 60 seconds | `504` before response headers have been sent; otherwise close the connection. |
| Concurrent CGI processes | 128 | An additional CGI request receives `503`. |
| Client connections | 1000 | Pause accepting new clients until capacity is available. |

CGI startup failures or unusable CGI output produce `502` before a response has started. Once response headers have been sent, CGI failures close the connection because the status can no longer be replaced.

## Source layout

| Directory | Responsibility |
| --- | --- |
| [app/](../app/) | Entry point: loading the configuration and starting the server. |
| [src/config/](../src/config/) | Reading, parsing, and validating configuration files. |
| [src/net/](../src/net/) | Sockets, connections, `poll`, the event loop, and timeouts. |
| [src/http/](../src/http/) | HTTP parsing, sessions, routing, handlers, and response construction. |
| [src/cgi/](../src/cgi/) | Launching CGI processes, handling pipes, and tracking processes. |
| [src/fs/](../src/fs/) | Path and file descriptor utilities. |
| [src/log/](../src/log/) | Diagnostic messages. |
| [public/](../public/) | Demo pages, files, CGI scripts, and the upload directory. |
| [config/](../config/) | Working configuration examples and invalid examples. |
| [tests/](../tests/) | Component tests and server behavior tests. |
