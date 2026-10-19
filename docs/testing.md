# Testing and development

[Back to README](../README.md)

## 42 tester

From the repository root, prepare the fixture directories and start the server:

```sh
make
config/42.test.sh /path/to/cgi_tester
./webserv config/42.test.conf
```

Replace `/path/to/cgi_tester` with the path to the CGI tester executable supplied for your platform. The script creates the required files under `YoupiBanane/`, copies the executable to `/tmp/cgi_tester`, and makes it executable. Re-running it preserves existing fixture contents but replaces `/tmp/cgi_tester`. `YoupiBanane/` is ignored by Git. The CGI tester and the main HTTP tester are separate programs: run the main tester in another terminal while the server is running, using its supplied instructions.

The fixture layout is:

```text
YoupiBanane/
├── youpi.bad_extension
└── directory/
    ├── youpi.bad_extension
    ├── youpi.bla
    ├── nop/
    │   ├── youpi.bad_extension
    │   └── other.pouic
    └── Yeah/
        └── not_happy.bad_extension
```

### Known incompatibilities and commented switches

With the committed server code, some tester checks fail. Preparing the fixtures does not enable the compatibility switches. All three changes below are needed for the corresponding tester checks; enabling them is not a claim that every possible tester check passes.

| Case | Committed behavior and tester expectation | Commented alternative |
| --- | --- | --- |
| CGI through `cgi_tester` | `PATH_INFO` is empty; the tester expects the request path. | In [CgiEnvironment.cpp](../src/http/src/CgiEnvironment.cpp), change the `PATH_INFO` value from the empty string to `request.path` at the tester marker. |
| Directory without an index | The server returns `403` when autoindex is off; the tester expects `404`. | In [StaticHandler.cpp](../src/http/src/StaticHandler.cpp), use `HttpStatus::NotFound` instead of `HttpStatus::Forbidden` at the tester marker in `handleDirectoryRequest`. |
| Chunked body limit at `/post_body` | `return 200` answers immediately, so the handler-based chunked size check is skipped; the tester expects the 100-byte limit to be enforced. | In [RequestDispatcher.cpp](../src/http/src/RequestDispatcher.cpp), uncomment the marked `if (!length)` branch that wraps the response in `Discard`. This receives and discards the body while allowing the session to enforce the limit before returning the configured response. |

The team deliberately left these alternatives disabled because it disagrees with these tester expectations: `PATH_INFO` should describe the suffix after the script path, a directory that exists but cannot be listed warrants `403`, and `return` is intended to respond immediately. The current router does not support a path-info suffix, so it supplies an empty `PATH_INFO`. These are explicit compatibility decisions; the tester is useful for finding issues but does not define all server behavior.

If you choose to enable the marked alternatives for evaluation, rebuild with `make` and restart the server. The setup script only prepares fixtures; it does not modify C++ sources.

## Manual checks

Start `./webserv config/demo.conf`, then run the following in another terminal:

```sh
curl -i http://127.0.0.1:8080/
curl -I http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/guide
curl -i http://127.0.0.1:8080/files/
curl -i http://127.0.0.1:8080/ping
curl -i http://127.0.0.1:8080/old
curl -i http://127.0.0.1:8080/missing
```

Expected results, in order: the home page, headers without a body, a `301` redirect to `/guide/`, a directory listing, `200` with `pong`, a `301` redirect to `/`, and the custom `404` page.

Create, retrieve, and delete a file:

```sh
curl -i --data-binary 'hello' http://127.0.0.1:8080/uploads/hello.txt
curl -i http://127.0.0.1:8080/uploads/hello.txt
curl -i -X DELETE http://127.0.0.1:8080/uploads/hello.txt
```

If `hello.txt` does not already exist, the responses are `201`, `200` with the body `hello`, and `204`, respectively.

Check CGI:

```sh
curl -N 'http://127.0.0.1:8080/cgi-bin/snake.py?frames=30'
curl 'http://127.0.0.1:8080/cgi-bin/cat.sh?name=Anna'
curl -d 'name=Anna' http://127.0.0.1:8080/cgi-bin/cat.sh
```

The `-N` option disables `curl` output buffering so you can see the streaming response as it arrives.

## Automated tests

Building the tests requires GoogleTest; `pkg-config` is used to discover compiler and linker flags when available. CI installs `libgtest-dev` and `pkg-config` on Linux, and `googletest` and `pkgconf` on macOS.

```sh
make test
make test-config_loader
make test-router
make test-http_session
make test-server
```

Individual targets let you check the configuration loader, routing, HTTP sessions, or the server through real connections. The other groups are listed in [tests/tests.mk](../tests/tests.mk): HTTP parsing, serialization, response bodies (`response_body`), the static handler, connections, and CGI.

For a custom GoogleTest installation, specify the directory containing `include/` and `lib/`:

```sh
make test GTEST_ROOT=/path/to/gtest
```

The project tests observable results: statuses, headers, response bodies, files, and connection behavior. When adding a fix, a new test must reproduce the issue and fail without that fix.

## Build and development commands

| Command | Purpose |
| --- | --- |
| `make` | Build the `webserv` executable. |
| `make clean` | Remove the `build/` directory. |
| `make fclean` | Remove the build directory and `webserv`. |
| `make re` | Perform a full rebuild. |
| `make test` | Build and run all tests. Requires GoogleTest. |
| `make format-check` | Check C++ formatting in `src/`. Requires `clang-format`. |
| `make format` | Format C++ files in `src/`, modifying them in place. |

## Project tooling configuration

| File | Purpose |
| --- | --- |
| [Makefile](../Makefile) | Defines the C++20 build with `-Wall -Wextra -Werror`, object dependencies, and cleanup/formatting commands. |
| [tests/tests.mk](../tests/tests.mk) | Integrates GoogleTest, builds separate test executables, and defines the `test` and `test-<name>` targets. |
| [.clang-format](../.clang-format) | Defines the C++ style: LLVM base, 4-space indentation, a 120-character line limit, and brace/line wrapping rules. |
| [.editorconfig](../.editorconfig) | Shared editor settings: UTF-8, LF, a final newline, trailing whitespace removal, and tabs for Makefile. |
| [.pre-commit-config.yaml](../.pre-commit-config.yaml) | Pre-commit checks for file size, merge conflict markers, whitespace, final newlines, C++ formatting, and EditorConfig compliance. Some hooks modify files. |
| [.github/workflows/checks.yml](../.github/workflows/checks.yml) | CI for pull requests and pushes to `main`/`master`: pre-commit style checks and a build on Linux, tests on Linux and macOS. |
| [.gitignore](../.gitignore) | Excludes build outputs, temporary files, and local tool settings from Git. |
| [config/42.test.sh](../config/42.test.sh) | Prepares the external tester fixtures and copies its CGI executable to `/tmp/cgi_tester`. |
| [public/uploads/.gitignore](../public/uploads/.gitignore) | Keeps the upload directory in the repository while excluding files uploaded through the server. |
