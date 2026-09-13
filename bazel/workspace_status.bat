@echo off
REM Emitted into bazel-out/stable-status.txt when a build passes --stamp
REM (see --config=release in .bazelrc).  "stable" is the right key prefix:
REM a STABLE_ key only invalidates its consumers when the value itself
REM changes, so a rebuild at the same commit stays cached.
REM
REM Failures here must not fail the build -- a source archive with no .git
REM directory still has to compile -- so every branch falls back to a literal.

for /f %%i in ('git rev-parse --short HEAD 2^>NUL') do set REV=%%i
if "%REV%"=="" set REV=unknown

REM A binary built from edited sources is not the commit it names.
git diff --quiet HEAD 2>NUL
if errorlevel 1 set REV=%REV%-dirty

echo STABLE_RAINGRASP_COMMIT %REV%
