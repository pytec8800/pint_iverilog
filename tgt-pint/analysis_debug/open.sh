#dot -Tpng always_cfg.dot -o always.png && eog always.png

for i in "$@"; do
  dot -Tpng ${i}.dot -o ${i}.png && eog ${i}.png &
done

#dot -Tpng cfg2.dot -o cfg2.png && eog cfg2.png &
#dot -Tpng cfg2.dot -o initial.png && eog initial.png
