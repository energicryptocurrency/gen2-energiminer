SDIR=./isa
BUILD=./bin

ASFLAGS=-I$(SDIR)
AS=clrxasm

.PHONY: all
all: ellesmere tonga baffin fiji gfx900 gfx901
	@echo "Built nrghash kernel for all architectures."

.PHONY: clean
clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.bin

%.bin:
	$(eval ARCH:=$(shell echo "$*" | sed -n 's/\(^[A-Za-z0-9]*\).*/\1/p'))
	$(eval LWS:=$(shell echo "$*" | sed -n 's/.*lws\([0-9]\+\)/\1/p'))
	$(AS) $(SDIR)/GCN_nrghash.isa -g $(ARCH) -Dworksize=$(LWS) $(ASFLAGS) -o $(BUILD)/nrghash_$(ARCH)_lws$(LWS).bin

.PHONY: ellesmere
ellesmere: ellesmere_lws64.bin ellesmere_lws128.bin ellesmere_lws192.bin ellesmere_lws256.bin
	@echo "Built Ellesmere kernels..."

.PHONY: tonga
tonga: tonga_lws64.bin tonga_lws128.bin tonga_lws192.bin tonga_lws256.bin
	@echo "Built Tonga kernels..."

.PHONY: baffin
baffin: baffin_lws64.bin baffin_lws128.bin baffin_lws192.bin baffin_lws256.bin
	@echo "Built Baffin kernels..."

.PHONY: fiji
fiji: fiji_lws64.bin fiji_lws128.bin fiji_lws192.bin fiji_lws256.bin
	@echo "Built Fiji kernels..."

.PHONY: gfx900
gfx900: gfx900_lws64.bin gfx900_lws128.bin gfx900_lws192.bin gfx900_lws256.bin
	@echo "Built gfx900 kernels..."

.PHONY: gfx901
gfx901: gfx901_lws64.bin gfx901_lws128.bin gfx901_lws192.bin gfx901_lws256.bin
	@echo "Built gfx901 kernels..."
