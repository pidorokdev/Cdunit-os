# Dunit OS HTTP And Browser Roadmap

Goal: make networking real enough that Dunit OS can fetch an HTTP page over
TCP and show it in a usable GUI browser. Do not fake terminal output. Every
completed item must be backed by QEMU testing or a small deterministic test.

## Current State

- [x] Dock and terminal have a Browser entry point.
- [x] Kernel has early Ethernet/ARP/IP/ICMP/UDP/TCP code in `kernel/net/`.
- [x] Kernel has a socket API shape in `kernel/net/socket.c`.
- [x] DNS packet building and parsing helpers exist in `kernel/net/dns.c`.
- [ ] Socket send/recv/connect are wired to the real TCP implementation.
- [ ] DNS sends UDP queries and receives UDP responses.
- [ ] `curl`, `wget`, `nslookup`, `ifconfig`, and Browser output are real.
- [ ] Browser has URL input, page loading, navigation, and HTML rendering.
- [ ] HTTPS/TLS works.

## Milestone 0 - Define The First Working Target

- [ ] First supported URL: `http://example.com/`.
- [ ] First local test URL: `http://10.0.2.2:8080/` from QEMU user networking.
- [ ] First supported browser content:
  - [ ] status line and headers;
  - [ ] `text/plain`;
  - [ ] very small `text/html`;
  - [ ] ASCII and UTF-8 text without shaping.
- [ ] Explicitly out of scope for first HTTP milestone:
  - [ ] HTTPS;
  - [ ] JavaScript;
  - [ ] CSS layout;
  - [ ] forms;
  - [ ] images;
  - [ ] cookies;
  - [ ] HTTP/2 and HTTP/3.

## Milestone 1 - QEMU Network Baseline

- [ ] Pick and document one canonical QEMU networking setup:
  - [ ] `-netdev user,id=net0`;
  - [ ] matching virtio/e1000 device;
  - [ ] host access via `10.0.2.2`;
  - [ ] guest IP strategy.
- [ ] Verify the selected NIC driver actually receives frames in x86_64 QEMU.
- [ ] Add `./build.sh gui-net` or extend `./build.sh gui` with networking flags.
- [ ] Add serial logs for:
  - [ ] NIC initialization;
  - [ ] MAC address;
  - [ ] IP address;
  - [ ] RX/TX packet counters;
  - [ ] ARP cache events;
  - [ ] TCP state changes.
- [ ] Add a terminal `netinfo` command that prints real network state.
- [ ] Stop printing fake `ifconfig`/`netstat` data once real state is available.

## Milestone 2 - Ethernet, ARP, And Routing

- [ ] Confirm byte order for all stored IPv4 addresses.
- [ ] Normalize IPv4 representation across network stack:
  - [ ] interface IP;
  - [ ] gateway IP;
  - [ ] DNS server IP;
  - [ ] ARP cache keys;
  - [ ] TCP connection endpoints.
- [ ] Implement ARP request send.
- [ ] Implement ARP reply receive and cache insert.
- [ ] Implement ARP cache lookup with expiration using `arch_timer_get_ms()`.
- [ ] Route outbound packets:
  - [ ] direct LAN target uses target MAC;
  - [ ] off-subnet target uses gateway MAC;
  - [ ] no ARP entry queues or fails predictably.
- [ ] Add terminal command:
  - [ ] `arp`;
  - [ ] `route`;
  - [ ] `ping <ip>`.
- [ ] Acceptance test:
  - [ ] `ping 10.0.2.2` sends ICMP echo;
  - [ ] ARP table shows gateway/host entry;
  - [ ] RX/TX counters increase.

## Milestone 3 - ICMP Smoke Test

- [ ] Implement ICMP echo request send with checksum.
- [ ] Implement ICMP echo reply receive and match by id/sequence.
- [ ] Add timeout handling.
- [ ] Make terminal `ping <ip>` print real success/failure.
- [ ] Use ping as the first network regression test before TCP work.

## Milestone 4 - UDP Receive Path

