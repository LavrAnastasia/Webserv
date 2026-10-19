*This project has been created as part of the 42 curriculum by jjahkola, alavrukh, audobnai.*

# webserv

## Description

A single-threaded HTTP/1.1 server written in C++20. The networking layer, HTTP parser, router, handlers, and configuration loader are all implemented in this repository.

The server handles multiple clients in one event loop using `poll`. Socket and pipe I/O happens only after `poll` reports readiness; regular files are read and written directly. CGI programs run as separate processes.

Features include static files, directory indexes and listings, GET/HEAD/POST/DELETE, raw file uploads, CGI, redirects, custom error pages, persistent connections, pipelined requests, and chunked bodies. Multiple listening addresses and servers can be configured in one file.

Only HTTP/1.1 is supported; HTTP/1.0 requests receive `505`. Virtual hosts selected by the `Host` header were deliberately omitted because the subject marks them as out of scope. Without virtual hosts, `server_name` would have no effect, so it is not supported. A server is selected only by the local address and port of the connection.

The project uses nginx as a reference for behavior, and Apache where nginx has no equivalent, with deliberate exceptions described in [server behavior](docs/behavior.md).

| Documentation | Contents |
| --- | --- |
| [Configuration reference](docs/config.md) | Every directive, defaults, path resolution, and the purpose of all configuration examples. |
| [Server behavior](docs/behavior.md) | Request processing, protocol choices, path rules, limits, timeouts, and source layout. |
| [Testing and development](docs/testing.md) | Manual checks, GoogleTest, external tester compatibility, and project tooling. |

## Instructions

### Build and run

Requirements: Linux or macOS, `make`, and a C++ compiler with C++20 support. The demo CGI scripts require Python 3 and `/bin/sh`; check the interpreter paths in the configuration for your system.

Run from the repository root so relative paths such as `root ./public` resolve correctly:

```sh
make
./webserv config/demo.conf
```

Open [http://127.0.0.1:8080/](http://127.0.0.1:8080/). The demo also listens on `127.0.0.1:8081`; a second server on port `8082` responds with `second-server`.

The configuration path is the single required argument; there is no default path or `-t` validation mode. Messages are written to stderr. Stop with `Ctrl+C` or `SIGTERM`, and restart after changing the configuration.

### Run tests

GoogleTest is required for automated tests. `pkg-config` supplies compiler and linker flags when available; a custom installation can be selected with `GTEST_ROOT`.

```sh
make test
make test-router
```

See [testing and development](docs/testing.md) for individual test targets, `curl` examples, and build/formatting commands.

### Prepare the 42 tester

Replace `/path/to/cgi_tester` with the path to the CGI tester executable for your platform:

```sh
config/42.test.sh /path/to/cgi_tester
./webserv config/42.test.conf
```

Run these commands from the repository root. The script creates `YoupiBanane/`, copies the executable to `/tmp/cgi_tester`, and makes it executable. Run the main HTTP tester in another terminal while the server is running.

With the committed server code, some checks fail: `cgi_tester` expects a nonempty `PATH_INFO`, a directory without an index is expected to return `404` instead of `403`, and the chunked body limit at `/post_body` is bypassed by an immediate `return`. The code has three commented tester alternatives (`PATH_INFO`, `NotFound`, and `Discard`), deliberately left disabled. Read [the compatibility decisions and switches](docs/testing.md#known-incompatibilities-and-commented-switches) before using the tester.

## Resources

- [HTTP semantics — RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html): methods, response statuses, and headers.
- [HTTP/1.1 — RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html): message syntax, body framing, and connection handling.
- [CGI/1.1 — RFC 3875](https://www.rfc-editor.org/rfc/rfc3875.html): CGI execution and environment variables, including `PATH_INFO`.
- [nginx core module](https://nginx.org/en/docs/http/ngx_http_core_module.html) and [index module](https://nginx.org/en/docs/http/ngx_http_index_module.html): reference behavior for configuration and file serving.
- [Apache custom error responses](https://httpd.apache.org/docs/current/custom-error.html): error handling reference.

### AI use

We used AI to review code, explain system calls and HTTP, suggest tests, and help write the docs. All decisions and all merged code were made and checked by the team.
