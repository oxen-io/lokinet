# TODO plan for 0.8

These may not all be implemented, but are a tentative roadmap for things to work for in 0.8.  0.8
is really meant to be a refinement release rather than institute major underlying changes, with the
investment paying off by cleaning up the code to allow fast and more reliable future features.

## Installer build improvements

Better installers so we can "make windows" or "make macos" and out pops an installer.  Currently
installers are too manual and we need to automate this.  More specifically:
- CPack installer for windows (32 and 64-bit).  If possible figure out how to do a "single"
  installer that can install the right one.
- macOS build automation (i.e. up to code signing)
- lokinet GUI package for linux (debs)

## Clean up cruft.

There is some unused (non-code) junk that has built up in the repo: (.exe files, random patches,
etc.).


## Make `make format` less annoying

New `make format` rules.  Relax some things, decide on other sane rules that are helpful without
being annoyingly rigid.  Currently make format rules are far too rigid to the point of being
annoying.

## .ini file overhaul

Right now we have a hard-coded generated .ini file that only includes some settings.  We need to
overhaul this to include everything -- some options with initial settings, other options commented
out -- along with descriptions of each option.  We should do this "right": i.e. by having one single
set of specification of options that can do both parsing and config file generation rather than
duplicating options in parsing and options printed in the generated config file (because that will
inevitably diverge).

## Service node testing

We want to start implementing snode performance testing in 0.8.  A few initial ideas:
- Simple check: look for a gossipped RC within last 2 hours from a given router.
- Could passively report session stats and use "trigger" conditions (e.g. at least 10 session
  establish attempts in the last two hours and >= 75% of them failed)
- Active tests: Similar to the above, but also establishing sessions with random routers we wouldn't
  ordinary have a session with just for the purpose of testing them.

## Documentation of code
- get current codebase to document all methods (even if trivial -- though trivial methods can have
  trivial documentation, they should at least have *something*).
- once done, keep things documented (i.e. new code needs new docs to be accepted as a PR)
- also working on technical whitepaper in parallel (significant crossover, particularly with the
  larger classes/methods).

## Hive testing tool:
- continue development here and there as needed/beneficial.  It will be helped by documentation
  efforts, above, to devise Hive-based tests that will be useful (to test the described
  documentation rather than the current implementation).

## Refactoring:
- DHT code.  Two phases:
  - phase 1, while documenting, make notes about refactoring DHT
  - phase 2: implement DHT refactor.
- LokiMQ integration.
  - replace libabyss RPC interface
  - lokid communication
  - (experimental) handle job scheduling
  - (experimental) work incoming UDP polling into lokimq's poll loop (potentially replacing libuv)

## Android

There is a lot on this list already, but we may also take some strides towards getting a working
Android lokinet implementation.
