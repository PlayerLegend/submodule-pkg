bin/pkg-install: src/log/log.o
bin/pkg-install: src/pkg/install.o
bin/pkg-install: src/pkg/install.util.o
bin/pkg-install: src/pkg/root.o
bin/pkg-install: src/table2/table.o
bin/pkg-install: src/tar/read.o
bin/pkg-pack: src/log/log.o
bin/pkg-pack: src/dzip/deflate.o
bin/pkg-pack: src/pkg/pkg-pack.o
bin/pkg-pack: src/pkg/pkg-pack.util.o
bin/pkg-pack: src/tar/write.o
bin/pkg-pack: src/vluint/vluint.o

C_PROGRAMS += bin/pkg-install
C_PROGRAMS += bin/pkg-pack

pkg-utils: bin/pkg-install
pkg-utils: bin/pkg-pack

#utils: pkg-utils
