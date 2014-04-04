#!/bin/sh
make test > output.out
grep TSO output.out > tso.out
grep HB output.out > hb.out
perl -p -i -w -e 's/TSO//g' tso.out
perl -p -i -w -e 's/HB//g' hb.out
diff tso.out hb.out
