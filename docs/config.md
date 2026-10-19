# Configuration reference

[Back to README](../README.md)

## Format

The top level contains `server` blocks, each containing server directives and `location` blocks. Directives end with `;`, and blocks are enclosed in `{` and `}`. The `#` character starts a comment that continues to the end of the line.

Arguments are separated by whitespace. Quoting, escaping, variables, and `include` are not supported: for example, `return 200 hello;` is valid, but quotes cannot turn text containing spaces into a single argument. The configuration file size is limited to 1 MiB, or 1048576 bytes.

Minimal configuration:

```nginx
server {
    listen 127.0.0.1:8080;
    root ./public;

    location / {
        methods GET;
    }
}
```

Required elements: at least one `server`; `listen`, `root`, and at least one `location` in each server; and a nonempty `methods` list in each `location`.

## Selecting `server` and `location`

A `server` defines settings for its `listen` addresses. Virtual hosts, which select a server using the `Host` header, were deliberately left out because the subject marks them as out of scope. Without virtual hosts, `server_name` would have no effect, so it is not supported. A server is selected only by the local address and port of the connection.

A `location /path` defines rules for `/path` and its descendants. The longest match is selected at a path segment boundary: `/docs` matches `/docs` and `/docs/page.html`, but not `/docs-old`. Block order does not affect selection, and path matching is case-sensitive. `location /` matches every path; if no block matches, the server responds with `404`.

Regular expressions, `=`/`~` modifiers, and nested `location` blocks are not supported. A `/docs/` block matches `/docs/` and its descendants, but not `/docs`.

`root`, `index`, and `client_max_body_size` come from the selected `location`, falling back to the `server` settings when omitted. Settings are not inherited from less specific locations: `/api/v1` does not inherit settings from `/api`.

## Directives

| Directive | Allowed context | Default | Purpose |
| --- | --- | --- | --- |
| `listen <host>:<port>;` | `server` | None; required | Set the server's network address and port. Multiple addresses are allowed. |
| `root <path>;` | `server`, `location` | Required in `server`; inherited in `location` | Set the filesystem root for the route's resources. |
| `index <path>;` | `server`, `location` | `index.html`; inherited in `location` | Select the index using one relative path, such as `index.html` or `sub/start.html`. |
| `client_max_body_size <size>;` | `server`, `location` | `1m`; inherited in `location` | Limit the request body size. |
| `error_page <code> [<code> ...] <path>;` | `server` | Built-in HTML error page | Assign a custom page to one or more error codes. |
| `methods <method> [<method> ...];` | `location` | None; required | Set the methods allowed by the route's handler. |
| `autoindex on;` / `autoindex off;` | `location` | `off` | List directory contents when no index file exists. |
| `upload_path <path>;` | `location` | Uploads disabled | Set the directory where POST saves the request body. |
| `return <code> [<value>];` | `location` | No configured response | Return a redirect, text, an error page, or an empty response. |
| `cgi <extension> <interpreter>;` | `location` | CGI disabled | Associate a file extension with the program that executes it. |

Only `listen`, `error_page`, and `cgi` may appear more than once in the same block. The same listening address, error code, or CGI extension cannot be assigned twice. Location paths within a server must also be unique.

A `location` may use at most one of `return`, `upload_path`, or `cgi`. Multiple `cgi` directives for different extensions are allowed.

## `listen`: address and port

The format is strictly `host:port`. IPv4 addresses and the name `localhost`, which resolves to `127.0.0.1`, are supported. Ports range from `1` to `65535`.

```nginx
listen 127.0.0.1:8080;
listen localhost:8081;
listen 0.0.0.0:8082;
```

`127.0.0.1` accepts local connections; `0.0.0.0` accepts connections on all IPv4 interfaces. A bare port such as `8080`, arbitrary DNS names, and IPv6 are not supported in `listen`. IPv4 octets with leading zeros are rejected. Leading zeros in a port are accepted: `:08080` means port `8080`. `localhost:8080` and `127.0.0.1:8080` are the same endpoint.

Addresses are checked for conflicts across the entire configuration. The same `host:port` cannot be used twice, and `0.0.0.0:8080` cannot be combined with a specific address on port `8080`. Different specific IPv4 addresses on the same port are allowed.

## `root` and path resolution

`root` may be an absolute or relative path. A regular file is resolved as `root + request path`: the `location` prefix remains part of the path.

```nginx
location /docs {
    methods GET;
    root ./public/archive;
}
```

In this example, a request for `/docs/page.html` accesses `./public/archive/docs/page.html`.

Path resolution rules for the three filesystem directives:

| Setting | Resolved path | Source of `root` |
| --- | --- | --- |
| `upload_path` | `root + upload_path` is the upload directory | The request's `location`, falling back to `server`. |
| `index` | `root + request path + index` | The request's `location`, falling back to `server`. |
| `error_page` | `root + error page path` | The `location` matching the **error page path**, falling back to `server`. |

