
../../install/lib/ivl/ivlpp -L \
-F"ivrlg2" \
-f"ivrlg" \
-p"ivrli" \
>tmp_pp

../../install/lib/ivl/ivl -C"ivrlh" -C"../../install/lib/ivl/pint.conf" tmp_pp
