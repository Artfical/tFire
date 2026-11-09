# Security Policy

## Supported versions

tFire is released continuously: every commit on `main` becomes a build. Only the **latest build** gets security fixes. Use the built-in updater, or the newest build from the [Releases](https://github.com/Artfical/tFire/releases) page.

## Reporting a vulnerability

Please **do not open a public issue** for a security problem.

Report it privately, in either way:

- by email to **security@artfical.com**, or
- on GitHub: open the repository's **Security** tab and choose **Report a vulnerability**.

Say:

- what is affected (the browser engine, the network stack, the updater, the update server, an app, a package),
- how to reproduce it (steps, a program, or a packet capture), and the build you used,
- what an attacker could do with it.

You can expect an answer within a week, and updates as the fix progresses.

## Artfical Security Intel

Every vulnerability, once it is fixed, is published in **Artfical Security Intel**: what it was, which builds were affected, which build fixes it, and credit to whoever reported it (if they wish). Nothing is kept quiet.

## What is in scope

- The browser engine: memory safety, a page reading or changing another's memory or files, denial of service by an unprivileged page.
- The network stack: TLS certificate verification, cookie handling, HSTS, CSP, CSP violations.
- The updater: signature verification, downgrade prevention, the update server's integrity.
- The update server: its keys, its database, the packages it serves.
