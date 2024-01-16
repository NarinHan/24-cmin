all:
	gcc -o cmin cmin.c

test:
	gcc -o test test.c

runjsmn:
	./cmin -i testsrc/jsmn/testcases/crash.json -m “AddressSanitizer: heap-buffer-overflow” -o reduced testsrc/jsmn/jsondump

clean:
	rm cmin test