- [ ] Add UDP socket/port registry.
- [ ] Dispatch incoming UDP packets to registered port handlers.
- [ ] Implement minimal UDP receive queue.
- [ ] Implement `udp_sendto()` and `udp_recvfrom()` style internal APIs.
- [ ] Handle packet truncation and buffer limits explicitly.
- [ ] Acceptance test:
  - [ ] send UDP packet from guest to host;
  - [ ] receive UDP packet from host to guest;
  - [ ] log source IP, source port, destination port, and length.

## Milestone 5 - Real DNS Resolver

- [ ] Wire `dns_resolve()` to UDP port 53.
- [ ] Use QEMU DNS server `10.0.2.3` by default for user networking.
- [ ] Keep configurable DNS servers.
- [ ] Match DNS responses by transaction ID.
- [ ] Parse:
  - [ ] A records;
  - [ ] CNAME chains;
  - [ ] NXDOMAIN;
  - [ ] truncated responses as a clear unsupported case.
- [ ] Implement TTL-aware DNS cache using `arch_timer_get_ms()`.
- [ ] Replace fake `nslookup` terminal output with real resolver output.
- [ ] Acceptance test:
  - [ ] `nslookup example.com` returns a real A record;
  - [ ] second lookup hits cache;
  - [ ] failed domain returns a real error.

## Milestone 6 - TCP Correctness For HTTP

- [ ] Wire `socket_connect()` to `tcp_connect()`.
- [ ] Wire `socket_send()` to `tcp_send()`.
- [ ] Wire `socket_recv()` to `tcp_recv()`.
- [ ] Store protocol-specific TCP connection in `socket.sk`.
- [ ] Add a way to map TCP connection index back to a safe pointer.
- [ ] Implement TCP connect wait with timeout:
  - [ ] SYN sent;
  - [ ] SYN+ACK received;
  - [ ] ACK sent;
  - [ ] connection state becomes established.
- [ ] Fix TCP send semantics:
  - [ ] segment data within MSS;
  - [ ] track unacknowledged bytes;
  - [ ] retransmit on timeout;
  - [ ] handle peer RST.
- [ ] Fix TCP receive semantics:
  - [ ] buffer payload;
  - [ ] ACK received bytes;
  - [ ] support FIN after response body;
  - [ ] return EOF cleanly.
- [ ] Implement close path:
  - [ ] FIN;
  - [ ] ACK;
  - [ ] timeout cleanup.
- [ ] Acceptance test:
  - [ ] connect to `10.0.2.2:8080`;
  - [ ] send raw bytes;
  - [ ] receive raw bytes;
  - [ ] close without leaking connection slots.

## Milestone 7 - HTTP Client Library

- [ ] Add `kernel/net/http.c` and `kernel/include/net/http.h`.
- [ ] Implement URL parser:
  - [ ] scheme;
  - [ ] host;
  - [ ] optional port;
  - [ ] path;
  - [ ] query;
  - [ ] reject unsupported schemes clearly.
- [ ] Implement HTTP/1.1 GET:
  - [ ] `Host`;
  - [ ] `User-Agent: DunitOS/0`;
  - [ ] `Accept`;
  - [ ] `Connection: close`.
- [ ] Implement response parser:
  - [ ] status code;
  - [ ] reason phrase;
  - [ ] headers;
  - [ ] body start.
- [ ] Support body modes:
  - [ ] connection-close body;
  - [ ] `Content-Length`;
  - [ ] basic `Transfer-Encoding: chunked`.
- [ ] Add limits:
  - [ ] max URL length;
  - [ ] max headers size;
  - [ ] max body size for first browser version.
- [ ] Return structured errors:
  - [ ] DNS failure;
  - [ ] connect timeout;
  - [ ] send failure;
  - [ ] invalid HTTP response;
  - [ ] body too large.
- [ ] Acceptance test:
  - [ ] `curl http://10.0.2.2:8080/` prints real response;
  - [ ] `curl http://example.com/` prints real response;
  - [ ] non-existent host gives DNS error.

## Milestone 8 - Terminal Tools