Here, `+` means joining path components. A leading `/` in `error_page` or `upload_path` does not make it a path relative to the operating system's root: it is resolved under the selected `root`. Use a relative path for `index`, such as `index.html` or `sub/start.html`.

Examples with `root ./public`:

- `index start.html` and a request for `/guide/` → `./public/guide/start.html`;
- `upload_path uploads` → the `./public/uploads` directory;
- `error_page 404 /404.html` → `./public/404.html`, unless the matching `location` overrides the root.

The `.` and `..` path components are forbidden in `index`, `upload_path`, and `error_page` values. This restriction does not prohibit `root ./public`.

## `index` and `autoindex`: directory requests

`index` accepts one relative path, including subdirectories such as `sub/start.html`. Absolute values are accepted by the configuration parser but do not follow the intended path mapping: `/start.html` is resolved from the filesystem root and causes `403` for directory requests when it falls outside the configured `root`.

For an existing directory requested without a trailing `/`, the server first returns a `301` redirect to the path with `/`, preserving the query string. This redirect happens before the index check.

When a directory is requested with a trailing `/`, the server looks for the index file. If the file is missing and `autoindex on` is set, it generates an HTML directory listing. Directories appear first, followed by files; names are sorted within each group. Names starting with `.` are hidden from the listing.

If the index file is missing and `autoindex off` is set, the response is `403`. A directory or another unsuitable object at the index path also produces `403`.

## `methods`: allowed operations

Only uppercase `GET`, `HEAD`, `POST`, and `DELETE` are accepted, with no duplicates.

| Method | Behavior |
| --- | --- |
| `GET` | Retrieve a file, index page, directory listing, or CGI response. |
| `HEAD` | Retrieve response headers without a body. Must be explicitly allowed: `GET` alone does not enable it. |
| `POST` | Pass a body to an upload handler or CGI program. |
| `DELETE` | Delete a file or symbolic link at `root + request path`. Success returns `204`; a missing target returns `404`. Directory deletion is forbidden (`403`). |

DELETE is available for any file under the selected location's `root` when the method is allowed and the file handler is selected; it is not limited to uploads. Deleting a symbolic link removes the link itself, not its target.

A supported method that is disallowed for the route produces `405` with an `Allow` header. An unsupported method, such as `PUT` or `PATCH`, produces `501`.

Setting `methods POST` alone is not enough to handle a body: `upload_path` or a matching CGI extension is required. POST is removed from the allowed methods for the regular file handler.

`return` has its own execution order: it runs before the `methods` check. The methods list is still required in the configuration, but it does not restrict a response configured with `return`. The declared body size is checked before `return` runs.

## `client_max_body_size`: request body size

The value is a nonnegative integer number of bytes, optionally followed by a **lowercase** `k` or `m` suffix:

| Value | Size |
| --- | --- |
| `100` | 100 bytes. |
| `1k` | 1024 bytes. |
| `1m` | 1048576 bytes; the default. |
| `200m` | 209715200 bytes. |
| `0` | No configured limit: internally, the largest `size_t` value is used. The practical limit depends on memory and other resources. |

The suffixes `K`, `M`, and `g`, fractional values, and negative values are not supported. Overflow during conversion to bytes is rejected.

The server responds with `413` and closes the connection when the limit is exceeded; it may drain pending input before closing. A size declared in `Content-Length` is checked before the body is processed. Chunked body limits are checked on every route that creates a request handler, including static files, uploads, and CGI. Responses produced immediately, such as `return`, no matching location (`404`), or a rejected method (`405`), do not pass through that handler check. The limit applies to the request body, not to the configuration file or response size.

A route with `return` produces its response immediately without waiting for the entire body. The `body-size.conf` example therefore demonstrates limits using requests with a known `Content-Length`; use uploads or CGI to check chunked body limits.

## `upload_path`: file uploads

```nginx
location /uploads {
    methods GET POST DELETE;
    upload_path uploads;
    autoindex on;
}
```

With `root ./public`, a POST to `/uploads/hello.txt` creates `./public/uploads/hello.txt`. The upload directory is `root + upload_path`; the filename and any subdirectories come from the part of the request path after the `location` prefix.

The request body is saved as a sequence of bytes. There is no `multipart/form-data` parsing: use `curl --data-binary @file` to upload file contents, rather than `curl -F`.

The upload directory and any required subdirectories must already exist; a missing directory produces `500`. New files are created with mode `0600` (owner read/write), subject to the process umask. A successful upload returns `201` and a `Location` header containing the request path. Existing files are not overwritten: the response is `409`. A request without a filename also produces `409`. Incomplete uploads are removed if the connection is interrupted, the size limit is exceeded, or an error occurs.

