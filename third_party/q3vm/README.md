# q3vm (vendored)

Embedded Quake III Arena bytecode interpreter, vendored to run compiled `.qvm`
files directly in this editor (no external host process required).

* Upstream: <https://github.com/jnz/q3vm>
* Author: Jan Zwiener (jan@zwiener.org)
* License: MIT — see `LICENSE`
* Vendored files: `vm.c`, `vm.h`

Only `vm.c`/`vm.h` are compiled into this project. The upstream `lcc`, `q3asm`,
examples and standalone `main.c` are **not** included.
