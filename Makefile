all:
	gcc -o cmin cmin.c
	gcc -o test test.c

runjsmn:
	./cmin -i ./testsrc/jsmn/testcases/crash.json -m "AddressSanitizer: heap-buffer-overflow" -o reduced testsrc/jsmn/jsondump

runxml:
	./cmin -i ./testsrc/libxml2/testcases/crash.xml -m "SEGV on unknown address" -o reduced "testsrc/libxml2/xmllint --recover --postvalid -"

clean:
	rm cmin test
