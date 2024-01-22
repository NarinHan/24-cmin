all:
	gcc -o cmin cmin.c

cminafl:
	$(CC) $(LDFLAGS) $^ -o $@

runjsmn:
	./cmin -i ./testsrc/jsmn/testcases/crash.json -m "AddressSanitizer: heap-buffer-overflow" -o reduced testsrc/jsmn/jsondump

runxml:
	./cmin -i ./testsrc/libxml2/testcases/crash.xml -m "SEGV on unknown address" -o reduced "testsrc/libxml2/xmllint --recover --postvalid -"

runpng:
	./cmin -i ./testsrc/libpng/crash.png -m "MemorySanitizer: use-of-uninitialized-value" -o reduced "testsrc/libpng/libpng/test_pngfix"

clean:
	rm cmin cminafl
