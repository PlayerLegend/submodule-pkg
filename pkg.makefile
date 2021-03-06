bin/pkg-install: src/log/log.o
bin/pkg-install: src/pkg/install.o
bin/pkg-install: src/pkg/install.util.o
bin/pkg-install: src/pkg/internal/mkdir.o
bin/pkg-install: src/pkg/root.o
bin/pkg-install: src/table/table.o
bin/pkg-install: src/window/alloc.o
bin/pkg-install: src/convert/source.o
bin/pkg-install: src/convert/fd/sink.o
bin/pkg-install: src/convert/sink.o
bin/pkg-install: src/convert/fd/source.o
bin/pkg-install: src/convert/getline.o
bin/pkg-install: src/window/string.o
bin/pkg-install: src/window/printf.o
bin/pkg-install: src/window/printf_append.o
bin/pkg-install: src/lang/tokenizer/tokenizer.o
bin/pkg-install: src/lang/tree/tree.o
bin/pkg-install: src/immutable/immutable.o
bin/pkg-install: src/range/string_init.o
bin/pkg-install: src/range/strstr.o
bin/pkg-install: src/window/vprintf.o
bin/pkg-install: src/window/vprintf_append.o
bin/pkg-install: src/range/strchr.o
bin/pkg-install: src/range/range_strstr_string.o
bin/pkg-install: src/window/path.o
bin/pkg-install: src/tar/read.o
bin/pkg-pack: src/log/log.o
bin/pkg-pack: src/pkg/pack.o
bin/pkg-pack: src/pkg/pack.util.o
bin/pkg-pack: src/tar/write.o
bin/pkg-pack: src/window/alloc.o
bin/pkg-pack: src/window/string.o
bin/pkg-pack: src/window/path.o
bin/pkg-pack: src/range/string_init.o
bin/pkg-pack: src/convert/fd/sink.o
bin/pkg-pack: src/convert/fd/source.o
bin/pkg-pack: src/convert/duplex.o
bin/pkg-pack: src/convert/sink.o
bin/pkg-pack: src/convert/source.o

C_PROGRAMS += bin/pkg-install
C_PROGRAMS += bin/pkg-pack

pkg-utils: bin/pkg-install
pkg-utils: bin/pkg-pack

#utils: pkg-utils
