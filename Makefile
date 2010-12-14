CFLAGS=-g -static -Iinclude/ -Wimplicit
LDFLAGS=-luuid

all: binary

libs:
				cc $(CFLAGS) -o lib/lib_smbios.o -c lib/lib_smbios.c
				cc $(CFLAGS) -o lib/lib_vpd.o -c lib/lib_vpd.c
				cc $(CFLAGS) -o lib/math.o -c lib/math.c

binary: libs
				cc $(CFLAGS) -o vpd.o -c vpd.c
				cc $(CFLAGS) -o vpd vpd.o lib/lib_vpd.o lib/lib_smbios.o lib/math.o -luuid

test: libs
				cc $(CFLAGS) -o lib/lib_vpd_test.o -c lib/lib_vpd_test.c
				cc $(CFLAGS) -o vpd_test lib/lib_vpd.o lib/lib_vpd_test.o
				./vpd_test

clean:
				rm -f *.o lib/*.o vpd vpd_test
