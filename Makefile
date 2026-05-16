PROJECT := MHModern
POWERSHELL := powershell -NoProfile -ExecutionPolicy Bypass

.PHONY: all clean test
.NOTPARALLEL:

all:
	$(POWERSHELL) -File scripts/build.ps1 -Target dll

test:
	$(POWERSHELL) -File scripts/build.ps1 -Target test

clean:
	$(POWERSHELL) -File scripts/build.ps1 -Target clean