- [ ] Replace fake `curl` and `wget` output with real HTTP client calls.
- [ ] Add `curl -I <url>` for headers only.
- [ ] Add `curl -o <path> <url>` to save response body into VFS.
- [ ] Add `httpget <url>` as a simpler debug command if `curl` grows too much.
- [ ] Add progress output:
  - [ ] resolving;
  - [ ] connecting;
  - [ ] sending request;
  - [ ] receiving bytes;
  - [ ] done/error.
- [ ] Add serial log categories for DNS/TCP/HTTP.

## Milestone 9 - Browser Window Shell

- [ ] Replace placeholder Browser window with a real browser state object.
- [ ] Add browser window layout:
  - [ ] toolbar;
  - [ ] back button;
  - [ ] forward button;
  - [ ] reload button;
  - [ ] URL input field;
  - [ ] status text;
  - [ ] scrollable content area.
- [ ] Add keyboard focus for URL input.
- [ ] Pressing Enter in URL input starts load.
- [ ] Browser dock icon opens the real browser.
- [ ] Terminal `browser <url>` opens browser and starts load.
- [ ] Show loading/error states in the browser window.
- [ ] Never freeze the entire GUI while waiting for network.

## Milestone 10 - Browser Loading Model

- [ ] Decide first implementation style:
  - [ ] synchronous load with periodic `net_poll()` and redraw;
  - [ ] or background task/event-driven load.
- [ ] Add cancellation when user presses Stop or closes window.
- [ ] Add timeout handling per phase:
  - [ ] DNS;
  - [ ] TCP connect;
  - [ ] request send;
  - [ ] first byte;
  - [ ] whole response.
- [ ] Add browser history:
  - [ ] current URL;
  - [ ] back stack;
  - [ ] forward stack.
- [ ] Add simple cache later, not in first version.

## Milestone 11 - HTML Tokenizer

- [ ] Add `kernel/browser/html_tokenizer.c` or equivalent module.
- [ ] Tokenize:
  - [ ] text;
  - [ ] start tags;
  - [ ] end tags;
  - [ ] attributes;
  - [ ] comments as ignored tokens;
  - [ ] entities for `&amp;`, `&lt;`, `&gt;`, `&quot;`, `&#...;`.
- [ ] Tolerate malformed HTML without panicking.
- [ ] Add a small local test list of HTML snippets.
- [ ] Keep tokenizer independent from network code.

## Milestone 12 - Minimal HTML Renderer

- [ ] Build a simple document model or direct flow layout.
- [ ] Render supported elements:
  - [ ] text nodes;
  - [ ] `html`;
  - [ ] `head` ignored;
  - [ ] `title`;
  - [ ] `body`;
  - [ ] `h1`-`h3`;
  - [ ] `p`;
  - [ ] `br`;
  - [ ] `pre`;
  - [ ] `a href`;
  - [ ] `ul`, `ol`, `li`;
  - [ ] `strong`, `b`;
  - [ ] `em`, `i`;
  - [ ] `code`.
- [ ] Implement text wrapping within content width.
- [ ] Implement vertical scrolling.
- [ ] Draw links in a distinct color and underline or hover state.
- [ ] Clicking an `http://` link navigates.
- [ ] Clicking unsupported links shows an error.
- [ ] Acceptance test:
  - [ ] local HTML page with headings, paragraphs, links, and lists renders readably.

## Milestone 13 - Browser Files And Local Pages

- [ ] Support `file:///path` for local HTML files.
- [ ] Add `/Desktop/start.html` or `/Documents/start.html` as a browser homepage.
- [ ] Add `about:dunit` page generated by the browser.
- [ ] Add `about:network` page with real NIC/DNS/TCP status.
- [ ] Use local pages for deterministic GUI regression testing.

## Milestone 14 - Images In Pages

- [ ] Add image resource fetch for `<img src>`.
- [ ] Support local and HTTP image URLs.
- [ ] Decode:
  - [ ] BMP first;
  - [ ] PNG second;
  - [ ] JPEG third.
- [ ] Add image layout boxes with max width.
- [ ] Add placeholder for loading and failed images.
- [ ] Cache decoded images per page.
- [ ] Enforce memory limits so a page cannot exhaust kernel heap.

## Milestone 15 - CSS Subset Later