`upload_path` controls writes through POST. GET and DELETE use `root + request path`. In the example, the `/uploads` URL and the `uploads` directory align, so an uploaded file can be retrieved and deleted using the same URL. With `location /upload` and `upload_path incoming`, writes go to `root/incoming`, while a request for `/upload/file.txt` looks for `root/upload/file.txt`.

## `cgi`: running a program

```nginx
location /cgi-bin {
    methods GET POST;
    cgi .py /usr/bin/python3;
    cgi .sh /bin/sh;
}
```

The first argument starts with a dot and identifies the extension. The second is an absolute path to an executable program. The program's existence and permission to execute are checked when processing a request; adjust the path for your system. A missing or non-executable interpreter produces `502`. The server does not explicitly reject a missing script as `404`: the interpreter handles it. For example, Python produces no valid CGI output for a missing script, resulting in `502`.

CGI is selected by the request path's extension within the selected `location`. The script is resolved as `root + request path`. Other extensions are served as regular files; CGI rules from other locations are not used.

The process runs with the script's directory as its working directory. The request body is passed through stdin, and variables such as `REQUEST_METHOD`, `QUERY_STRING`, `SCRIPT_NAME`, `CONTENT_TYPE`, and `REMOTE_ADDR` are passed through the environment. The CGI program must write headers, a blank line, and a body to stdout. Output is forwarded to the client as it arrives; `snake.py` in `public/cgi-bin/` demonstrates this behavior.

The CGI request body is collected in memory before the program starts, so `client_max_body_size` is especially relevant for these routes. If the CGI response has no `Content-Length`, its body is sent in chunks using `Transfer-Encoding: chunked`, except for `HEAD`, `204`, and `304`, whose bodies are suppressed.

## `return`: redirects and configured responses

Configured response behavior:

| Syntax | Result |
| --- | --- |
| `return <code> <target>;` for `301`, `302`, `303`, `307`, `308` | A redirect with `Location: <target>`. The target is required and must start with `/`, `http://`, or `https://`. |
| `return <code> <text>;` for any other allowed code | The specified status with the text as a `text/plain` body. |
| `return <code>;` for an allowed code in the `400–599` range | The configured `error_page` for that code, or the built-in error page if none is set. |
| `return <code>;` for any other allowed code except redirects | The specified status with an empty body. |

`return` and `error_page` accept statuses known to the server. `return` accepts the following final response codes; `error_page` accepts only the codes in the `400–599` range. This list comes from [HttpStatus](../src/http/include/http/HttpStatus.hpp) and `Http::Status::fromCode`, not from a fixed list in the subject:

```text
200, 201, 204, 301, 302, 303, 304, 307, 308,
400, 403, 404, 405, 408, 409, 413, 414, 417, 431,
500, 501, 502, 503, 504, 505
```

Examples of individual directives for separate locations:

```nginx
return 301 /;
return 302 https://example.com/;
return 200 pong;
return 404;
return 404 missing;
return 204;
```

`return 404;` uses the error page mechanism, while `return 404 missing;` provides custom text. `304` is not treated as a redirect and does not require `Location`. When sending the response, the body is suppressed for `HEAD`, `204`, and `304`, even if text is configured.

`417` is also returned by the server for an unsupported `Expect` value. Informational status `100` is generated for `100-continue` when appropriate, but is not accepted by these configuration directives.

## `error_page`: custom error pages

```nginx
error_page 404 /404.html;
error_page 403 405 /errors/denied.html;
```

This directive is allowed only in `server`. One file can be assigned to several codes, and the directive can be repeated for other codes. Supported error codes in the `400–599` range are accepted. The path must start with `/`.

The root is selected using the **error page path**, rather than the original request path. For example:

```nginx
server {
    listen 127.0.0.1:8080;
    root ./public;
    error_page 404 /errors/404.html;

    location /api {
        methods GET;
        root ./application;
    }

    location /errors {
        methods GET;
        root ./custom;
    }
}
```

For a `404` on `/api/missing`, the server reads `./custom/errors/404.html`. If `/errors` has no `root` of its own, it uses `./public/errors/404.html`. If no location matches the error page path, the server root is also used.

The page is read as a file: the client is not redirected, and the handler for `location /errors` is not executed. The original error code is preserved. If the file is missing or cannot be opened, the server uses a built-in HTML page with the same status.

## Configuration files in the repository

Each file directly inside `config/` is a standalone configuration for `./webserv <file>`. Most use port `8080`, so run them one at a time. The smaller configurations let you check individual features, while `demo.conf` combines them for a demonstration.

