CC = gcc
COPTS = -std=gnu11 -Wall -Wextra -O2 -march=native -flto

NANOPB_COMMIT = 1c987eb09d70f80a2752ede407e0f4060375e8de
NANOPB_URL = https://github.com/nanopb/nanopb/archive/$(NANOPB_COMMIT).zip
NANOPB_SHA = 9742ad36a41e878b7cf80048b58470f3e7650852bf6c2f338b497dbb702ef121

NANOPB_SRCS = pb_common.c pb_decode.c pb_encode.c
NANOPB_HDRS = pb_common.h pb_decode.h pb_encode.h pb.h
NANOPB_GEN = generator/protoc-gen-nanopb
NANOPB_PROTOS = generator/proto/nanopb.proto generator/proto/plugin.proto
NANOPB_UNZIP = $(NANOPB_SRCS) $(NANOPB_HDRS) $(NANOPB_GEN) $(NANOPB_PROTOS)

NANOPB_PY_PROTOS = generator/proto/nanopb_pb2.py generator/proto/plugin_pb2.py

.PHONY : all clean test rdoc

all: build/gearlance build/cranklance build/gearlanced build/cranklanced build/genelance build/chainlance build/torquelance build/torquelance-compile build/wrenchlance-left build/wrenchlance-right zhill/geartalk.pb.rb

build/.dir:
	mkdir -p build
	touch $@

# Currently active lances:

GEARLANCE_SRCS = gearlance.c common.c parser.c
GEARLANCE_DEPS = gearlance.h common.h parser.h

GEARLANCED_SRCS = gearlanced.c $(GEARLANCE_SRCS) build/geartalk.pb.c $(foreach f,$(NANOPB_SRCS),build/nanopb-$(NANOPB_COMMIT)/$(f))
GEARLANCED_DEPS = $(GEARLANCE_DEPS) build/geartalk.pb.h $(foreach f,$(NANOPB_HDRS),build/nanopb-$(NANOPB_COMMIT)/$(f))

build/gearlance: $(GEARLANCE_SRCS) $(GEARLANCE_DEPS) build/.dir
	$(CC) $(COPTS) -o $@ $(GEARLANCE_SRCS)

build/cranklance: $(GEARLANCE_SRCS) $(GEARLANCE_DEPS) build/.dir
	$(CC) $(COPTS) -DCRANK_IT=1 -o $@ $(GEARLANCE_SRCS)

build/gearlanced: $(GEARLANCED_SRCS) $(GEARLANCED_DEPS)
	$(CC) $(COPTS) -DNO_MAIN=1 -DPARSE_STDIN=1 -DPB_FIELD_16BIT=1 -Ibuild -Ibuild/nanopb-$(NANOPB_COMMIT) -o $@ $(GEARLANCED_SRCS)

build/cranklanced: $(GEARLANCED_SRCS) $(GEARLANCED_DEPS)
	$(CC) $(COPTS) -DNO_MAIN=1 -DPARSE_STDIN=1 -DPB_FIELD_16BIT=1 -Ibuild -Ibuild/nanopb-$(NANOPB_COMMIT) -DCRANK_IT=1 -o $@ $(GEARLANCED_SRCS)

build/genelance: genelance.c $(GEARLANCE_SRCS) $(GEARLANCE_DEPS)
	$(CC) $(COPTS) -DNO_MAIN=1 -DPARSE_NEWLINE_AS_EOF=1 genelance.c $(GEARLANCE_SRCS)

# Protobuf generation for the GearTalk™ protocol:

build/nanopb-$(NANOPB_COMMIT).zip: build/.dir
	wget -O $@.UNVERIFIED $(NANOPB_URL)
	if echo "$(NANOPB_SHA) $@.UNVERIFIED" | sha256sum -c; then mv $@.UNVERIFIED $@; fi

.PRECIOUS : $(foreach f,$(NANOPB_UNZIP),build/nanopb-$(NANOPB_COMMIT)/$(f))
$(foreach f,$(NANOPB_UNZIP),build/nanopb-%/$(f)): build/nanopb-%.zip
	unzip -o -DD $< -d build

build/nanopb-$(NANOPB_COMMIT)/generator/proto/%_pb2.py: build/nanopb-$(NANOPB_COMMIT)/generator/proto/%.proto
	protoc -Ibuild/nanopb-$(NANOPB_COMMIT)/generator/proto --python_out=build/nanopb-$(NANOPB_COMMIT)/generator/proto $<

build/%.pb.c build/%.pb.h: %.proto %.options $(foreach f,$(NANOPB_GEN) $(NANOPB_PY_PROTOS),build/nanopb-$(NANOPB_COMMIT)/$(f))
	protoc --plugin=protoc-gen-nanopb=build/nanopb-$(NANOPB_COMMIT)/$(NANOPB_GEN) --nanopb_out=build $<

zhill/geartalk.pb.rb: geartalk.proto
	rprotoc --out zhill geartalk.proto

# More or less outdated lances:

build/chainlance: chainlance.c build/.dir
	$(CC) $(COPTS) -o $@ chainlance.c

build/torquelance: torquelance.c torquelance-header.s common.c common.h parser.h build/.dir
	$(CC) $(COPTS) -o $@ torquelance.c torquelance-header.s common.c

build/torquelance-compile: torquelance-compile.c torquelance-ops.s common.c common.h parser.c parser.h build/.dir
	$(CC) $(COPTS) -DPARSER_EXTRAFIELDS='unsigned code;' -o $@ torquelance-compile.c torquelance-ops.s common.c parser.c

build/wrenchlance-left: wrenchlance-left.c wrenchlance-ops.s common.c common.h parser.c parser.h build/.dir
	$(CC) $(COPTS) -DPARSER_EXTRAFIELDS='unsigned code;' -o $@ wrenchlance-left.c wrenchlance-ops.s common.c parser.c

build/wrenchlance-right: wrenchlance-right.c common.c common.h parser.c parser.h build/.dir
	$(CC) $(COPTS) -o $@ wrenchlance-right.c common.c parser.c

# Phony targets:

clean:
	$(RM) -r build
	$(RM) zhill/geartalk.pb.rb

rdoc:
	rdoc -o rdoc zhillbot.rb zhill