- [ ] Start with browser default styles, no CSS.
- [ ] Later parse inline `style` for:
  - [ ] color;
  - [ ] background-color;
  - [ ] font-size;
  - [ ] margin;
  - [ ] padding.
- [ ] Later parse simple `<style>` blocks.
- [ ] Do not attempt full CSS until layout is stable.

## Milestone 16 - HTTPS/TLS

- [ ] Keep HTTPS unsupported until HTTP/1.1 is solid.
- [ ] Decide TLS approach:
  - [ ] port a small TLS library;
  - [ ] or write a very small educational TLS client for limited ciphers.
- [ ] Required TLS features for real web:
  - [ ] TLS 1.2 or TLS 1.3;
  - [ ] SNI;
  - [ ] certificate parsing;
  - [ ] root CA store or pinned certificates;
  - [ ] AES-GCM or ChaCha20-Poly1305;
  - [ ] SHA-256;
  - [ ] ECDHE.
- [ ] Add `https://` URL parsing only when TLS connect works.
- [ ] Add browser security error UI:
  - [ ] certificate invalid;
  - [ ] hostname mismatch;
  - [ ] expired certificate;
  - [ ] unsupported cipher.

## Milestone 17 - Robustness And Security

- [ ] Put hard limits on:
  - [ ] DNS name length;
  - [ ] redirects;
  - [ ] header size;
  - [ ] body size;
  - [ ] HTML token count;
  - [ ] layout boxes;
  - [ ] per-page image memory.
- [ ] Validate every network length before reading packet data.
- [ ] Reject malformed chunked responses safely.
- [ ] Add kernel heap allocation checks to all browser/network paths.
- [ ] Do not render or execute JavaScript in early versions.
- [ ] Keep remote content from writing arbitrary files unless user explicitly saves.

## Milestone 18 - Testing Matrix

- [ ] Host-side test server:
  - [ ] static text response;
  - [ ] small HTML page;
  - [ ] chunked response;
  - [ ] delayed response;
  - [ ] 404 response;
  - [ ] malformed response.
- [ ] Guest tests:
  - [ ] ping host;
  - [ ] DNS resolve;
  - [ ] TCP connect;
  - [ ] HTTP GET;
  - [ ] browser render local page;
  - [ ] browser render host-served page.
- [ ] Regression command:
  - [ ] `./build.sh net-test`;
  - [ ] launches host HTTP server;
  - [ ] boots QEMU;
  - [ ] checks serial output for pass/fail markers.

## Suggested File Layout

- [ ] `kernel/include/net/http.h`
- [ ] `kernel/net/http.c`
- [ ] `kernel/include/browser/browser.h`
- [ ] `kernel/browser/url.c`
- [ ] `kernel/browser/html_tokenizer.c`
- [ ] `kernel/browser/html_layout.c`
- [ ] `kernel/browser/browser_window.c`
- [ ] `kernel/browser/browser_tests.c`
- [ ] `docs/networking.md` later, once behavior is real.

## Definition Of Done: Real HTTP

- [ ] `./build.sh build` completes.
- [ ] `./build.sh gui-net` or documented equivalent boots with NIC enabled.
- [ ] `ping 10.0.2.2` works from Dunit OS.
- [ ] `nslookup example.com` performs a real DNS query.
- [ ] `curl http://10.0.2.2:8080/` returns bytes from a host HTTP server.
- [ ] `curl http://example.com/` returns a real HTTP response.
- [ ] Failed DNS/connect/HTTP responses produce readable errors.
- [ ] No fake network success output remains in terminal commands.

## Definition Of Done: Real Browser V1

- [ ] Browser window has toolbar and editable URL field.
- [ ] Browser can open `file:///...` local HTML pages.
- [ ] Browser can open `http://10.0.2.2:8080/`.
- [ ] Browser can open `http://example.com/`.
- [ ] Browser renders text, headings, paragraphs, lists, and links.
- [ ] Browser supports vertical scrolling.
- [ ] Clicking a rendered `http://` link navigates.
- [ ] Browser shows loading, error, and status messages.
- [ ] Browser does not freeze the whole GUI during normal page load.
- [ ] Memory limits prevent huge pages from crashing the system.
