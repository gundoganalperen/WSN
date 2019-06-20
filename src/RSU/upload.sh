device0="/dev/ttyUSB0"
  

for i in {1..10}; do
  while [[ ! -e  "$device0" ]]; do
	echo "Insert Mote! id=$i"
	sleep 3
  done
  command="make TARGET=zoul BOARD=remote-revb NODEID=0x0$i c2x_v0.upload"
  echo "device found, id $i, MAKE"
  $command

  while [[ -e  "$device0" ]]; do
	echo "Remove Mote! id=$i"
	sleep 3
  done
done



#if [[ -e  "$device0" ]]
#then
#  echo "DEV found"
#elif [[ ! -e  "$device0" ]]
#then
#  echo "NOT FOUND"
#fi
