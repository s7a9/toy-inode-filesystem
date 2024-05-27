SUB_DIR := ./network ./bytepack ./step1 ./step2 

all:
	@for n in $(SUB_DIR); do $(MAKE) -C $$n -j || exit 1; done

clean:
	# @for n in $(SUB_DIR); do $(MAKE) -C $$n clean || exit 1; done
	rm -f ./bin/*.o