| File | Settings and purpose |
| --- | --- |
| [demo.conf](../config/demo.conf) | General demonstration: two servers, three addresses, the `demo.html` home page, a separate root for `/docs`, an index for `/guide`, a listing at `/files`, uploads at `/uploads`, CGI, redirects, `/ping`, and error pages. |
| [listen.conf](../config/listen.conf) | Two servers: the first on `127.0.0.1:8080` and `localhost:8081`, the second on `0.0.0.0:8082`. Demonstrates multiple `listen` directives and server selection by connection address. |
| [root.conf](../config/root.conf) | A shared `./public` root and a separate `./public/archive` root for `/docs`. Demonstrates overriding the root while retaining the request path. |
| [index.conf](../config/index.conf) | The default `index.html` and `start.html` for `/guide`. Demonstrates index selection, redirects to a trailing `/`, and rejection of a directory request without an index. |
| [methods.conf](../config/methods.conf) | GET on `/`, GET and HEAD on `/guide`, and GET and POST on `/readonly` without a POST handler. Demonstrates explicit HEAD permission, `405`/`501` responses, and the need for a POST handler. |
| [autoindex.conf](../config/autoindex.conf) | Directory listings enabled only at `/files`. Used to check nested directories, links to names containing spaces, and directories with listings disabled. |
| [body-size.conf](../config/body-size.conf) | A server limit of `1k`, a `/big` limit of `1m`, and a `/unlimited` limit of `0`. Demonstrates inheritance, overrides, and disabling the limit for requests with `Content-Length`. |
| [upload.conf](../config/upload.conf) | GET, POST, DELETE, and directory listings at `/uploads`, with writes to `./public/uploads`. Demonstrates file creation, retrieval, conflicts on repeated uploads, and deletion. |
| [cgi.conf](../config/cgi.conf) | `.py` through `/usr/bin/python3` and `.sh` through `/bin/sh` at `/cgi-bin`. `snake.py` demonstrates a streaming response; `cat.sh` demonstrates GET parameters and a POST body. |
| [return.conf](../config/return.conf) | `/old` → `301 /`, `/away` → an external `302`, `/gone` → `404` with an error page, `/ping` → `200 pong`, `/empty` → `204`. Demonstrates the different forms of `return`. |
| [error-page.conf](../config/error-page.conf) | `/404.html` for `404`, `/errors/denied.html` for `403` and `405`, and built-in pages for other errors. Used to check custom pages and fallback responses. |
| [42.test.conf](../config/42.test.conf) | Configuration for the external 42 tester: the `./YoupiBanane` root, the `youpi.bad_extension` index, POST at `/post_body` with a 100-byte limit and `return 200`, and `.bla` CGI through `/tmp/cgi_tester` at `/directory` with a `200m` limit. |

Run `config/42.test.sh /path/to/cgi_tester` from the repository root to prepare `YoupiBanane/` and install the CGI tester at `/tmp/cgi_tester`. Replace the argument with the path to your downloaded executable. The `200m` limit allows the 100 MB body used by the tester. With the committed server code, some tester checks fail: CGI through `cgi_tester` because `PATH_INFO` is empty, a directory without an index because the server returns `403` while the tester expects `404`, and the chunked body size limit at `/post_body`. See [tester setup and compatibility switches](testing.md#42-tester) for the three commented alternatives and the team's reasons for leaving them disabled.

### Intentionally invalid configurations

The `broken.*` directories are used to check that startup fails with a clear diagnostic. These files are passed to the server in the same way, but the expected result is an error message and a nonzero exit status.

| Directory | What the files check |
| --- | --- |
| [broken.syntax/](../config/broken.syntax/) | Text structure: missing block or directive names (`expected-*-name`), missing terminators or incomplete statements (`expected-terminator-*`), unclosed blocks (`unclosed-block-*`), an extra `}`, and excessive nesting. |
| [broken.validation/](../config/broken.validation/) | Schema rules: invalid block or directive contexts (`*-not-allowed-*`), unknown names (`unknown-*`), empty configurations, blocks without `{}` (`missing-body-*`), missing required directives (`missing-directive-*`), unexpected directive bodies (`unexpected-body-*`), argument counts (`wrong-argument-count-*`), duplicates (`duplicate-*`), and conflicts (`conflicting-*`). |
| [broken.decoding/](../config/broken.decoding/) | Argument values: `listen`, path, and CGI formats, `return` codes and targets (`invalid-format-*`), unsupported methods, statuses, and `autoindex` values (`unsupported-value-*`), duplicate methods and error codes, missing redirect targets, `.`/`..` path components, and body size overflow. |

Each filename identifies the error being checked. For example, `conflicting-behavior-return-upload.conf` combines incompatible handlers, while `invalid-format-body-size-unit.conf` uses the unsupported suffix `1g`.

```sh
./webserv config/broken.decoding/empty-value-return-target.conf
```

This example contains `return 301;` without the required target and should fail with a configuration error.
